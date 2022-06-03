#ifndef __NODE_OGR_GEOMETRY_H__
#define __NODE_OGR_GEOMETRY_H__

// node
#include <napi.h>
#include <uv.h>
#include <node_object_wrap.h>

// nan
#include <napi.h>

// ogr
#include <ogrsf_frmts.h>

#include "../async.hpp"
#include "gdal_geometrybase.hpp"

using namespace Napi;
using namespace Napi;

namespace node_gdal {

class Geometry : public GeometryBase<Geometry, OGRGeometry> {
    public:
  static Napi::FunctionReference constructor;
  using InstanceWrap<GeometryBase<Geometry, OGRGeometry>>::InstanceMethod;

  static void Initialize(Napi::Object target);
  Napi::Value New(const Napi::CallbackInfo &info);
  using GeometryBase<Geometry, OGRGeometry>::New;
  Napi::Value New(OGRGeometry *geom, bool owned);
  Napi::Value toString(const Napi::CallbackInfo &info);
  GDAL_ASYNCABLE_DECLARE(isEmpty);
  GDAL_ASYNCABLE_DECLARE(isValid);
  GDAL_ASYNCABLE_DECLARE(isSimple);
  GDAL_ASYNCABLE_DECLARE(isRing);
  static Napi::Value clone(const Napi::CallbackInfo &info);
  GDAL_ASYNCABLE_DECLARE(empty);
  GDAL_ASYNCABLE_DECLARE(exportToKML);
  GDAL_ASYNCABLE_DECLARE(exportToGML);
  GDAL_ASYNCABLE_DECLARE(exportToJSON);
  GDAL_ASYNCABLE_DECLARE(exportToWKT);
  GDAL_ASYNCABLE_DECLARE(exportToWKB);
  GDAL_ASYNCABLE_DECLARE(closeRings);
  GDAL_ASYNCABLE_DECLARE(segmentize);
  GDAL_ASYNCABLE_DECLARE(intersects);
  GDAL_ASYNCABLE_DECLARE(equals);
  GDAL_ASYNCABLE_DECLARE(disjoint);
  GDAL_ASYNCABLE_DECLARE(touches);
  GDAL_ASYNCABLE_DECLARE(crosses);
  GDAL_ASYNCABLE_DECLARE(within);
  GDAL_ASYNCABLE_DECLARE(contains);
  GDAL_ASYNCABLE_DECLARE(overlaps);
  GDAL_ASYNCABLE_DECLARE(boundary);
  GDAL_ASYNCABLE_DECLARE(distance);
  GDAL_ASYNCABLE_DECLARE(convexHull);
  GDAL_ASYNCABLE_DECLARE(buffer);
  GDAL_ASYNCABLE_DECLARE(intersection);
  GDAL_ASYNCABLE_DECLARE(unionGeometry);
  GDAL_ASYNCABLE_DECLARE(difference);
  GDAL_ASYNCABLE_DECLARE(symDifference);
  GDAL_ASYNCABLE_DECLARE(centroid);
  GDAL_ASYNCABLE_DECLARE(simplify);
  GDAL_ASYNCABLE_DECLARE(simplifyPreserveTopology);
  GDAL_ASYNCABLE_DECLARE(polygonize);
  GDAL_ASYNCABLE_DECLARE(swapXY);
  static Napi::Value getNumGeometries(const Napi::CallbackInfo &info);
  GDAL_ASYNCABLE_DECLARE(getEnvelope);
  GDAL_ASYNCABLE_DECLARE(getEnvelope3D);
  GDAL_ASYNCABLE_DECLARE(flattenTo2D);
  GDAL_ASYNCABLE_DECLARE(transform);
  GDAL_ASYNCABLE_DECLARE(transformTo);
#if GDAL_VERSION_MAJOR >= 3
  GDAL_ASYNCABLE_DECLARE(makeValid);
#endif

  // static constructor methods
  GDAL_ASYNCABLE_DECLARE(create);
  GDAL_ASYNCABLE_DECLARE(createFromWkt);
  GDAL_ASYNCABLE_DECLARE(createFromWkb);
  GDAL_ASYNCABLE_DECLARE(createFromGeoJson);
  GDAL_ASYNCABLE_DECLARE(createFromGeoJsonBuffer);
  Napi::Value getName(const Napi::CallbackInfo &info);
  Napi::Value getConstructor(const Napi::CallbackInfo &info);

  Napi::Value srsGetter(const Napi::CallbackInfo &info);
  Napi::Value typeGetter(const Napi::CallbackInfo &info);
  Napi::Value nameGetter(const Napi::CallbackInfo &info);
  Napi::Value wkbSizeGetter(const Napi::CallbackInfo &info);
  Napi::Value dimensionGetter(const Napi::CallbackInfo &info);
  Napi::Value coordinateDimensionGetter(const Napi::CallbackInfo &info);

  void srsSetter(const Napi::CallbackInfo &info, const Napi::Value &value);
  void coordinateDimensionSetter(const Napi::CallbackInfo &info, const Napi::Value &value);

  static OGRwkbGeometryType getGeometryType_fixed(OGRGeometry *geom);
  static Napi::Value _getConstructor(OGRwkbGeometryType type);
};

} // namespace node_gdal
#endif
