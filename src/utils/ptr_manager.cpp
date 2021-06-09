#include "ptr_manager.hpp"
#include "../gdal_dataset.hpp"
#include "../gdal_group.hpp"
#include "../gdal_mdarray.hpp"
#include "../gdal_dimension.hpp"
#include "../gdal_attribute.hpp"
#include "../gdal_layer.hpp"
#include "../gdal_rasterband.hpp"

#include <sstream>
#include <thread>

// Here used to be dragons, but now there is a shopping mall
//
// This is the object store, a singleton
//
// It serves 2 purposes:
//
// First, it keeps track of created objects so that they can be reused
// The point of this mechanism is that it returns a reference to the same object
// for two successive calls of `ds.bands.get(1)` for example
// For this use, the V8 objects are indexed with the pointer to the GDAL
// base object
// uids won't work for this use
//
// Second, it is allocated entirely outside of the V8 memory management and the GC
// Thus, it is accessible from the worker threads
// The async locks and the I/O queues live here
// For this use, the V8 objects are indexed with numeric uids
// ptrs won't be safe for this use

namespace node_gdal {

void uv_sem_deleter::operator()(uv_sem_t *p) {
  uv_sem_destroy(p);
  delete p;
}

ObjectStore::ObjectStore()
  : uid(1),
    uidDrivers(),
    uidLayers(),
    uidBands(),
    uidDatasets(),
    uidSpatialRefs(),
    ptrDrivers(),
    ptrLayers(),
    ptrBands(),
    ptrDatasets(),
    ptrSpatialRefs()
#if GDAL_VERSION_MAJOR > 3 || (GDAL_VERSION_MAJOR == 3 && GDAL_VERSION_MINOR >= 1)
    ,
    uidGroups(),
    uidArrays(),
    uidDimensions(),
    uidAttributes(),
    ptrGroups(),
    ptrArrays(),
    ptrDimensions(),
    ptrAttributes()
#endif
{
  uv_mutex_init_recursive(&master_lock);
}

ObjectStore::~ObjectStore() {
}

bool ObjectStore::isAlive(long uid) {
  if (uid == 0) return true;
  return uidBands.count(uid) > 0 || uidLayers.count(uid) > 0 || uidDatasets.count(uid) > 0
#if GDAL_VERSION_MAJOR > 3 || (GDAL_VERSION_MAJOR == 3 && GDAL_VERSION_MINOR >= 1)
    || uidGroups.count(uid) > 0 || uidArrays.count(uid) > 0 || uidDimensions.count(uid) > 0 ||
    uidAttributes.count(uid) > 0
#endif
    ;
}

shared_ptr<uv_sem_t> ObjectStore::tryLockDataset(long uid) {
  while (true) {
    lock();
    auto parent = uidDatasets.find(uid);
    if (parent != uidDatasets.end()) {
      int r = uv_sem_trywait(parent->second->async_lock.get());
      unlock();
      if (r == 0) return parent->second->async_lock;
    } else {
      unlock();
      throw "Parent Dataset object has already been destroyed";
    }
    std::this_thread::yield();
  }
}

vector<shared_ptr<uv_sem_t>> ObjectStore::tryLockDatasets(vector<long> uids) {
  // There is lots of copying around here but these vectors are never longer than 3 elements
  // Avoid deadlocks
  std::sort(uids.begin(), uids.end());
  // Eliminate dupes
  uids.erase(std::unique(uids.begin(), uids.end()), uids.end());
  while (true) {
    vector<shared_ptr<uv_sem_t>> locks;
    lock();
    for (long uid : uids) {
      if (!uid) continue;
      auto parent = uidDatasets.find(uid);
      if (parent == uidDatasets.end()) {
        unlock();
        throw "Parent Dataset object has already been destroyed";
      }
      locks.push_back(parent->second->async_lock);
    }
    vector<shared_ptr<uv_sem_t>> locked;
    int r = 0;
    for (shared_ptr<uv_sem_t> &async_lock : locks) {
      r = uv_sem_trywait(async_lock.get());
      if (r == 0) {
        locked.push_back(async_lock);
      } else {
        // We failed acquiring one of the locks =>
        // free all acquired locks and start a new cycle
        for (shared_ptr<uv_sem_t> &lock : locked) { uv_sem_post(lock.get()); }
        break;
      }
    }
    unlock();
    if (r == 0) return locks;
    std::this_thread::yield();
  }
}

template <> constexpr UidMap<GDALDriver *> &ObjectStore::uidMap() {
  return uidDrivers;
}
template <> constexpr PtrMap<GDALDriver *> &ObjectStore::ptrMap() {
  return ptrDrivers;
}
template <> constexpr UidMap<GDALDataset *> &ObjectStore::uidMap() {
  return uidDatasets;
}
template <> constexpr PtrMap<GDALDataset *> &ObjectStore::ptrMap() {
  return ptrDatasets;
}
template <> constexpr UidMap<OGRLayer *> &ObjectStore::uidMap() {
  return uidLayers;
}
template <> constexpr PtrMap<OGRLayer *> &ObjectStore::ptrMap() {
  return ptrLayers;
}
template <> constexpr UidMap<GDALRasterBand *> &ObjectStore::uidMap() {
  return uidBands;
}
template <> constexpr PtrMap<GDALRasterBand *> &ObjectStore::ptrMap() {
  return ptrBands;
}
template <> constexpr UidMap<OGRSpatialReference *> &ObjectStore::uidMap() {
  return uidSpatialRefs;
}
template <> constexpr PtrMap<OGRSpatialReference *> &ObjectStore::ptrMap() {
  return ptrSpatialRefs;
}
#if GDAL_VERSION_MAJOR > 3 || (GDAL_VERSION_MAJOR == 3 && GDAL_VERSION_MINOR >= 1)
template <> constexpr UidMap<shared_ptr<GDALGroup>> &ObjectStore::uidMap() {
  return uidGroups;
}
template <> constexpr PtrMap<shared_ptr<GDALGroup>> &ObjectStore::ptrMap() {
  return ptrGroups;
}
template <> constexpr UidMap<shared_ptr<GDALMDArray>> &ObjectStore::uidMap() {
  return uidArrays;
}
template <> constexpr PtrMap<shared_ptr<GDALMDArray>> &ObjectStore::ptrMap() {
  return ptrArrays;
}
template <> constexpr UidMap<shared_ptr<GDALDimension>> &ObjectStore::uidMap() {
  return uidDimensions;
}
template <> constexpr PtrMap<shared_ptr<GDALDimension>> &ObjectStore::ptrMap() {
  return ptrDimensions;
}
template <> constexpr UidMap<shared_ptr<GDALAttribute>> &ObjectStore::uidMap() {
  return uidAttributes;
}
template <> constexpr PtrMap<shared_ptr<GDALAttribute>> &ObjectStore::ptrMap() {
  return ptrAttributes;
}
#endif

template <typename GDALPTR> long ObjectStore::add(GDALPTR ptr, Local<Object> obj, long parent_uid) {
  lock();
  shared_ptr<ObjectStoreItem<GDALPTR>> item(new ObjectStoreItem<GDALPTR>);
  item->uid = uid++;
  if (parent_uid) {
    shared_ptr<ObjectStoreItem<GDALDataset *>> parent = uidDatasets[parent_uid];
    item->parent = parent;
    parent->children.push_back(item->uid);
  } else {
    item->parent = nullptr;
  }
  item->ptr = ptr;
  item->obj.Reset(obj);
  item->obj.SetWeak(item.get(), weakCallback<GDALPTR>, Nan::WeakCallbackType::kParameter);

  uidMap<GDALPTR>()[item->uid] = item;
  ptrMap<GDALPTR>()[ptr] = item;
  unlock();
  return item->uid;
}

long ObjectStore::add(OGRLayer *ptr, Local<Object> obj, long parent_uid, bool is_result_set) {
  long uid = ObjectStore::add<OGRLayer *>(ptr, obj, parent_uid);
  uidLayers[uid]->is_result_set = is_result_set;
  return uid;
}

long ObjectStore::add(GDALDataset *ptr, Local<Object> obj, long parent_uid) {
  long uid = ObjectStore::add<GDALDataset *>(ptr, obj, parent_uid);
  if (parent_uid == 0) {
    uidDatasets[uid]->async_lock = shared_ptr<uv_sem_t>(new uv_sem_t(), uv_sem_deleter());
    uv_sem_init(uidDatasets[uid]->async_lock.get(), 1);
  }
  return uid;
}

// Explicit instantiation:
// * allows calling object_store.add without <>
// * makes sure that this class template won't be accidentally instantiated with an unsupported type
template long ObjectStore::add(GDALDriver *, Local<Object>, long);
template long ObjectStore::add(GDALRasterBand *, Local<Object>, long);
template long ObjectStore::add(OGRSpatialReference *, Local<Object>, long);
template bool ObjectStore::has(GDALDriver *);
template bool ObjectStore::has(GDALDataset *);
template bool ObjectStore::has(OGRLayer *);
template bool ObjectStore::has(GDALRasterBand *);
template bool ObjectStore::has(OGRSpatialReference *);
template Local<Object> ObjectStore::get(GDALDriver *);
template Local<Object> ObjectStore::get(GDALDataset *);
template Local<Object> ObjectStore::get(OGRLayer *);
template Local<Object> ObjectStore::get(GDALRasterBand *);
template Local<Object> ObjectStore::get(OGRSpatialReference *);
#if GDAL_VERSION_MAJOR > 3 || (GDAL_VERSION_MAJOR == 3 && GDAL_VERSION_MINOR >= 1)
template long ObjectStore::add(shared_ptr<GDALAttribute>, Local<Object>, long);
template long ObjectStore::add(shared_ptr<GDALDimension>, Local<Object>, long);
template long ObjectStore::add(shared_ptr<GDALGroup>, Local<Object>, long);
template long ObjectStore::add(shared_ptr<GDALMDArray>, Local<Object>, long);
template bool ObjectStore::has(shared_ptr<GDALAttribute>);
template bool ObjectStore::has(shared_ptr<GDALDimension>);
template bool ObjectStore::has(shared_ptr<GDALGroup>);
template bool ObjectStore::has(shared_ptr<GDALMDArray>);
template Local<Object> ObjectStore::get(shared_ptr<GDALAttribute>);
template Local<Object> ObjectStore::get(shared_ptr<GDALDimension>);
template Local<Object> ObjectStore::get(shared_ptr<GDALGroup>);
template Local<Object> ObjectStore::get(shared_ptr<GDALMDArray>);
#endif

// Death by calling dispose from C++ code
template <> void ObjectStore::dispose(shared_ptr<ObjectStoreItem<GDALDataset *>> item) {
  lock();
  uv_sem_wait(item->async_lock.get());
  uidDatasets.erase(item->uid);
  uv_sem_post(item->async_lock.get());

  while (!item->children.empty()) { dispose(item->children.back()); }

  if (item->ptr) {
    ptrDatasets.erase(item->ptr);
    GDALClose(item->ptr);
  }
  item->obj.Reset();
  unlock();
}

template <typename GDALPTR> void ObjectStore::dispose(shared_ptr<ObjectStoreItem<GDALPTR>> item) {
  LOG("ObjectStore: Death by calling dispose from C++ [%p]", item->ptr);
  shared_ptr<uv_sem_t> async_lock = nullptr;
  try {
    async_lock = tryLockDataset(item->parent->uid);
  } catch (const char *) {};
  ptrMap<GDALPTR>().erase(item->ptr);
  uidMap<GDALPTR>().erase(item->uid);
  if (item->parent != nullptr) item->parent->children.remove(item->uid);
  if (async_lock != nullptr) uv_sem_post(async_lock.get());
  item->obj.Reset();
}

template <> void ObjectStore::dispose(shared_ptr<ObjectStoreItem<OGRLayer *>> item) {
  shared_ptr<uv_sem_t> async_lock = nullptr;
  try {
    async_lock = tryLockDataset(item->parent->uid);
  } catch (const char *) {};
  GDALDataset *parent_ds = item->parent->ptr;
  if (item->is_result_set) { parent_ds->ReleaseResultSet(item->ptr); }
  uidLayers.erase(item->uid);
  ptrLayers.erase(item->ptr);
  item->parent->children.remove(item->uid);
  if (async_lock != nullptr) uv_sem_post(async_lock.get());
  item->obj.Reset();
}

// Death by GC
template <typename GDALPTR>
void ObjectStore::weakCallback(const Nan::WeakCallbackInfo<ObjectStoreItem<GDALPTR>> &data) {
  ObjectStoreItem<GDALPTR> *item = (ObjectStoreItem<GDALPTR> *)data.GetParameter();
  LOG("ObjectStore: Death by GC [%p]", item->ptr);
  object_store.dispose(item->uid);
}

void ObjectStore::dispose(long uid) {
  lock();

  if (uidDatasets.count(uid))
    dispose(uidDatasets[uid]);
  else if (uidLayers.count(uid))
    dispose(uidLayers[uid]);
  else if (uidBands.count(uid))
    dispose(uidBands[uid]);
#if GDAL_VERSION_MAJOR > 3 || (GDAL_VERSION_MAJOR == 3 && GDAL_VERSION_MINOR >= 1)
  else if (uidGroups.count(uid))
    dispose(uidGroups[uid]);
  else if (uidArrays.count(uid))
    dispose(uidArrays[uid]);
  else if (uidDimensions.count(uid))
    dispose(uidDimensions[uid]);
  else if (uidAttributes.count(uid))
    dispose(uidAttributes[uid]);
#endif
  unlock();
}

} // namespace node_gdal