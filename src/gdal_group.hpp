#ifndef __NODE_GDAL_GROUP_H__
#define __NODE_GDAL_GROUP_H__

// node
#include <napi.h>
#include <uv.h>
#include <node_object_wrap.h>

// nan
#include <napi.h>

// gdal
#include <gdal_priv.h>

// ogr
#include <ogrsf_frmts.h>

#include "async.hpp"

#if GDAL_VERSION_MAJOR > 3 || (GDAL_VERSION_MAJOR == 3 && GDAL_VERSION_MINOR >= 1)

using namespace Napi;
using namespace Napi;

namespace node_gdal {

class Group : public Napi::ObjectWrap<Group> {
    public:
  static Napi::FunctionReference constructor;
  static void Initialize(Napi::Object target);
  static Napi::Value New(const Napi::CallbackInfo &info);
  static Napi::Value New(std::shared_ptr<GDALGroup> group, Napi::Object parent_ds);
  static Napi::Value New(std::shared_ptr<GDALGroup> group, GDALDataset *parent_ds);
  static Napi::Value toString(const Napi::CallbackInfo &info);
  Napi::Value descriptionGetter(const Napi::CallbackInfo &info);
  Napi::Value groupsGetter(const Napi::CallbackInfo &info);
  Napi::Value arraysGetter(const Napi::CallbackInfo &info);
  Napi::Value dimensionsGetter(const Napi::CallbackInfo &info);
  Napi::Value attributesGetter(const Napi::CallbackInfo &info);
  Napi::Value uidGetter(const Napi::CallbackInfo &info);

  Group();
  Group(std::shared_ptr<GDALGroup> group);
  inline std::shared_ptr<GDALGroup> get() {
    return this_;
  }

  void dispose();
  long uid;
  long parent_uid;

  inline bool isAlive() {
    return this_ && object_store.isAlive(uid);
  }

    private:
  ~Group();
  std::shared_ptr<GDALGroup> this_;
  GDALDataset *parent_ds;
};

} // namespace node_gdal
#endif
#endif
