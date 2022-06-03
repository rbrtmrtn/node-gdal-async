#ifndef __NODE_GDAL_DRIVER_H__
#define __NODE_GDAL_DRIVER_H__

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

using namespace Napi;
using namespace Napi;

// > GDAL 2.0 : a wrapper for GDALDriver
// < GDAL 2.0 : a wrapper for either a GDALDriver or OGRSFDriver that behaves
// like a 2.0 Driver
//
namespace node_gdal {

class Driver : public Napi::ObjectWrap<Driver> {
    public:
  static Napi::FunctionReference constructor;
  static void Initialize(Napi::Object target);
  static Napi::Value New(const Napi::CallbackInfo &info);
  static Napi::Value New(GDALDriver *driver);
  static Napi::Value toString(const Napi::CallbackInfo &info);
  GDAL_ASYNCABLE_DECLARE(open);
  GDAL_ASYNCABLE_DECLARE(create);
  GDAL_ASYNCABLE_DECLARE(createCopy);
  static Napi::Value deleteDataset(const Napi::CallbackInfo &info);
  static Napi::Value rename(const Napi::CallbackInfo &info);
  GDAL_ASYNCABLE_DECLARE(copyFiles);
  static Napi::Value getMetadata(const Napi::CallbackInfo &info);

  Napi::Value descriptionGetter(const Napi::CallbackInfo &info);

  Driver();
  Driver(GDALDriver *driver);
  inline GDALDriver *getGDALDriver() {
    return this_gdaldriver;
  }
  void dispose();
  long uid;

  inline bool isAlive() {
    return this_gdaldriver;
  }

    private:
  ~Driver();
  GDALDriver *this_gdaldriver;
};

} // namespace node_gdal
#endif
