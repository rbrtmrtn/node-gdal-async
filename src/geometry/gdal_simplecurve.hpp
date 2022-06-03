#ifndef __NODE_OGR_CURVE_H__
#define __NODE_OGR_CURVE_H__

// node
#include <napi.h>
#include <uv.h>
#include <node_object_wrap.h>

// nan
#include <napi.h>

// ogr
#include <ogrsf_frmts.h>

#include "gdal_curvebase.hpp"
#include "../collections/linestring_points.hpp"

using namespace Napi;
using namespace Napi;

namespace node_gdal {

class SimpleCurve : public CurveBase<SimpleCurve, OGRSimpleCurve, LineStringPoints> {

    public:
  static Napi::FunctionReference constructor;
  using CurveBase<SimpleCurve, OGRSimpleCurve, LineStringPoints>::CurveBase;

  static void Initialize(Napi::Object target);
  static Napi::Value New(const Napi::CallbackInfo &info);
  static Napi::Value toString(const Napi::CallbackInfo &info);
  static Napi::Value value(const Napi::CallbackInfo &info);
  static Napi::Value getLength(const Napi::CallbackInfo &info);
  static Napi::Value addSubLineString(const Napi::CallbackInfo &info);

  Napi::Value pointsGetter(const Napi::CallbackInfo &info);
};

} // namespace node_gdal

#endif
