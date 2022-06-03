#ifndef __NODE_OGR_MULTIPOLYGON_H__
#define __NODE_OGR_MULTIPOLYGON_H__

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

class MultiPolygon : public GeometryCollectionBase<MultiPolygon, OGRMultiPolygon> {

    public:
  static Napi::FunctionReference constructor;
  using GeometryCollectionBase<MultiPolygon, OGRMultiPolygon>::GeometryCollectionBase;

  static void Initialize(Napi::Object target);
  using GeometryCollectionBase<MultiPolygon, OGRMultiPolygon>::New;
  static Napi::Value toString(const Napi::CallbackInfo &info);
  static Napi::Value unionCascaded(const Napi::CallbackInfo &info);
  static Napi::Value getArea(const Napi::CallbackInfo &info);
};

} // namespace node_gdal
#endif
