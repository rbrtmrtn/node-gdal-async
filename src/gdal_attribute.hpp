#ifndef __NODE_GDAL_ATTRIBUTE_H__
#define __NODE_GDAL_ATTRIBUTE_H__

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

class Attribute : public Napi::ObjectWrap<Attribute> {
    public:
  static Napi::FunctionReference constructor;
  static void Initialize(Napi::Object target);
  static Napi::Value New(const Napi::CallbackInfo &info);
  static Napi::Value New(std::shared_ptr<GDALAttribute> group, GDALDataset *parent_ds);
  static Napi::Value toString(const Napi::CallbackInfo &info);
  Napi::Value typeGetter(const Napi::CallbackInfo &info);
  Napi::Value valueGetter(const Napi::CallbackInfo &info);
  Napi::Value uidGetter(const Napi::CallbackInfo &info);

  Attribute();
  Attribute(std::shared_ptr<GDALAttribute> group);
  inline std::shared_ptr<GDALAttribute> get() {
    return this_;
  }

  void dispose();
  long uid;
  long parent_uid;

  inline bool isAlive() {
    return this_ && object_store.isAlive(uid);
  }

    private:
  ~Attribute();
  std::shared_ptr<GDALAttribute> this_;
  GDALDataset *parent_ds;
};

} // namespace node_gdal
#endif
#endif
