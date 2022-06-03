#ifndef __NODE_GDAL_POLYGON_RINGS_H__
#define __NODE_GDAL_POLYGON_RINGS_H__

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

// Polygon.rings

namespace node_gdal {

class PolygonRings : public Napi::ObjectWrap<PolygonRings> {
    public:
  static Napi::FunctionReference constructor;

  static void Initialize(Napi::Object target);
  static Napi::Value New(const Napi::CallbackInfo &info);
  static Napi::Value New(Napi::Value geom);
  static Napi::Value toString(const Napi::CallbackInfo &info);

  static Napi::Value get(const Napi::CallbackInfo &info);
  static Napi::Value count(const Napi::CallbackInfo &info);
  static Napi::Value add(const Napi::CallbackInfo &info);
  static Napi::Value remove(const Napi::CallbackInfo &info);

  PolygonRings();

    private:
  ~PolygonRings();
};

} // namespace node_gdal
#endif
