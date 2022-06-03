#ifndef __NODE_OGR_MULTIPOINT_H__
#define __NODE_OGR_MULTIPOINT_H__

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

class MultiPoint : public GeometryCollectionBase<MultiPoint, OGRMultiPoint> {

    public:
  static Napi::FunctionReference constructor;
  using GeometryCollectionBase<MultiPoint, OGRMultiPoint>::GeometryCollectionBase;

  static void Initialize(Napi::Object target);
  using GeometryCollectionBase<MultiPoint, OGRMultiPoint>::New;
  static Napi::Value toString(const Napi::CallbackInfo &info);
};

} // namespace node_gdal
#endif
