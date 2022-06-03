#ifndef __NODE_GDAL_DRIVERS_H__
#define __NODE_GDAL_DRIVERS_H__

// node
#include <napi.h>
#include <uv.h>
#include <node_object_wrap.h>

// nan
#include <napi.h>

// gdal
#include <gdal_priv.h>

using namespace Napi;
using namespace Napi;

namespace node_gdal {

class GDALDrivers : public Napi::ObjectWrap<GDALDrivers> {
    public:
  static Napi::FunctionReference constructor;

  static void Initialize(Napi::Object target);
  static Napi::Value New(const Napi::CallbackInfo &info);
  static Napi::Value New();
  static Napi::Value toString(const Napi::CallbackInfo &info);

  static Napi::Value get(const Napi::CallbackInfo &info);
  static Napi::Value getNames(const Napi::CallbackInfo &info);
  static Napi::Value count(const Napi::CallbackInfo &info);

  GDALDrivers();

    private:
  ~GDALDrivers();
};

} // namespace node_gdal
#endif
