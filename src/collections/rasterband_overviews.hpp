#ifndef __NODE_GDAL_BAND_OVERVIEWS_H__
#define __NODE_GDAL_BAND_OVERVIEWS_H__

// node
#include <napi.h>
#include <uv.h>
#include <node_object_wrap.h>

// nan
#include <napi.h>

// gdal
#include <gdal_priv.h>

#include "../async.hpp"

using namespace Napi;
using namespace Napi;

namespace node_gdal {

class RasterBandOverviews : public Napi::ObjectWrap<RasterBandOverviews> {
    public:
  static Napi::FunctionReference constructor;

  static void Initialize(Napi::Object target);
  static Napi::Value New(const Napi::CallbackInfo &info);
  static Napi::Value New(Napi::Value band_obj);
  static Napi::Value toString(const Napi::CallbackInfo &info);

  GDAL_ASYNCABLE_DECLARE(get);
  GDAL_ASYNCABLE_DECLARE(getBySampleCount);
  GDAL_ASYNCABLE_DECLARE(count);

  RasterBandOverviews();

    private:
  ~RasterBandOverviews();
};

} // namespace node_gdal
#endif
