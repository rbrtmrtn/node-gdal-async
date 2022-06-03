#ifndef __NODE_OGR_CIRCULARSTRING_H__
#define __NODE_OGR_CIRCULARSTRING_H__

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

class CircularString : public CurveBase<CircularString, OGRCircularString, LineStringPoints> {

    public:
  static Napi::FunctionReference constructor;
  using CurveBase<CircularString, OGRCircularString, LineStringPoints>::CurveBase;

  static void Initialize(Napi::Object target);
  using CurveBase<CircularString, OGRCircularString, LineStringPoints>::New;
  static Napi::Value toString(const Napi::CallbackInfo &info);
};

} // namespace node_gdal
#endif
