#ifndef __NODE_OGR_POLY_H__
#define __NODE_OGR_POLY_H__

// node
#include <napi.h>
#include <uv.h>
#include <node_object_wrap.h>

// nan
#include <napi.h>

// ogr
#include <ogrsf_frmts.h>

#include "gdal_curvebase.hpp"
#include "../collections/polygon_rings.hpp"

using namespace Napi;
using namespace Napi;

namespace node_gdal {

class Polygon : public CurveBase<Polygon, OGRPolygon, PolygonRings> {
  friend CurveBase;

    public:
  static Napi::FunctionReference constructor;
  using CurveBase<Polygon, OGRPolygon, PolygonRings>::CurveBase;

  static void Initialize(Napi::Object target);
  using CurveBase<Polygon, OGRPolygon, PolygonRings>::New;
  static Napi::Value toString(const Napi::CallbackInfo &info);
  static Napi::Value getArea(const Napi::CallbackInfo &info);

  Napi::Value ringsGetter(const Napi::CallbackInfo &info);

    protected:
  static void SetPrivate(Napi::ADDON_REGISTER_FUNCTION_ARGS_TYPE, Napi::Value);
};

} // namespace node_gdal
#endif
