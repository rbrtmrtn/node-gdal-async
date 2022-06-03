#ifndef __NODE_OGR_MULTICURVE_H__
#define __NODE_OGR_MULTICURVE_H__

// node
#include <napi.h>
#include <uv.h>
#include <node_object_wrap.h>

// nan
#include <napi.h>

// ogr
#include <ogrsf_frmts.h>

#include "gdal_geometrycollectionbase.hpp"

using namespace Napi;
using namespace Napi;

namespace node_gdal {

class MultiCurve : public GeometryCollectionBase<MultiCurve, OGRMultiCurve> {

    public:
  static Napi::FunctionReference constructor;
  using GeometryCollectionBase<MultiCurve, OGRMultiCurve>::GeometryCollectionBase;

  static void Initialize(Napi::Object target);
  using GeometryCollectionBase<MultiCurve, OGRMultiCurve>::New;
  static Napi::Value toString(const Napi::CallbackInfo &info);
  static Napi::Value polygonize(const Napi::CallbackInfo &info);
};

} // namespace node_gdal
#endif
