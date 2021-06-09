
#ifndef __OBJ_STORE_H__
#define __OBJ_STORE_H__

// node
#include <node.h>

// nan
#include "../nan-wrapper.h"

// gdal
#include <gdal_priv.h>

// ogr
#include <ogrsf_frmts.h>

#include <list>
#include <map>

using namespace v8;
using namespace std;

namespace node_gdal {

template <typename GDALPTR> struct ObjectStoreItem {
  long uid;
  shared_ptr<ObjectStoreItem<GDALDataset *>> parent;
  GDALPTR ptr;
  Nan::Persistent<v8::Object> obj;
};

template <> struct ObjectStoreItem<OGRLayer *> {
  long uid;
  shared_ptr<ObjectStoreItem<GDALDataset *>> parent;
  OGRLayer *ptr;
  bool is_result_set;
  Nan::Persistent<v8::Object> obj;
};

template <> struct ObjectStoreItem<GDALDataset *> {
  long uid;
  shared_ptr<ObjectStoreItem<GDALDataset *>> parent;
  list<long> children;
  GDALDataset *ptr;
  shared_ptr<uv_sem_t> async_lock;
  Nan::Persistent<v8::Object> obj;
};

struct uv_sem_deleter {
  void operator()(uv_sem_t *p);
};

// A class for cleaning up GDAL objects that depend on open datasets

// Async lock semantics:
//
// * There is one global master lock
//
// * There is one async lock per dataset
//
// * All operations on the ObjectStore should acquire the master_lock
// - This implicit in all cases except isAlive()
// - The caller should explicitly lock isAlive()
//
// * All objects carry the dataset uid
//
// * All GDAL operations on an object require locking the parent dataset
// - This is best accomplished though tr.tryLockDataset
//
// * Deadlock avoidance strategy
// - One should never lock the master lock while holding an async_lock
// - Multiple datasets are to be locked with .tryLockDataset which sorts locks
//

template <typename GDALPTR> using UidMap = map<long, shared_ptr<ObjectStoreItem<GDALPTR>>>;
template <typename GDALPTR> using PtrMap = map<GDALPTR, shared_ptr<ObjectStoreItem<GDALPTR>>>;
class ObjectStore {
    public:
  template <typename GDALPTR> long add(GDALPTR ptr, Local<Object> obj, long parent_uid);
  long add(OGRLayer *ptr, Local<Object> obj, long parent_uid, bool is_result_set);
  long add(GDALDataset *ptr, Local<Object> obj, long parent_uid);

  void dispose(long uid);
  bool isAlive(long uid);
  shared_ptr<uv_sem_t> tryLockDataset(long uid);
  vector<shared_ptr<uv_sem_t>> tryLockDatasets(vector<long> uids);
  inline void lock() {
    uv_mutex_unlock(&master_lock);
  }
  inline void unlock() {
    uv_mutex_lock(&master_lock);
  }

  template <typename GDALPTR> static void weakCallback(const Nan::WeakCallbackInfo<ObjectStoreItem<GDALPTR>> &data);

  template <typename GDALPTR> inline bool has(GDALPTR ptr) {
    return ptrMap<GDALPTR>().count(ptr) > 0;
  }
  template <typename GDALPTR> inline Local<Object> get(GDALPTR ptr) {
    Nan::EscapableHandleScope scope;
    return scope.Escape(Nan::New(ptrMap<GDALPTR>()[ptr]->obj));
  }

  ObjectStore();
  ~ObjectStore();

    private:
  long uid;
  uv_mutex_t master_lock;
  template <typename GDALPTR> void dispose(shared_ptr<ObjectStoreItem<GDALPTR>> item);

  // this is a manual implementation of member overloading which does not exist in C++
  template <typename GDALPTR> constexpr UidMap<GDALPTR> &uidMap();
  template <typename GDALPTR> constexpr PtrMap<GDALPTR> &ptrMap();

  UidMap<GDALDriver *> uidDrivers;
  UidMap<OGRLayer *> uidLayers;
  UidMap<GDALRasterBand *> uidBands;
  UidMap<GDALDataset *> uidDatasets;
  UidMap<OGRSpatialReference *> uidSpatialRefs;
  PtrMap<GDALDriver *> ptrDrivers;
  PtrMap<OGRLayer *> ptrLayers;
  PtrMap<GDALRasterBand *> ptrBands;
  PtrMap<GDALDataset *> ptrDatasets;
  PtrMap<OGRSpatialReference *> ptrSpatialRefs;
#if GDAL_VERSION_MAJOR > 3 || (GDAL_VERSION_MAJOR == 3 && GDAL_VERSION_MINOR >= 1)
  UidMap<shared_ptr<GDALGroup>> uidGroups;
  UidMap<shared_ptr<GDALMDArray>> uidArrays;
  UidMap<shared_ptr<GDALDimension>> uidDimensions;
  UidMap<shared_ptr<GDALAttribute>> uidAttributes;
  PtrMap<shared_ptr<GDALGroup>> ptrGroups;
  PtrMap<shared_ptr<GDALMDArray>> ptrArrays;
  PtrMap<shared_ptr<GDALDimension>> ptrDimensions;
  PtrMap<shared_ptr<GDALAttribute>> ptrAttributes;
#endif
};

} // namespace node_gdal

#endif
