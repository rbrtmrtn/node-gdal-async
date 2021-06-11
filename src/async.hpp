#ifndef __NODE_GDAL_ASYNC_WORKER_H__
#define __NODE_GDAL_ASYNC_WORKER_H__

#include <functional>
#include "nan-wrapper.h"
#include "gdal_common.hpp"

namespace node_gdal {

// This generates method definitions for 2 methods: sync and async version and a hidden common block
#define GDAL_ASYNCABLE_DEFINE(method)                                                                                  \
  NAN_METHOD(method) {                                                                                                 \
    method##_do(info, false);                                                                                          \
  }                                                                                                                    \
  NAN_METHOD(method##Async) {                                                                                          \
    method##_do(info, true);                                                                                           \
  }                                                                                                                    \
  void method##_do(const Nan::FunctionCallbackInfo<v8::Value> &info, bool async)

// This generates method declarations for 2 methods: sync and async version and a hidden common block
#define GDAL_ASYNCABLE_DECLARE(method)                                                                                 \
  static NAN_METHOD(method);                                                                                           \
  static NAN_METHOD(method##Async);                                                                                    \
  static void method##_do(const Nan::FunctionCallbackInfo<v8::Value> &info, bool async)

#define GDAL_ASYNCABLE_GLOBAL(method)                                                                                  \
  NAN_METHOD(method);                                                                                                  \
  NAN_METHOD(method##Async);                                                                                           \
  void method##_do(const Nan::FunctionCallbackInfo<v8::Value> &info, bool async)

#define GDAL_ASYNCABLE_TEMPLATE(method)                                                                                \
  static NAN_METHOD(method) {                                                                                          \
    method##_do(info, false);                                                                                          \
  }                                                                                                                    \
  static NAN_METHOD(method##Async) {                                                                                   \
    method##_do(info, true);                                                                                           \
  }                                                                                                                    \
  static void method##_do(const Nan::FunctionCallbackInfo<v8::Value> &info, bool async)

#define GDAL_ISASYNC async

// Handle locking
#define GDAL_LOCK_PARENT(p)                                                                                            \
  std::shared_ptr<uv_sem_t> async_lock = nullptr;                                                                      \
  try {                                                                                                                \
    async_lock = object_store.lockDataset((p)->parent_uid);                                                            \
  } catch (const char *err) {                                                                                          \
    Nan::ThrowError(err);                                                                                              \
    return;                                                                                                            \
  }
#define GDAL_LOCK_DS(uid)                                                                                              \
  std::shared_ptr<uv_sem_t> async_lock = nullptr;                                                                      \
  try {                                                                                                                \
    async_lock = object_store.lockDataset(uid);                                                                        \
  } catch (const char *err) {                                                                                          \
    Nan::ThrowError(err);                                                                                              \
    return;                                                                                                            \
  }
#define GDAL_UNLOCK_PARENT uv_sem_post(async_lock.get())

#define GDAL_ASYNCABLE_LOCK_MANY(...)                                                                                  \
  std::vector<std::shared_ptr<uv_sem_t>> async_locks = object_store.lockDatasets({__VA_ARGS__});
#define GDAL_UNLOCK_MANY                                                                                               \
  for (std::shared_ptr<uv_sem_t> & async_lock : async_locks) { uv_sem_post(async_lock.get()); }

// Node.js NAN null initializes and trivially copies objects of this class without asking permission
struct GDALProgressInfo {
  double complete;
  const char *message;

  GDALProgressInfo(const GDALProgressInfo &o);
  GDALProgressInfo(double, const char *);
  GDALProgressInfo();
};

class GDALSyncExecutionProgress {
  Nan::Callback *progress_callback;

  GDALSyncExecutionProgress() = delete;

    public:
  GDALSyncExecutionProgress(Nan::Callback *);
  ~GDALSyncExecutionProgress();
  void Send(GDALProgressInfo *) const;
};

typedef std::function<v8::Local<v8::Value>(const char *)> GetFromPersistentFunc;
class GDALAsyncProgressWorker : public Nan::AsyncProgressWorkerBase<GDALProgressInfo> {
    protected:
  std::shared_ptr<uv_sem_t> async_lock;
  uv_loop_t *event_loop;

    public:
  GDALAsyncProgressWorker(Nan::Callback *resultCallback);
  void passLock(const std::shared_ptr<uv_sem_t> &lock);
};

typedef GDALAsyncProgressWorker::ExecutionProgress GDALAsyncExecutionProgress;

// This an ExecutionContext that works both with Node.js' NAN ExecutionProgress when in async mode
// and with GDALSyncExecutionContext when in sync mode
class GDALExecutionProgress {
  // Only one of these is active at any given moment
  const GDALAsyncExecutionProgress *async;
  const GDALSyncExecutionProgress *sync;

  GDALExecutionProgress() = delete;

    public:
  GDALExecutionProgress(const GDALAsyncExecutionProgress *);
  GDALExecutionProgress(const GDALSyncExecutionProgress *);
  ~GDALExecutionProgress();
  void Send(GDALProgressInfo *info) const;
};

/**
 * @typedef ProgressOptions { progress_cb: ProgressCb }
 */

/**
 * @typedef ProgressCb ( complete: number, msg: string ) => void
 */

// This is the progress callback trampoline
// It can be invoked both in the main thread (in sync mode) or in auxillary thread (in async mode)
// It is essentially a gateway between the GDAL world and Node.js/V8 world
int ProgressTrampoline(double dfComplete, const char *pszMessage, void *pProgressArg);

//
// This class handles async operations
// It is meant to be used by GDALAsyncableJob defined below
//
// It takes the lambdas as input
// GDALType is the type of the object that will be carried from
// the aux thread to the main thread
//
// JS-visible object creation is possible only in the main thread while
// ths JS world is not running
//
template <class GDALType> class GDALAsyncWorker : public GDALAsyncProgressWorker {
    public:
  typedef std::function<GDALType(const GDALExecutionProgress &)> GDALMainFunc;
  typedef std::function<v8::Local<v8::Value>(const GDALType, const GetFromPersistentFunc &)> GDALRValFunc;

    private:
  long ds_uid;
  Nan::Callback *progressCallback;
  const GDALMainFunc doit;
  const GDALRValFunc rval;
  GDALType raw;

    public:
  explicit GDALAsyncWorker(
    long ds_uid,
    Nan::Callback *resultCallback,
    Nan::Callback *progressCallback,
    const GDALMainFunc &doit,
    const GDALRValFunc &rval,
    const std::map<std::string, v8::Local<v8::Object>> &objects);

  ~GDALAsyncWorker();

  void Execute(const ExecutionProgress &progress);
  void HandleOKCallback();
  void HandleErrorCallback();
  void HandleProgressCallback(const GDALProgressInfo *data, size_t count);
};

template <class GDALType>
GDALAsyncWorker<GDALType>::GDALAsyncWorker(
  long ds_uid,
  Nan::Callback *resultCallback,
  Nan::Callback *progressCallback,
  const GDALMainFunc &doit,
  const GDALRValFunc &rval,
  const std::map<std::string, v8::Local<v8::Object>> &objects)
  : GDALAsyncProgressWorker(resultCallback),
    ds_uid(ds_uid),
    progressCallback(progressCallback),
    // These members are not references! These functions must be copied
    // as they will be executed in async context!
    doit(doit),
    rval(rval) {
  // Main thread with the JS world is not running
  // Get persistent handles
  for (auto i = objects.begin(); i != objects.end(); i++) SaveToPersistent(i->first.c_str(), i->second);
}

template <class GDALType> GDALAsyncWorker<GDALType>::~GDALAsyncWorker() {
  if (progressCallback) delete progressCallback;
}

template <class GDALType> void GDALAsyncWorker<GDALType>::Execute(const ExecutionProgress &progress) {
  // Aux thread with the JS world running
  // V8 objects are not acessible here
  LOG("Running async job for Dataset %ld", ds_uid);
  try {
    GDALExecutionProgress executionProgress(&progress);
    raw = doit(executionProgress);
  } catch (const char *err) { this->SetErrorMessage(err); }

  // This can be a single job without a Dataset
  if (ds_uid == 0) { return; }

  std::unique_ptr<GDALAsyncProgressWorker> next_job = object_store.dequeueJob(ds_uid);
  if (next_job != nullptr) {
    // If there is another job waiting for this Dataset, we enqueue it in libuv and we pass the lock
    LOG("Chaining another async job for Dataset %ld [%p]", ds_uid, next_job.get());
    next_job->passLock(async_lock);
    // This is a manual call of Nan::AsyncQueueWorker with the saved pointer to the event loop
    // The unique_ptr to the job is released as Nan::AsyncQueueWorker expects ownership of the pointer
    // TODO: Maybe this should be a Nan feature
    uv_queue_work(event_loop, &(next_job.release()->request), Nan::AsyncExecute, Nan::AsyncExecuteComplete);
  } else {
    // Otherwise we can unlock the Dataset
    LOG("Queue is empty for Dataset %ld", ds_uid);
    uv_sem_post(async_lock.get());
  }
  async_lock = nullptr;
}

template <class GDALType> void GDALAsyncWorker<GDALType>::HandleOKCallback() {
  // Back to the main thread with the JS world not running
  Nan::HandleScope scope;

  // rval is the user function that will create the returned value
  // we give it a lambda that can access the persistent storage created for this operation
  v8::Local<v8::Value> argv[] = {
    Nan::Null(), rval(raw, [this](const char *key) { return this->GetFromPersistent(key); })};
  callback->Call(2, argv, async_resource);
}

template <class GDALType> void GDALAsyncWorker<GDALType>::HandleErrorCallback() {
  // Back to the main thread with the JS world not running
  Nan::HandleScope scope;
  v8::Local<v8::Value> argv[] = {Nan::Error(this->ErrorMessage())};
  callback->Call(1, argv, async_resource);
}

template <class GDALType>
void GDALAsyncWorker<GDALType>::HandleProgressCallback(const GDALProgressInfo *data, size_t count) {
  // Back to the main thread with the JS world not running
  Nan::HandleScope scope;
  // A mutex-protected pop in the calling function (in Node.js NAN) can sometimes produce a spurious call
  // with no data, handle gracefully this case -> no need to call JS if there is not data to deliver
  if (data == nullptr || count == 0) return;
  // Receiving more than one callback invocation at the same time is also possible
  // Send only the last one to JS
  const GDALProgressInfo *to_send = data + (count - 1);
  if (data != nullptr && count > 0) {
    v8::Local<v8::Value> argv[] = {Nan::New<Number>(to_send->complete), SafeString::New(to_send->message)};
    progressCallback->Call(2, argv, async_resource);
  }
}

// This the basic unit of the GDALAsyncable framework
// GDALAsyncableJob is a GDAL job consisting of a main
// lambda that calls GDAL and rval lambda that transforms
// the return value
// It handles persistence of the referenced objects and
// can be executed both synchronously and asynchronously
//
// GDALType is the intermediary type that will be carried
// across the context boundaries
//
// The caller must ensure that all the lambdas can be executed in
// another thread:
// * no capturing of automatic variables (C++ memory management)
// * no referencing of JS-visible objects in main() (V8 memory management)
// * protecting all JS-visible objects from the GC by calling persist() (V8 MM)
// * locking all GDALDatasets (GDAL limitation)
//
// If a GDALDataset is locked, but not persisted, the GC could still
// try to free it - in this case it will stop the JS world and then it will wait
// on the Dataset lock in PtrManager::dispose() blocking the event loop - the situation
// is safe but it is still best if it is avoided

template <class GDALType> class GDALAsyncableJob {
    public:
  typedef std::function<GDALType(const GDALExecutionProgress &)> GDALMainFunc;
  typedef std::function<v8::Local<v8::Value>(const GDALType, const GetFromPersistentFunc &)> GDALRValFunc;
  // This is the lambda that produces the <GDALType> object
  GDALMainFunc main;
  // This is the lambda that produces the JS return object from the <GDALType> object
  GDALRValFunc rval;
  Nan::Callback *progress;

  GDALAsyncableJob() : main(), rval(), progress(nullptr), persistent(), autoIndex(0){};

  inline void persist(const std::string &key, const v8::Local<v8::Object> &obj) {
    persistent[key] = obj;
  }

  inline void persist(const v8::Local<v8::Object> &obj) {
    persistent[std::to_string(autoIndex++)] = obj;
  }

  inline void persist(const v8::Local<v8::Object> &obj1, const v8::Local<v8::Object> &obj2) {
    persist(obj1);
    persist(obj2);
  }

  inline void persist(const std::vector<v8::Local<v8::Object>> &objs) {
    for (auto const &i : objs) persist(i);
  }

  // Run without locking a Dataset
  void run(const Nan::FunctionCallbackInfo<v8::Value> &info, bool async, int cb_arg) {
    // Async execution
    if (async) {
      Nan::Callback *callback;
      NODE_ARG_CB(cb_arg, "callback", callback);
      LOG("Will start immediately an async job with no Dataset (%d)", (int)async);
      Nan::AsyncQueueWorker(new GDALAsyncWorker<GDALType>(0, callback, progress, main, rval, persistent));
      return;
    }
    // Sync execution
    try {
      GDALExecutionProgress executionProgress(new GDALSyncExecutionProgress(progress));
      GDALType obj = main(executionProgress);
      // rval is the user function that will create the returned value
      // we give it a lambda that can access the persistent storage created for this operation
      info.GetReturnValue().Set(rval(obj, [this](const char *key) { return this->persistent[key]; }));
    } catch (const char *err) { Nan::ThrowError(err); }
  }

  // Run while locking a Dataset
  void run(const Nan::FunctionCallbackInfo<v8::Value> &info, bool async, int cb_arg, long ds_uid) {
    std::shared_ptr<uv_sem_t> lock = nullptr;

    // Async execution
    if (async) {
      Nan::Callback *callback;
      NODE_ARG_CB(cb_arg, "callback", callback);

      unique_ptr<GDALAsyncProgressWorker> async_job =
        make_unique<GDALAsyncWorker<GDALType>>(ds_uid, callback, progress, main, rval, persistent);

      // This is the master lock, this is a total exclusion zone (critical section)
      object_store.lock();
      lock = object_store.tryLockDataset(ds_uid);
      if (lock != nullptr) {
        object_store.unlock();
        // The Dataset is available, we start the job right away and pass it the lock (semaphore)
        // it will unlock it when it is finished
        LOG("Will start immediately an async job for Dataset %ld", ds_uid);
        async_job->passLock(lock);
        Nan::AsyncQueueWorker(async_job.release());
      } else {
        // An async operation is already in progress for this Dataset and we couldn't acquire the lock
        // The object store will take care of enqueuing
        LOG("Enqueuing an async job for Dataset %ld", ds_uid);
        object_store.enqueueJob(std::move(async_job), ds_uid);
        object_store.unlock();
      }
      return;
    }

    // Sync execution
    try {
      GDALExecutionProgress executionProgress(new GDALSyncExecutionProgress(progress));
      lock = object_store.tryLockDataset(ds_uid);
      if (lock == nullptr) {
        fprintf(
          stderr,
          "Warning, synchronous function call during asynchronous operation, waiting while holding the event loop\n");
        lock = object_store.lockDataset(ds_uid);
      }
      GDALType obj = main(executionProgress);
      uv_sem_post(lock.get());
      // rval is the user function that will create the returned value
      // we give it a lambda that can access the persistent storage created for this operation
      info.GetReturnValue().Set(rval(obj, [this](const char *key) { return this->persistent[key]; }));
    } catch (const char *err) {
      if (lock != nullptr) {
        uv_sem_post(lock.get());
        lock = nullptr;
      }
      Nan::ThrowError(err);
    }
  }

    private:
  std::map<std::string, v8::Local<v8::Object>> persistent;
  unsigned autoIndex;
};

} // namespace node_gdal
#endif
