
#ifndef __OBJ_STORE_H__
#define __OBJ_STORE_H__

// node
#include <napi.h>
#include <uv.h>

// nan
#include <napi.h>

// gdal
#include <gdal_priv.h>

// ogr
#include <ogrsf_frmts.h>

#include <list>
#include <map>

using namespace Napi;
using namespace std;

namespace node_gdal {

typedef shared_ptr<uv_sem_t> AsyncLock;

template <typename GDALPTR> struct ObjectStoreItem {
  long uid;
  Napi::ObjectReference &obj;
  GDALPTR ptr;
  shared_ptr<ObjectStoreItem<GDALDataset *>> parent;
  ObjectStoreItem(Napi::ObjectReference &obj);
};

template <> struct ObjectStoreItem<OGRLayer *> {
  long uid;
  Napi::ObjectReference &obj;
  OGRLayer *ptr;
  shared_ptr<ObjectStoreItem<GDALDataset *>> parent;
  bool is_result_set;
  ObjectStoreItem(Napi::ObjectReference &obj);
};

template <> struct ObjectStoreItem<GDALDataset *> {
  long uid;
  Napi::ObjectReference &obj;
  GDALDataset *ptr;
  shared_ptr<ObjectStoreItem<GDALDataset *>> parent;
  list<long> children;
  AsyncLock async_lock;
  ObjectStoreItem(Napi::ObjectReference &obj);
};

struct uv_sem_deleter {
  void operator()(uv_sem_t *p);
};

class ObjectStore {
    public:
  template <typename GDALPTR> long add(GDALPTR ptr, Napi::ObjectReference &obj, long parent_uid);
  long add(OGRLayer *ptr, Napi::ObjectReference &obj, long parent_uid, bool is_result_set);
  long add(GDALDataset *ptr, Napi::ObjectReference &obj, long parent_uid);

  void dispose(long uid, bool manual = false);
  bool isAlive(long uid);
  inline void lockDataset(AsyncLock lock) {
    uv_sem_wait(lock.get());
  }
  inline void unlockDataset(AsyncLock lock) {
    uv_sem_post(lock.get());
    uv_mutex_lock(&master_lock);
    uv_cond_broadcast(&master_sleep);
    uv_mutex_unlock(&master_lock);
  }
  inline void unlockDatasets(vector<AsyncLock> locks) {
    for (const AsyncLock &l : locks) uv_sem_post(l.get());
    uv_mutex_lock(&master_lock);
    uv_cond_broadcast(&master_sleep);
    uv_mutex_unlock(&master_lock);
  }
  AsyncLock lockDataset(long uid);
  vector<AsyncLock> lockDatasets(vector<long> uids);
  AsyncLock tryLockDataset(long uid);
  vector<AsyncLock> tryLockDatasets(vector<long> uids);

  template <typename GDALPTR> bool has(GDALPTR ptr);
  template <typename GDALPTR> Napi::Object get(GDALPTR ptr);
  template <typename GDALPTR> Napi::Object get(long uid);

  void cleanup();

  ObjectStore();
  ~ObjectStore();

    private:
  long uid;
  uv_mutex_t master_lock;
  uv_cond_t master_sleep;
  vector<AsyncLock> _tryLockDatasets(vector<long> uids);
  template <typename GDALPTR> void dispose(shared_ptr<ObjectStoreItem<GDALPTR>> item, bool manual);
  void do_dispose(long uid, bool manual = false);
};

} // namespace node_gdal

#endif
