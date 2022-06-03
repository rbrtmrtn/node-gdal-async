#ifndef __NODE_GDAL_BAND_PIXELS_H__
#define __NODE_GDAL_BAND_PIXELS_H__

// node
#include <napi.h>
#include <uv.h>
#include <node_object_wrap.h>

// nan
#include <napi.h>

// gdal
#include <gdal_priv.h>

#include "../gdal_rasterband.hpp"
#include "../async.hpp"

using namespace Napi;
using namespace Napi;

namespace node_gdal {

class RasterBandPixels : public Napi::ObjectWrap<RasterBandPixels> {
    public:
  static Napi::FunctionReference constructor;

  static void Initialize(Napi::Object target);
  static Napi::Value New(const Napi::CallbackInfo &info);
  static Napi::Value New(Napi::Value band_obj);
  static Napi::Value toString(const Napi::CallbackInfo &info);

  GDAL_ASYNCABLE_DECLARE(get);
  GDAL_ASYNCABLE_DECLARE(set);
  GDAL_ASYNCABLE_DECLARE(read);
  GDAL_ASYNCABLE_DECLARE(write);
  GDAL_ASYNCABLE_DECLARE(readBlock);
  GDAL_ASYNCABLE_DECLARE(writeBlock);
  GDAL_ASYNCABLE_DECLARE(clampBlock);

  Napi::Value bandGetter(const Napi::CallbackInfo &info);

  static RasterBand *parent(const Napi::CallbackInfo &info);

  RasterBandPixels();

    private:
  ~RasterBandPixels();
};

} // namespace node_gdal
#endif
