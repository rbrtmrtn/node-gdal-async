#ifndef __NODE_OGR_GEOMETRYCOLLECTION_H__
#define __NODE_OGR_GEOMETRYCOLLECTION_H__

// node
#include <napi.h>
#include <uv.h>
#include <node_object_wrap.h>

// nan
#include <napi.h>

// ogr
#include <ogrsf_frmts.h>

using namespace Napi;
using namespace Napi;

#include "gdal_geometrybase.hpp"
#include "gdal_geometrycollectionbase.hpp"
#include "../collections/geometry_collection_children.hpp"

namespace node_gdal {

class GeometryCollection : public GeometryCollectionBase<GeometryCollection, OGRGeometryCollection> {

    public:
  static Napi::FunctionReference constructor;
  using GeometryCollectionBase<GeometryCollection, OGRGeometryCollection>::GeometryCollectionBase;

  static void Initialize(Napi::Object target);
  using GeometryCollectionBase<GeometryCollection, OGRGeometryCollection>::New;
  static Napi::Value toString(const Napi::CallbackInfo &info);
  static Napi::Value getArea(const Napi::CallbackInfo &info);
  static Napi::Value getLength(const Napi::CallbackInfo &info);

  Napi::Value childrenGetter(const Napi::CallbackInfo &info);
};

} // namespace node_gdal
#endif
