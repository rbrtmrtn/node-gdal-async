#ifndef __NODE_OGR_POINT_H__
#define __NODE_OGR_POINT_H__

// node
#include <napi.h>
#include <uv.h>
#include <node_object_wrap.h>

// nan
#include <napi.h>

// ogr
#include <ogrsf_frmts.h>

#include "gdal_geometrybase.hpp"

using namespace Napi;
using namespace Napi;

namespace node_gdal {

class Point : public GeometryBase<Point, OGRPoint> {
    public:
  static Napi::FunctionReference constructor;
  using GeometryBase<Point, OGRPoint>::GeometryBase;

  static void Initialize(Napi::Object target);
  static Napi::Value New(const Napi::CallbackInfo &info);
  using GeometryBase<Point, OGRPoint>::New;
  static Napi::Value toString(const Napi::CallbackInfo &info);

  Napi::Value xGetter(const Napi::CallbackInfo &info);
  Napi::Value yGetter(const Napi::CallbackInfo &info);
  Napi::Value zGetter(const Napi::CallbackInfo &info);
  void xSetter(const Napi::CallbackInfo &info, const Napi::Value &value);
  void ySetter(const Napi::CallbackInfo &info, const Napi::Value &value);
  void zSetter(const Napi::CallbackInfo &info, const Napi::Value &value);
};

} // namespace node_gdal
#endif
