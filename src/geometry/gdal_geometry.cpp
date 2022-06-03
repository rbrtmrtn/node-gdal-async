#include "../gdal_common.hpp"

#include "../gdal_coordinate_transformation.hpp"
#include "gdal_geometry.hpp"
#include "gdal_geometrycollection.hpp"
#include "gdal_linearring.hpp"
#include "gdal_linestring.hpp"
#include "gdal_circularstring.hpp"
#include "gdal_compoundcurve.hpp"
#include "gdal_multilinestring.hpp"
#include "gdal_multicurve.hpp"
#include "gdal_multipoint.hpp"
#include "gdal_multipolygon.hpp"
#include "gdal_point.hpp"
#include "gdal_polygon.hpp"
#include "../gdal_spatial_reference.hpp"

#include <node_buffer.h>
#include <ogr_core.h>
#include <memory>
#include <sstream>
#include <stdlib.h>

namespace node_gdal {

Napi::FunctionReference Geometry::constructor;

void Geometry::Initialize(Napi::Object target) {
  Napi::Env env = target.Env();
  Napi::FunctionReference *lcons = new Napi::FunctionReference();

  Napi::InstanceWrap<Geometry>::InstanceMethod(
    "getName", &Geometry::getName, static_cast<napi_property_attributes>(napi_writable | napi_configurable));

  Napi::Function cons = DefineClass(env, "Geometry", {
    InstanceAsyncableMethod("fromWKT", createFromWkt), InstanceAsyncableMethod("fromWKB", createFromWkb),
      InstanceAsyncableMethod("fromGeoJson", createFromGeoJson),
      InstanceAsyncableMethod("fromGeoJsonBuffer", createFromGeoJsonBuffer),
      InstanceMethod<&getName>("getName", static_cast<napi_property_attributes>(napi_writable | napi_configurable)),
      InstanceMethod<&getConstructor>(
        "getConstructor", static_cast<napi_property_attributes>(napi_writable | napi_configurable)),
      InstanceMethod<&toString>("toString", static_cast<napi_property_attributes>(napi_writable | napi_configurable)),
      PrototypeAsyncableMethod("toKML", exportToKML), PrototypeAsyncableMethod("toGML", exportToGML),
      PrototypeAsyncableMethod("toJSON", exportToJSON), PrototypeAsyncableMethod("toWKT", exportToWKT),
      PrototypeAsyncableMethod("toWKB", exportToWKB), PrototypeAsyncableMethod("isEmpty", isEmpty),
      PrototypeAsyncableMethod("isValid", isValid), PrototypeAsyncableMethod("isSimple", isSimple),
      PrototypeAsyncableMethod("isRing", isRing), InstanceMethod("clone", &clone),
      PrototypeAsyncableMethod("empty", empty), PrototypeAsyncableMethod("closeRings", closeRings),
      PrototypeAsyncableMethod("intersects", intersects), PrototypeAsyncableMethod("equals", equals),
      PrototypeAsyncableMethod("disjoint", disjoint), PrototypeAsyncableMethod("touches", touches),
      PrototypeAsyncableMethod("crosses", crosses), PrototypeAsyncableMethod("within", within),
      PrototypeAsyncableMethod("contains", contains), PrototypeAsyncableMethod("overlaps", overlaps),
      PrototypeAsyncableMethod("boundary", boundary), PrototypeAsyncableMethod("distance", distance),
      PrototypeAsyncableMethod("convexHull", convexHull), PrototypeAsyncableMethod("buffer", buffer),
      PrototypeAsyncableMethod("intersection", intersection), PrototypeAsyncableMethod("union", unionGeometry),
      PrototypeAsyncableMethod("difference", difference), PrototypeAsyncableMethod("symDifference", symDifference),
      PrototypeAsyncableMethod("centroid", centroid), PrototypeAsyncableMethod("simplify", simplify),
      PrototypeAsyncableMethod("simplifyPreserveTopology", simplifyPreserveTopology),
      InstanceMethod("segmentize", &segmentize), PrototypeAsyncableMethod("swapXY", swapXY),
      PrototypeAsyncableMethod("getEnvelope", getEnvelope), PrototypeAsyncableMethod("getEnvelope3D", getEnvelope3D),
      PrototypeAsyncableMethod("flattenTo2D", flattenTo2D), PrototypeAsyncableMethod("transform", transform),
      PrototypeAsyncableMethod("transformTo", transformTo),
#if GDAL_VERSION_MAJOR >= 3
      PrototypeAsyncableMethod("makeValid", makeValid),
#endif
      InstanceAccessor(
        "srs",
        &srsGetter,
        &srsSetter,
        static_cast<napi_property_attributes>(napi_writable | napi_configurable | napi_enumerable)),

      InstanceMethod<&wkbSizeGetter>(
        "wkbSize", static_cast<napi_property_attributes>(napi_writable | napi_configurable)),

      InstanceAccessor("dimension", &dimensionGetter, nullptr, napi_enumerable),
      InstanceAccessor(
        "coordinateDimension",
        &coordinateDimensionGetter,
        &coordinateDimensionSetter,
        static_cast<napi_property_attributes>(napi_writable | napi_configurable | napi_enumerable)),
      InstanceAccessor("wkbType", &Geometry::typeGetter, nullptr, napi_enumerable),
      InstanceAccessor("name", &Geometry::nameGetter, nullptr, napi_enumerable)
  });

  (target).Set(Napi::String::New(env, "Geometry"), Napi::GetFunction(lcons));

  constructor.Reset(lcons);
}

/**
 * Abstract base class for all geometry classes.
 *
 * @class Geometry
 */
Napi::Value Geometry::New(const Napi::CallbackInfo &info) {
  Geometry *f;

  if (!info.IsConstructCall()) {
    Napi::Error::New(env, "Cannot call constructor as function, you need to use 'new' keyword")
      .ThrowAsJavaScriptException();
    return env.Null();
  }

  if (info[0].IsExternal()) {
    Napi::External ext = info[0].As<Napi::External>();
    void *ptr = ext->Value();
    f = static_cast<Geometry *>(ptr);

  } else {
    Napi::ThrowError(
      "Geometry doesnt have a constructor, use Geometry.fromWKT(), Geometry.fromWKB() or type-specific constructor. ie. new ogr.Point()");
    return;
    // OGRwkbGeometryType geometry_type;
    // NODE_ARG_ENUM(0, "geometry type", OGRwkbGeometryType, geometry_type);
    // OGRGeometry *geom = OGRGeometryFactory::createGeometry(geometry_type);
    // f = new Geometry(geom);
  }

  f->Wrap(info.This());
  return info.This();
}

Napi::Value Geometry::New(OGRGeometry *geom, bool owned) {
  Napi::EscapableHandleScope scope(env);

  if (!geom) { return scope.Escape(env.Null()); }

  OGRwkbGeometryType type = getGeometryType_fixed(geom);
  type = wkbFlatten(type);

  switch (type) {
    case wkbPoint: return scope.Escape(Point::New(static_cast<OGRPoint *>(geom), owned));
    case wkbLineString: return scope.Escape(LineString::New(static_cast<OGRLineString *>(geom), owned));
    case wkbLinearRing: return scope.Escape(LinearRing::New(static_cast<OGRLinearRing *>(geom), owned));
    case wkbPolygon: return scope.Escape(Polygon::New(static_cast<OGRPolygon *>(geom), owned));
    case wkbGeometryCollection:
      return scope.Escape(GeometryCollection::New(static_cast<OGRGeometryCollection *>(geom), owned));
    case wkbMultiPoint: return scope.Escape(MultiPoint::New(static_cast<OGRMultiPoint *>(geom), owned));
    case wkbMultiLineString: return scope.Escape(MultiLineString::New(static_cast<OGRMultiLineString *>(geom), owned));
    case wkbMultiPolygon: return scope.Escape(MultiPolygon::New(static_cast<OGRMultiPolygon *>(geom), owned));
    case wkbCompoundCurve: return scope.Escape(CompoundCurve::New(static_cast<OGRCompoundCurve *>(geom), owned));
    case wkbCircularString: return scope.Escape(CircularString::New(static_cast<OGRCircularString *>(geom), owned));
    case wkbMultiCurve: return scope.Escape(MultiCurve::New(static_cast<OGRMultiCurve *>(geom), owned));
    default:
      Napi::Error::New(env, "Tried to create unsupported geometry type").ThrowAsJavaScriptException();
      return scope.Escape(env.Undefined());
  }
}

OGRwkbGeometryType Geometry::getGeometryType_fixed(OGRGeometry *geom) {
  // For some reason OGRLinearRing::getGeometryType uses OGRLineString's
  // method... meaning OGRLinearRing::getGeometryType returns wkbLineString

  // http://trac.osgeo.org/gdal/ticket/1755

  OGRwkbGeometryType type = geom->getGeometryType();

  if (std::string(geom->getGeometryName()) == "LINEARRING") {
    type = (OGRwkbGeometryType)(wkbLinearRing | (type & wkb25DBit));
  }

  return type;
}

Napi::Value Geometry::toString(const Napi::CallbackInfo &info) {
  Geometry *geom = info.This().Unwrap<Geometry>();
  std::ostringstream ss;
  ss << "Geometry (" << geom->this_->getGeometryName() << ")";
  return Napi::New(env, ss.str().c_str());
}

/**
 * Closes any un-closed rings.
 *
 * @method closeRings
 * @instance
 * @memberof Geometry
 */

/**
 * Closes any un-closed rings.
 * @async
 *
 * @method closeRingsAsync
 * @instance
 * @memberof Geometry
 * @param {callback<void>} [callback=undefined]
 * @return {Promise<void>}
 */

NODE_WRAPPED_ASYNC_METHOD(Geometry, closeRings, closeRings);

/**
 * Clears the geometry.
 *
 * @method empty
 * @instance
 * @memberof Geometry
 */

/**
 * Clears the geometry.
 * @async
 *
 * @method emptyAsync
 * @instance
 * @memberof Geometry
 * @param {callback<void>} [callback=undefined]
 * @return {Promise<void>}
 */

NODE_WRAPPED_ASYNC_METHOD(Geometry, empty, empty);

/**
 * Swaps x, y coordinates.
 *
 * @method swapXY
 * @instance
 * @memberof Geometry
 */

/**
 * Swaps x, y coordinates.
 * @async
 *
 * @method swapXYAsync
 * @instance
 * @memberof Geometry
 * @param {callback<void>} [callback=undefined]
 * @return {Promise<void>}
 */

NODE_WRAPPED_ASYNC_METHOD(Geometry, swapXY, swapXY);

/**
 * Determines if the geometry is empty.
 *
 * @method isEmpty
 * @instance
 * @memberof Geometry
 * @return {boolean}
 */

/**
 * Determines if the geometry is empty.
 * @async
 *
 * @method isEmptyAsync
 * @instance
 * @memberof Geometry
 * @param {callback<boolean>} [callback=undefined]
 * @return {Promise<boolean>}
 */

NODE_WRAPPED_ASYNC_METHOD_WITH_RESULT(Geometry, OGRBoolean, isEmpty, Boolean, IsEmpty);

/**
 * Determines if the geometry is valid.
 *
 * @method isValid
 * @instance
 * @memberof Geometry
 * @return {boolean}
 */

/**
 * Determines if the geometry is valid.
 * @async
 *
 * @method isValidAsync
 * @instance
 * @memberof Geometry
 * @param {callback<boolean>} [callback=undefined]
 * @return {Promise<boolean>}
 */

NODE_WRAPPED_ASYNC_METHOD_WITH_RESULT(Geometry, OGRBoolean, isValid, Boolean, IsValid);

/**
 * Determines if the geometry is simple.
 *
 * @method isSimple
 * @instance
 * @memberof Geometry
 * @return {boolean}
 */

/**
 * Determines if the geometry is simple.
 * @async
 *
 * @method isSimpleAsync
 * @instance
 * @memberof Geometry
 * @param {callback<boolean>} [callback=undefined]
 * @return {Promise<boolean>}
 */

NODE_WRAPPED_ASYNC_METHOD_WITH_RESULT(Geometry, OGRBoolean, isSimple, Boolean, IsSimple);

/**
 * Determines if the geometry is a ring.
 *
 * @method isRing
 * @instance
 * @memberof Geometry
 * @return {boolean}
 */

/**
 * Determines if the geometry is a ring.
 * @async
 *
 * @method isRingAsync
 * @instance
 * @memberof Geometry
 * @param {callback<boolean>} [callback=undefined]
 * @return {Promise<boolean>}
 */

NODE_WRAPPED_ASYNC_METHOD_WITH_RESULT(Geometry, OGRBoolean, isRing, Boolean, IsRing);

/**
 * Determines if the two geometries intersect.
 *
 * @method intersects
 * @instance
 * @memberof Geometry
 * @param {Geometry} geometry
 * @return {boolean}
 */

/**
 * Determines if the two geometries intersect.
 * @async
 *
 * @method intersectsAsync
 * @instance
 * @memberof Geometry
 * @param {Geometry} geometry
 * @param {callback<boolean>} [callback=undefined]
 * @return {Promise<boolean>}
 */

NODE_WRAPPED_ASYNC_METHOD_WITH_RESULT_1_WRAPPED_PARAM(
  Geometry, OGRBoolean, intersects, Boolean, Intersects, Geometry, "geometry to compare");

/**
 * Determines if the two geometries equal each other.
 *
 * @method equals
 * @instance
 * @memberof Geometry
 * @param {Geometry} geometry
 * @return {boolean}
 */

/**
 * Determines if the two geometries equal each other.
 * @async
 *
 * @method equalsAsync
 * @instance
 * @memberof Geometry
 * @param {Geometry} geometry
 * @param {callback<boolean>} [callback=undefined]
 * @return {Promise<boolean>}
 */

NODE_WRAPPED_ASYNC_METHOD_WITH_RESULT_1_WRAPPED_PARAM(
  Geometry, OGRBoolean, equals, Boolean, Equals, Geometry, "geometry to compare");

/**
 * Determines if the two geometries are disjoint.
 *
 * @method disjoint
 * @instance
 * @memberof Geometry
 * @param {Geometry} geometry
 * @return {boolean}
 */

/**
 * Determines if the two geometries are disjoint.
 * @async
 *
 * @method disjointAsync
 * @instance
 * @memberof Geometry
 * @param {Geometry} geometry
 * @param {callback<boolean>} [callback=undefined]
 * @return {Promise<boolean>}
 */

NODE_WRAPPED_ASYNC_METHOD_WITH_RESULT_1_WRAPPED_PARAM(
  Geometry, OGRBoolean, disjoint, Boolean, Disjoint, Geometry, "geometry to compare");

/**
 * Determines if the two geometries touch.
 *
 * @method touches
 * @instance
 * @memberof Geometry
 * @param {Geometry} geometry
 * @return {boolean}
 */

/**
 * Determines if the two geometries touch.
 * @async
 *
 * @method touchesAsync
 * @instance
 * @memberof Geometry
 * @param {Geometry} geometry
 * @param {callback<boolean>} [callback=undefined]
 * @return {Promise<boolean>}
 */

NODE_WRAPPED_ASYNC_METHOD_WITH_RESULT_1_WRAPPED_PARAM(
  Geometry, OGRBoolean, touches, Boolean, Touches, Geometry, "geometry to compare");

/**
 * Determines if the two geometries cross.
 *
 * @method crosses
 * @instance
 * @memberof Geometry
 * @param {Geometry} geometry
 * @return {boolean}
 */

/**
 * Determines if the two geometries cross.
 * @async
 *
 * @method crossesAsync
 * @instance
 * @memberof Geometry
 * @param {Geometry} geometry
 * @param {callback<boolean>} [callback=undefined]
 * @return {Promise<boolean>}
 */

NODE_WRAPPED_ASYNC_METHOD_WITH_RESULT_1_WRAPPED_PARAM(
  Geometry, OGRBoolean, crosses, Boolean, Crosses, Geometry, "geometry to compare");

/**
 * Determines if the current geometry is within the provided geometry.
 *
 * @method within
 * @instance
 * @memberof Geometry
 * @param {Geometry} geometry
 * @return {boolean}
 */

/**
 * Determines if the current geometry is within the provided geometry.
 * @async
 *
 * @method withinAsync
 * @instance
 * @memberof Geometry
 * @param {Geometry} geometry
 * @param {callback<boolean>} [callback=undefined]
 * @return {Promise<boolean>}
 */

NODE_WRAPPED_ASYNC_METHOD_WITH_RESULT_1_WRAPPED_PARAM(
  Geometry, OGRBoolean, within, Boolean, Within, Geometry, "geometry to compare");

/**
 * Determines if the current geometry contains the provided geometry.
 *
 * @method contains
 * @instance
 * @memberof Geometry
 * @param {Geometry} geometry
 * @return {boolean}
 */

/**
 * Determines if the current geometry contains the provided geometry.
 * @async
 *
 * @method containsAsync
 * @instance
 * @memberof Geometry
 * @param {Geometry} geometry
 * @param {callback<boolean>} [callback=undefined]
 * @return {Promise<boolean>}
 */

NODE_WRAPPED_ASYNC_METHOD_WITH_RESULT_1_WRAPPED_PARAM(
  Geometry, OGRBoolean, contains, Boolean, Contains, Geometry, "geometry to compare");

/**
 * Determines if the current geometry overlaps the provided geometry.
 *
 * @method overlaps
 * @instance
 * @memberof Geometry
 * @param {Geometry} geometry
 * @return {boolean}
 */

/**
 * Determines if the current geometry overlaps the provided geometry.
 * @async
 *
 * @method overlapsAsync
 * @instance
 * @memberof Geometry
 * @param {Geometry} geometry
 * @param {callback<boolean>} [callback=undefined]
 * @return {Promise<boolean>}
 */

NODE_WRAPPED_ASYNC_METHOD_WITH_RESULT_1_WRAPPED_PARAM(
  Geometry, OGRBoolean, overlaps, Boolean, Overlaps, Geometry, "geometry to compare");

/**
 * Computes the distance between the two geometries.
 *
 * @method distance
 * @instance
 * @memberof Geometry
 * @param {Geometry} geometry
 * @return {number}
 */

/**
 * Computes the distance between the two geometries.
 * @async
 *
 * @method distanceAsync
 * @instance
 * @memberof Geometry
 * @param {Geometry} geometry
 * @param {callback<number>} [callback=undefined]
 * @return {Promise<number>}
 */

NODE_WRAPPED_ASYNC_METHOD_WITH_RESULT_1_WRAPPED_PARAM(
  Geometry, double, distance, Number, Distance, Geometry, "geometry to use for distance calculation");

/**
 * Modify the geometry such it has no segment longer then the given distance.
 *
 * @method segmentize
 * @instance
 * @memberof Geometry
 * @param {number} segment_length
 * @return {void}
 */
NODE_WRAPPED_METHOD_WITH_1_DOUBLE_PARAM(Geometry, segmentize, segmentize, "segment length");

/**
 * Apply arbitrary coordinate transformation to the geometry.
 *
 * This method will transform the coordinates of a geometry from their current
 * spatial reference system to a new target spatial reference system. Normally
 * this means reprojecting the vectors, but it could include datum shifts,
 * and changes of units.
 *
 * Note that this method does not require that the geometry already have a
 * spatial reference system. It will be assumed that they can be treated as
 * having the source spatial reference system of the {@link CoordinateTransformation}
 * object, and the actual SRS of the geometry will be ignored.
 *
 * @throws {Error}
 * @method transform
 * @instance
 * @memberof Geometry
 * @param {CoordinateTransformation} transformation
 */

/**
 * Apply arbitrary coordinate transformation to the geometry.
 * @async
 *
 * @throws {Error}
 * @method transformAsync
 * @instance
 * @memberof Geometry
 * @param {CoordinateTransformation} transformation
 * @param {callback<void>} [callback=undefined]
 * @return {Promise<void>}
 */

NODE_WRAPPED_ASYNC_METHOD_WITH_OGRERR_RESULT_1_WRAPPED_PARAM(
  Geometry, int, transform, transform, CoordinateTransformation, "transform");

/**
 * Transforms the geometry to match the provided {@link SpatialReference}
 *
 * @throws {Error}
 * @method transformTo
 * @instance
 * @memberof Geometry
 * @param {SpatialReference} srs
 */

/**
 * Transforms the geometry to match the provided {@link SpatialReference}
 * @async
 *
 * @throws {Error}
 * @method transformToAsync
 * @instance
 * @memberof Geometry
 * @param {SpatialReference} srs
 * @param {callback<void>} [callback=undefined]
 * @return {Promise<void>}
 */

NODE_WRAPPED_ASYNC_METHOD_WITH_OGRERR_RESULT_1_WRAPPED_PARAM(
  Geometry, int, transformTo, transformTo, SpatialReference, "spatial reference");

/**
 * Clones the instance.
 *
 * @method clone
 * @instance
 * @memberof Geometry
 * @return {Geometry}
 */
Napi::Value Geometry::clone(const Napi::CallbackInfo &info) {
  Geometry *geom = info.This().Unwrap<Geometry>();
  return Geometry::New(geom->this_->clone());
}

/**
 * Compute convex hull.
 *
 * @method convexHull
 * @instance
 * @memberof Geometry
 * @throws {Error}
 * @return {Geometry}
 */

/**
 * Compute convex hull.
 * @async
 *
 * @method convexHullAsync
 * @instance
 * @memberof Geometry
 * @throws {Error}
 * @param {callback<Geometry>} [callback=undefined]
 * @return {Promise<Geometry>}
 */

GDAL_ASYNCABLE_DEFINE(Geometry::convexHull) {
  Geometry *geom = info.This().Unwrap<Geometry>();
  OGRGeometry *gdal_geom = geom->this_;
  GDALAsyncableJob<OGRGeometry *> job(0);
  job.main = [gdal_geom](const GDALExecutionProgress &) {
    CPLErrorReset();
    auto r = gdal_geom->ConvexHull();
    if (r == nullptr) throw CPLGetLastErrorMsg();
    return r;
  };
  job.rval = [](OGRGeometry *r, const GetFromPersistentFunc &) { return Geometry::New(r); };
  job.run(info, async, 0);
}

/**
 * Compute boundary.
 *
 * @method boundary
 * @instance
 * @memberof Geometry
 * @throws {Error}
 * @return {Geometry}
 */

/**
 * Compute boundary.
 * @async
 *
 * @method boundaryAsync
 * @instance
 * @memberof Geometry
 * @throws {Error}
 * @param {callback<Geometry>} [callback=undefined]
 * @return {Promise<Geometry>}
 */

GDAL_ASYNCABLE_DEFINE(Geometry::boundary) {
  Geometry *geom = info.This().Unwrap<Geometry>();
  OGRGeometry *gdal_geom = geom->this_;
  GDALAsyncableJob<OGRGeometry *> job(0);
  job.main = [gdal_geom](const GDALExecutionProgress &) {
    CPLErrorReset();
    auto r = gdal_geom->Boundary();
    if (r == nullptr) throw CPLGetLastErrorMsg();
    return r;
  };
  job.rval = [](OGRGeometry *r, const GetFromPersistentFunc &) { return Geometry::New(r); };
  job.run(info, async, 0);
}

/**
 * Compute intersection with another geometry.
 *
 * @method intersection
 * @instance
 * @memberof Geometry
 * @param {Geometry} geometry
 * @throws {Error}
 * @return {Geometry}
 */

/**
 * Compute intersection with another geometry.
 * @async
 *
 * @method intersectionAsync
 * @instance
 * @memberof Geometry
 * @param {Geometry} geometry
 * @param {callback<Geometry>} [callback=undefined]
 * @throws {Error}
 * @return {Promise<Geometry>}
 */

GDAL_ASYNCABLE_DEFINE(Geometry::intersection) {
  Geometry *geom = info.This().Unwrap<Geometry>();
  Geometry *x = NULL;

  NODE_ARG_WRAPPED(0, "geometry to use for intersection", Geometry, x);

  OGRGeometry *gdal_geom = geom->this_;
  OGRGeometry *gdal_x = x->this_;
  GDALAsyncableJob<OGRGeometry *> job(0);
  job.main = [gdal_geom, gdal_x](const GDALExecutionProgress &) {
    CPLErrorReset();
    auto r = gdal_geom->Intersection(gdal_x);
    if (r == nullptr) throw CPLGetLastErrorMsg();
    return r;
  };
  job.rval = [](OGRGeometry *r, const GetFromPersistentFunc &) { return Geometry::New(r); };
  job.run(info, async, 1);
}

/**
 * Compute the union of this geometry with another.
 *
 * @method union
 * @instance
 * @memberof Geometry
 * @param {Geometry} geometry
 * @throws {Error}
 * @return {Geometry}
 */

/**
 * Compute the union of this geometry with another.
 * @async
 *
 * @method unionAsync
 * @instance
 * @memberof Geometry
 * @param {Geometry} geometry
 * @param {callback<Geometry>} [callback=undefined]
 * @throws {Error}
 * @return {Promise<Geometry>}
 */

GDAL_ASYNCABLE_DEFINE(Geometry::unionGeometry) {
  Geometry *geom = info.This().Unwrap<Geometry>();
  Geometry *x = NULL;

  NODE_ARG_WRAPPED(0, "geometry to use for union", Geometry, x);

  OGRGeometry *gdal_geom = geom->this_;
  OGRGeometry *gdal_x = x->this_;
  GDALAsyncableJob<OGRGeometry *> job(0);
  job.main = [gdal_geom, gdal_x](const GDALExecutionProgress &) {
    CPLErrorReset();
    auto r = gdal_geom->Union(gdal_x);
    if (r == nullptr) throw CPLGetLastErrorMsg();
    return r;
  };
  job.rval = [](OGRGeometry *r, const GetFromPersistentFunc &) { return Geometry::New(r); };
  job.run(info, async, 1);
}

/**
 * Compute the difference of this geometry with another.
 *
 * @method difference
 * @instance
 * @memberof Geometry
 * @param {Geometry} geometry
 * @throws {Error}
 * @return {Geometry}
 */

/**
 * Compute the difference of this geometry with another.
 * @async
 *
 * @method differenceAsync
 * @instance
 * @memberof Geometry
 * @param {Geometry} geometry
 * @param {callback<Geometry>} [callback=undefined]
 * @throws {Error}
 * @return {Promise<Geometry>}
 */

GDAL_ASYNCABLE_DEFINE(Geometry::difference) {
  Geometry *geom = info.This().Unwrap<Geometry>();
  Geometry *x = NULL;

  NODE_ARG_WRAPPED(0, "geometry to use for difference", Geometry, x);

  OGRGeometry *gdal_geom = geom->this_;
  OGRGeometry *gdal_x = x->this_;
  GDALAsyncableJob<OGRGeometry *> job(0);
  job.main = [gdal_geom, gdal_x](const GDALExecutionProgress &) {
    CPLErrorReset();
    auto r = gdal_geom->Difference(gdal_x);
    if (r == nullptr) throw CPLGetLastErrorMsg();
    return r;
  };
  job.rval = [](OGRGeometry *r, const GetFromPersistentFunc &) { return Geometry::New(r); };
  job.run(info, async, 1);
}

/**
 * Computes the symmetric difference of this geometry and the second geometry.
 *
 * @method symDifference
 * @instance
 * @memberof Geometry
 * @param {Geometry} geometry
 * @throws {Error}
 * @return {Geometry}
 */

/**
 * Computes the symmetric difference of this geometry and the second geometry.
 * @async
 *
 * @method symDifferenceAsync
 * @instance
 * @memberof Geometry
 * @param {Geometry} geometry
 * @param {callback<Geometry>} [callback=undefined]
 * @throws {Error}
 * @return {Promise<Geometry>}
 */

GDAL_ASYNCABLE_DEFINE(Geometry::symDifference) {
  Geometry *geom = info.This().Unwrap<Geometry>();
  Geometry *x = NULL;

  NODE_ARG_WRAPPED(0, "geometry to use for symDifference", Geometry, x);

  OGRGeometry *gdal_geom = geom->this_;
  OGRGeometry *gdal_x = x->this_;
  GDALAsyncableJob<OGRGeometry *> job(0);
  job.main = [gdal_geom, gdal_x](const GDALExecutionProgress &) {
    CPLErrorReset();
    auto r = gdal_geom->SymDifference(gdal_x);
    if (r == nullptr) throw CPLGetLastErrorMsg();
    return r;
  };
  job.rval = [](OGRGeometry *r, const GetFromPersistentFunc &) { return Geometry::New(r); };
  job.run(info, async, 1);
}

/**
 * Reduces the geometry complexity.
 *
 * @method simplify
 * @instance
 * @memberof Geometry
 * @param {number} tolerance
 * @throws {Error}
 * @return {Geometry}
 */

/**
 * Reduces the geometry complexity.
 * @async
 *
 * @method simplifyAsync
 * @instance
 * @memberof Geometry
 * @param {number} tolerance
 * @param {callback<Geometry>} [callback=undefined]
 * @throws {Error}
 * @return {Promise<Geometry>}
 */

GDAL_ASYNCABLE_DEFINE(Geometry::simplify) {
  Geometry *geom = info.This().Unwrap<Geometry>();
  double tolerance;

  NODE_ARG_DOUBLE(0, "tolerance", tolerance);
  OGRGeometry *gdal_geom = geom->this_;

  GDALAsyncableJob<OGRGeometry *> job(0);
  job.main = [gdal_geom, tolerance](const GDALExecutionProgress &) {
    CPLErrorReset();
    auto r = gdal_geom->Simplify(tolerance);
    if (r == nullptr) throw CPLGetLastErrorMsg();
    return r;
  };
  job.rval = [](OGRGeometry *r, const GetFromPersistentFunc &) { return Geometry::New(r); };
  job.run(info, async, 1);
}

/**
 * Reduces the geometry complexity while preserving the topology.
 *
 * @method simplifyPreserveTopology
 * @instance
 * @memberof Geometry
 * @param {number} tolerance
 * @throws {Error}
 * @return {Geometry}
 */

/**
 * Reduces the geometry complexity while preserving the topology.
 * @async
 *
 * @method simplifyPreserveTopologyAsync
 * @instance
 * @memberof Geometry
 * @param {number} tolerance
 * @param {callback<Geometry>} [callback=undefined]
 * @throws {Error}
 * @return {Promise<Geometry>}
 */

GDAL_ASYNCABLE_DEFINE(Geometry::simplifyPreserveTopology) {
  Geometry *geom = info.This().Unwrap<Geometry>();
  double tolerance;

  NODE_ARG_DOUBLE(0, "tolerance", tolerance);
  OGRGeometry *gdal_geom = geom->this_;

  GDALAsyncableJob<OGRGeometry *> job(0);
  job.main = [gdal_geom, tolerance](const GDALExecutionProgress &) {
    CPLErrorReset();
    auto r = gdal_geom->SimplifyPreserveTopology(tolerance);
    if (r == nullptr) throw CPLGetLastErrorMsg();
    return r;
  };
  job.rval = [](OGRGeometry *r, const GetFromPersistentFunc &) { return Geometry::New(r); };
  job.run(info, async, 1);
}

/**
 * Buffers the geometry by the given distance.
 *
 * @method buffer
 * @instance
 * @memberof Geometry
 * @param {number} distance
 * @param {number} segments
 * @throws {Error}
 * @return {Geometry}
 */

/**
 * Buffers the geometry by the given distance.
 * @async
 *
 * @method bufferAsync
 * @instance
 * @memberof Geometry
 * @param {number} distance
 * @param {number} segments
 * @param {callback<Geometry>} [callback=undefined]
 * @throws {Error}
 * @return {Promise<Geometry>}
 */

GDAL_ASYNCABLE_DEFINE(Geometry::buffer) {
  double distance;
  int number_of_segments = 30;

  NODE_ARG_DOUBLE(0, "distance", distance);
  NODE_ARG_INT_OPT(1, "number of segments", number_of_segments);

  Geometry *geom = info.This().Unwrap<Geometry>();

  OGRGeometry *gdal_geom = geom->this_;

  GDALAsyncableJob<OGRGeometry *> job(0);
  job.main = [gdal_geom, distance, number_of_segments](const GDALExecutionProgress &) {
    CPLErrorReset();
    auto r = gdal_geom->Buffer(distance, number_of_segments);
    if (r == nullptr) throw CPLGetLastErrorMsg();
    return r;
  };
  job.rval = [](OGRGeometry *r, const GetFromPersistentFunc &) { return Geometry::New(r); };
  job.run(info, async, 2);
}

#if GDAL_VERSION_MAJOR >= 3
/**
 * Attempts to make an invalid geometry valid without losing vertices.
 * Requires GDAL 3.0
 *
 * @method makeValid
 * @instance
 * @memberof Geometry
 * @throws {Error}
 * @return {Geometry}
 */

/**
 * Attempts to make an invalid geometry valid without losing vertices.
 * Requires GDAL 3.0
 * @async
 *
 * @method makeValidAsync
 * @instance
 * @memberof Geometry
 * @param {callback<Geometry>} [callback=undefined]
 * @return {Promise<Geometry>}
 */

GDAL_ASYNCABLE_DEFINE(Geometry::makeValid) {
  Geometry *geom = info.This().Unwrap<Geometry>();
  OGRGeometry *gdal_geom = geom->this_;
  GDALAsyncableJob<OGRGeometry *> job(0);
  job.main = [gdal_geom](const GDALExecutionProgress &) {
    CPLErrorReset();
    auto r = gdal_geom->MakeValid();
    if (r == nullptr) throw CPLGetLastErrorMsg();
    return r;
  };
  job.rval = [](OGRGeometry *r, const GetFromPersistentFunc &) { return Geometry::New(r); };
  job.run(info, async, 0);
}
#endif

/**
 * Convert a geometry into well known text format.
 *
 * @method toWKT
 * @instance
 * @memberof Geometry
 * @throws {Error}
 * @return {string}
 */

/**
 * Convert a geometry into well known text format.
 * @async
 *
 * @method toWKTAsync
 * @instance
 * @memberof Geometry
 * @param {callback<string>} [callback=undefined]
 * @throws {Error}
 * @return {Promise<string>}
 */

GDAL_ASYNCABLE_DEFINE(Geometry::exportToWKT) {
  Geometry *geom = info.This().Unwrap<Geometry>();

  OGRGeometry *gdal_geom = geom->this_;
  uv_sem_t *async_lock = geom->async_lock;
  GDALAsyncableJob<char *> job(0);
  job.main = [async_lock, gdal_geom](const GDALExecutionProgress &) {
    char *text = NULL;
    uv_sem_wait(async_lock);
    OGRErr err = gdal_geom->exportToWkt(&text);
    uv_sem_post(async_lock);
    if (err) { throw getOGRErrMsg(err); }
    return text;
  };

  job.rval = [](char *text, const GetFromPersistentFunc &) {
    if (text) {
      auto r = SafeString::New(text);
      CPLFree(text);
      return r;
    }
    return env.Undefined().As<Napi::Value>();
  };

  job.run(info, async, 0);
}

/**
 * Convert a geometry into well known binary format.
 *
 * @method toWKB
 * @instance
 * @memberof Geometry
 * @param {string} [byte_order="MSB"] {@link wkbByteOrder|see options}
 * @param {string} [variant="OGC"] ({@link wkbVariant|see options})
 * @throws {Error}
 * @return {Buffer}
 */

/**
 * Convert a geometry into well known binary format.
 * @async
 *
 * @method toWKBAsync
 * @instance
 * @memberof Geometry
 * @param {string} [byte_order="MSB"] {@link wkbByteOrder|see options}
 * @param {string} [variant="OGC"] ({@link wkbVariant|see options})
 * @param {callback<Buffer>} [callback=undefined]
 * @throws {Error}
 * @return {Promise<Buffer>}
 */

GDAL_ASYNCABLE_DEFINE(Geometry::exportToWKB) {
  Geometry *geom = info.This().Unwrap<Geometry>();

  size_t size = geom->this_->WkbSize();

  // byte order
  OGRwkbByteOrder byte_order;
  std::string order = "MSB";
  NODE_ARG_OPT_STR(0, "byte order", order);
  if (order == "MSB") {
    byte_order = wkbXDR;
  } else if (order == "LSB") {
    byte_order = wkbNDR;
  } else {
    Napi::Error::New(env, "byte order must be 'MSB' or 'LSB'").ThrowAsJavaScriptException();
    return env.Null();
  }

  // wkb variant
  OGRwkbVariant wkb_variant;
  std::string variant = "OGC";
  NODE_ARG_OPT_STR(1, "wkb variant", variant);
  if (variant == "OGC") {
    wkb_variant = wkbVariantOldOgc;
  } else if (variant == "ISO") {
    wkb_variant = wkbVariantIso;
  } else {
    Napi::Error::New(env, "variant must be 'OGC' or 'ISO'").ThrowAsJavaScriptException();
    return env.Null();
  }

  unsigned char *data = (unsigned char *)malloc(size);
  if (data == nullptr) {
    Napi::Error::New(env, "Failed allocating memory").ThrowAsJavaScriptException();
    return env.Null();
  }

  OGRGeometry *gdal_geom = geom->this_;
  uv_sem_t *async_lock = geom->async_lock;
  GDALAsyncableJob<unsigned char *> job(0);
  job.main = [async_lock, gdal_geom, data, byte_order, wkb_variant](const GDALExecutionProgress &) {
    uv_sem_wait(async_lock);
    OGRErr err = gdal_geom->exportToWkb(byte_order, data, wkb_variant);
    uv_sem_post(async_lock);
    if (err) {
      free(data);
      throw getOGRErrMsg(err);
    }
    return data;
  };

  Napi::AdjustExternalMemory(size);

  job.rval = [size](unsigned char *data, const GetFromPersistentFunc &) {
    Napi::EscapableHandleScope scope(env);
    int *hint = new int{static_cast<int>(size)};
    Napi::Value result = Napi::Buffer<char>::New(
      env,
      reinterpret_cast<char *>(data),
      size,
      [](char *data, void *hint) {
        int *size = reinterpret_cast<int *>(hint);
        Napi::AdjustExternalMemory(-(*size));
        delete size;
        free(data);
      },
      hint);
    return scope.Escape(result);
  };
  job.run(info, async, 2);
}

/**
 * Convert a geometry into KML format.
 *
 * @method toKML
 * @instance
 * @memberof Geometry
 * @throws {Error}
 * @return {string}
 */

/**
 * Convert a geometry into KML format.
 * @async
 *
 * @method toKMLAsync
 * @instance
 * @memberof Geometry
 * @param {callback<string>} [callback=undefined]
 * @throws {Error}
 * @return {Promise<string>}
 */

GDAL_ASYNCABLE_DEFINE(Geometry::exportToKML) {
  Geometry *geom = info.This().Unwrap<Geometry>();

  OGRGeometry *gdal_geom = geom->this_;
  uv_sem_t *async_lock = geom->async_lock;
  GDALAsyncableJob<char *> job(0);
  job.main = [async_lock, gdal_geom](const GDALExecutionProgress &) {
    CPLErrorReset();
    uv_sem_wait(async_lock);
    char *text = gdal_geom->exportToKML();
    uv_sem_post(async_lock);
    if (text == nullptr) throw CPLGetLastErrorMsg();
    return text;
  };
  job.rval = [](char *text, const GetFromPersistentFunc &) {
    if (text) {
      Napi::Value result = Napi::New(env, text);
      CPLFree(text);
      return result;
    }
    return env.Undefined().As<Napi::Value>();
  };
  job.run(info, async, 0);
}

/**
 * Convert a geometry into GML format.
 *
 * @method toGML
 * @instance
 * @memberof Geometry
 * @throws {Error}
 * @return {string}
 */

/**
 * Convert a geometry into GML format.
 * @async
 *
 * @method toGMLAsync
 * @instance
 * @memberof Geometry
 * @param {callback<string>} [callback=undefined]
 * @throws {Error}
 * @return {Promise<string>}
 */

GDAL_ASYNCABLE_DEFINE(Geometry::exportToGML) {
  Geometry *geom = info.This().Unwrap<Geometry>();

  OGRGeometry *gdal_geom = geom->this_;
  uv_sem_t *async_lock = geom->async_lock;
  GDALAsyncableJob<char *> job(0);
  job.main = [async_lock, gdal_geom](const GDALExecutionProgress &) {
    CPLErrorReset();
    uv_sem_wait(async_lock);
    char *text = gdal_geom->exportToGML();
    uv_sem_post(async_lock);
    if (text == nullptr) throw CPLGetLastErrorMsg();
    return text;
  };
  job.rval = [](char *text, const GetFromPersistentFunc &) {
    if (text) {
      Napi::Value result = Napi::New(env, text);
      CPLFree(text);
      return result;
    }
    return env.Undefined().As<Napi::Value>();
  };
  job.run(info, async, 0);
}

/**
 * Convert a geometry into JSON format.
 *
 * @method toJSON
 * @instance
 * @memberof Geometry
 * @throws {Error}
 * @return {string}
 */

/**
 * Convert a geometry into JSON format.
 * @async
 *
 * @method toJSONAsync
 * @instance
 * @memberof Geometry
 * @param {callback<string>} [callback=undefined]
 * @throws {Error}
 * @return {Promise<string>}
 */

GDAL_ASYNCABLE_DEFINE(Geometry::exportToJSON) {
  Geometry *geom = info.This().Unwrap<Geometry>();

  OGRGeometry *gdal_geom = geom->this_;
  uv_sem_t *async_lock = geom->async_lock;
  GDALAsyncableJob<char *> job(0);
  job.main = [async_lock, gdal_geom](const GDALExecutionProgress &) {
    CPLErrorReset();
    uv_sem_wait(async_lock);
    char *text = gdal_geom->exportToJson();
    uv_sem_post(async_lock);
    if (text == nullptr) throw CPLGetLastErrorMsg();
    return text;
  };
  job.rval = [](char *text, const GetFromPersistentFunc &) {
    if (text) {
      Napi::Value result = Napi::New(env, text);
      CPLFree(text);
      return result;
    }
    return env.Undefined().As<Napi::Value>();
  };
  job.run(info, async, 0);
}

/**
 * Compute the centroid of the geometry.
 *
 * @method centroid
 * @instance
 * @memberof Geometry
 * @throws {Error}
 * @return {Point}
 */

/**
 * Compute the centroid of the geometry.
 * @async
 *
 * @method centroidAsync
 * @instance
 * @memberof Geometry
 * @param {callback<Geometry>} [callback=undefined]
 * @throws {Error}
 * @return {Promise<Geometry>}
 */

GDAL_ASYNCABLE_DEFINE(Geometry::centroid) {
  // The Centroid method wants the caller to create the point to fill in.
  // Instead of requiring the caller to create the point geometry to fill in, we
  // new up an OGRPoint and put the result into it and return that.
  Napi::HandleScope scope(env);

  Geometry *geom = info.This().Unwrap<Geometry>();

  OGRGeometry *gdal_geom = geom->this_;
  uv_sem_t *async_lock = geom->async_lock;
  GDALAsyncableJob<OGRPoint *> job(0);
  job.main = [async_lock, gdal_geom](const GDALExecutionProgress &) {
    OGRPoint *point = new OGRPoint();
    uv_sem_wait(async_lock);
    OGRErr err = gdal_geom->Centroid(point);
    uv_sem_post(async_lock);
    if (err) {
      delete point;
      throw getOGRErrMsg(err);
    }
    return point;
  };
  job.rval = [](OGRPoint *point, const GetFromPersistentFunc &) { return Point::New(point); };
  job.run(info, async, 0);
}

/**
 * Computes the bounding box (envelope).
 *
 * @method getEnvelope
 * @instance
 * @memberof Geometry
 * @return {Envelope} Bounding envelope
 */

/**
 * Computes the bounding box (envelope).
 * @async
 *
 * @method getEnvelopeAsync
 * @instance
 * @memberof Geometry
 * @param {callback<Envelope>} [callback=undefined]
 * @return {Promise<Envelope>}
 */
GDAL_ASYNCABLE_DEFINE(Geometry::getEnvelope) {
  // returns object containing boundaries until complete OGREnvelope binding is
  // built

  Napi::HandleScope scope(env);

  Geometry *geom = info.This().Unwrap<Geometry>();

  OGRGeometry *gdal_geom = geom->this_;
  uv_sem_t *async_lock = geom->async_lock;

  GDALAsyncableJob<OGREnvelope *> job(0);
  job.main = [async_lock, gdal_geom](const GDALExecutionProgress &) {
    OGREnvelope *envelope = new OGREnvelope();
    uv_sem_wait(async_lock);
    gdal_geom->getEnvelope(envelope);
    uv_sem_post(async_lock);
    return envelope;
  };

  job.rval = [](OGREnvelope *envelope, const GetFromPersistentFunc &) {
    Napi::Object obj = Napi::Object::New(env);
    (obj).Set(Napi::String::New(env, "minX"), Napi::Number::New(env, envelope->MinX));
    (obj).Set(Napi::String::New(env, "maxX"), Napi::Number::New(env, envelope->MaxX));
    (obj).Set(Napi::String::New(env, "minY"), Napi::Number::New(env, envelope->MinY));
    (obj).Set(Napi::String::New(env, "maxY"), Napi::Number::New(env, envelope->MaxY));
    delete envelope;
    return obj;
  };
  job.run(info, async, 0);
}

/**
 * Computes the 3D bounding box (envelope).
 *
 * @method getEnvelope3D
 * @instance
 * @memberof Geometry
 * @return {Envelope3D} Bounding envelope
 */

/**
 * Computes the 3D bounding box (envelope).
 * @async
 *
 * @method getEnvelope3DAsync
 * @instance
 * @memberof Geometry
 * @param {callback<Geometry>} [callback=undefined]
 * @return {Promise<Geometry>}
 */

GDAL_ASYNCABLE_DEFINE(Geometry::getEnvelope3D) {
  // returns object containing boundaries until complete OGREnvelope binding is
  // built

  Napi::HandleScope scope(env);

  Geometry *geom = info.This().Unwrap<Geometry>();

  OGRGeometry *gdal_geom = geom->this_;
  uv_sem_t *async_lock = geom->async_lock;

  GDALAsyncableJob<OGREnvelope3D *> job(0);
  job.main = [async_lock, gdal_geom](const GDALExecutionProgress &) {
    OGREnvelope3D *envelope = new OGREnvelope3D();
    uv_sem_wait(async_lock);
    gdal_geom->getEnvelope(envelope);
    uv_sem_post(async_lock);
    return envelope;
  };

  job.rval = [](OGREnvelope3D *envelope, const GetFromPersistentFunc &) {
    Napi::Object obj = Napi::Object::New(env);
    obj.Set(Napi::String::New(env, "minX"), Napi::Number::New(env, envelope->MinX));
    obj.Set(Napi::String::New(env, "maxX"), Napi::Number::New(env, envelope->MaxX));
    obj.Set(Napi::String::New(env, "minY"), Napi::Number::New(env, envelope->MinY));
    obj.Set(Napi::String::New(env, "maxY"), Napi::Number::New(env, envelope->MaxY));
    obj.Set(Napi::String::New(env, "minZ"), Napi::Number::New(env, envelope->MinZ));
    obj.Set(Napi::String::New(env, "maxZ"), Napi::Number::New(env, envelope->MaxZ));
    delete envelope;
    return obj;
  };
  job.run(info, async, 0);
}

/**
 * Convert geometry to strictly 2D.
 *
 * @method flattenTo2D
 * @instance
 * @memberof Geometry
 * @return {void}
 */

/**
 * Convert geometry to strictly 2D.
 * @async
 *
 * @method flattenTo2DAsync
 * @instance
 * @memberof Geometry
 * @param {callback<void>} [callback=undefined]
 * @return {Promise<void>}
 */

NODE_WRAPPED_ASYNC_METHOD(Geometry, flattenTo2D, flattenTo2D);

// --- JS static methods (OGRGeometryFactory) ---

/**
 * Creates a Geometry from a WKT string.
 *
 * @static
 * @method fromWKT
 * @instance
 * @memberof Geometry
 * @throws {Error}
 * @param {string} wkt
 * @param {SpatialReference} [srs]
 * @return {Geometry}
 */

/**
 * Creates a Geometry from a WKT string.
 * @async
 *
 * @static
 * @method fromWKTAsync
 * @instance
 * @memberof Geometry
 * @throws {Error}
 * @param {string} wkt
 * @param {SpatialReference} [srs]
 * @param {callback<Geometry>} [callback=undefined]
 * @return {Promise<Geometry>}
 */

GDAL_ASYNCABLE_DEFINE(Geometry::createFromWkt) {
  std::string *wkt_string = new std::string;
  SpatialReference *srs = NULL;

  NODE_ARG_STR(0, "wkt", *wkt_string);
  NODE_ARG_WRAPPED_OPT(1, "srs", SpatialReference, srs);

  OGRSpatialReference *ogr_srs = NULL;
  if (srs) { ogr_srs = srs->get(); }

  GDALAsyncableJob<OGRGeometry *> job(0);
  job.main = [wkt_string, ogr_srs](const GDALExecutionProgress &) {
    std::unique_ptr<std::string> wkt_string_ptr(wkt_string);
    OGRGeometry *geom = NULL;
    OGRChar *wkt = (OGRChar *)wkt_string->c_str();
    OGRErr err = OGRGeometryFactory::createFromWkt(&wkt, ogr_srs, &geom);
    if (err) throw getOGRErrMsg(err);
    return geom;
  };
  job.rval = [](OGRGeometry *geom, const GetFromPersistentFunc &) { return Geometry::New(geom, true); };
  job.run(info, async, 2);
}

/**
 * Creates a Geometry from a WKB buffer.
 *
 * @static
 * @method fromWKB
 * @instance
 * @memberof Geometry
 * @throws {Error}
 * @param {Buffer} wkb
 * @param {SpatialReference} [srs]
 * @return {Geometry}
 */

/**
 * Creates a Geometry from a WKB buffer.
 * @async
 *
 * @static
 * @method fromWKBAsync
 * @instance
 * @memberof Geometry
 * @throws {Error}
 * @param {Buffer} wkb
 * @param {SpatialReference} [srs]
 * @param {callback<Geometry>} [callback=undefined]
 * @return {Promise<Geometry>}
 */

GDAL_ASYNCABLE_DEFINE(Geometry::createFromWkb) {
  std::string wkb_string;
  SpatialReference *srs = NULL;

  Napi::Object wkb_obj;
  NODE_ARG_OBJECT(0, "wkb", wkb_obj);
  NODE_ARG_WRAPPED_OPT(1, "srs", SpatialReference, srs);

  std::string obj_type = wkb_obj->GetConstructorName(.As<Napi::String>().Utf8Value().c_str());

  if (obj_type != "Buffer" && obj_type != "Uint8Array") {
    Napi::Error::New(env, "Argument must be a buffer object").ThrowAsJavaScriptException();
    return env.Null();
  }

  unsigned char *data = (unsigned char *)Buffer::Data(wkb_obj);
  size_t length = Buffer::Length(wkb_obj);

  OGRSpatialReference *ogr_srs = NULL;
  if (srs) { ogr_srs = srs->get(); }

  GDALAsyncableJob<OGRGeometry *> job(0);
  job.main = [data, length, ogr_srs](const GDALExecutionProgress &) {
    OGRGeometry *geom = NULL;
    OGRErr err = OGRGeometryFactory::createFromWkb(data, ogr_srs, &geom, length);
    if (err) throw getOGRErrMsg(err);
    return geom;
  };
  job.rval = [](OGRGeometry *geom, const GetFromPersistentFunc &) { return Geometry::New(geom, true); };
  job.run(info, async, 2);
}

/**
 * Creates a Geometry from a GeoJSON object fragment.
 * The async version depends on V8 for object serialization and this part is not parallelizable.
 * V8 objects cannot be accessed outside of the main thread. This function should not be used
 * for importing objects of more than few tens of Kbytes when low latency is needed. If you need
 * to import very large GeoJSON geometries in server code, use the much faster and completely
 * parallel fromGeoJonBuffer(Async)
 *
 * @static
 * @method fromGeoJson
 * @instance
 * @memberof Geometry
 * @throws {Error}
 * @param {object} geojson
 * @return {Geometry}
 */

/**
 * Creates a Geometry from a GeoJSON object fragment.
 * The async version depends on V8 for object serialization and this part is not parallelizable.
 * V8 objects cannot be accessed outside of the main thread. This function should not be used
 * for importing objects of more than few tens of Kbytes when low latency is needed. If you need
 * to import very large GeoJSON geometries in server code, use the much faster and completely
 * parallel fromGeoJonBuffer(Async)
 * @async
 *
 * @static
 * @method fromGeoJsonAsync
 * @instance
 * @memberof Geometry
 * @throws {Error}
 * @param {object} geojson
 * @param {callback<Geometry>} [callback=undefined]
 * @return {Promise<Geometry>}
 */
GDAL_ASYNCABLE_DEFINE(Geometry::createFromGeoJson) {
#if GDAL_VERSION_MAJOR == 2 && GDAL_VERSION_MINOR < 3
  Napi::Error::New(env, "GDAL < 2.3 does not support parsing GeoJSON directly").ThrowAsJavaScriptException();
  return env.Null();
#else
  Napi::Object geo_obj;
  NODE_ARG_OBJECT(0, "geojson", geo_obj);

  // goes to text to pass it in, there isn't a performant way to
  // go from v8 JSON -> CPLJSON anyways
  Napi::JSON NanJSON;
  Napi::MaybeNapi::String result = NanJSON.Stringify(geo_obj);
  if (result.IsEmpty()) {
    Napi::Error::New(env, "Invalid GeoJSON").ThrowAsJavaScriptException();
    return env.Null();
  }
  Napi::String stringified = result;
  std::string *val = new std::string(stringified.As<Napi::String>().Utf8Value().c_str());

  GDALAsyncableJob<OGRGeometry *> job(0);
  job.main = [val](const GDALExecutionProgress &) {
    CPLErrorReset();
    std::unique_ptr<std::string> val_ptr(val);
    OGRGeometry *geom = OGRGeometryFactory::createFromGeoJson(val->c_str());
    if (geom == nullptr) throw CPLGetLastErrorMsg();
    return geom;
  };
  job.rval = [](OGRGeometry *geom, const GetFromPersistentFunc &) { return Geometry::New(geom, true); };
  job.run(info, async, 1);
#endif
}

/**
 * Creates a Geometry from a buffer containing a GeoJSON fragment in UT8 format.
 *
 * @static
 * @method fromGeoJsonBuffer
 * @instance
 * @memberof Geometry
 * @throws {Error}
 * @param {Buffer} geojson
 * @return {Geometry}
 */

/**
 * Creates a Geometry from a buffer containing a GeoJSON fragment in UT8 format.
 * @async
 *
 * @static
 * @method fromGeoJsonBufferAsync
 * @instance
 * @memberof Geometry
 * @throws {Error}
 * @param {Buffer} geojson
 * @param {callback<Geometry>} [callback=undefined]
 * @return {Promise<Geometry>}
 */

GDAL_ASYNCABLE_DEFINE(Geometry::createFromGeoJsonBuffer) {
#if GDAL_VERSION_MAJOR == 2 && GDAL_VERSION_MINOR < 3
  Napi::Error::New(env, "GDAL < 2.3 does not support parsing GeoJSON directly").ThrowAsJavaScriptException();
  return env.Null();
#else
  std::string geojson_string;

  Napi::Object geojson_obj;
  NODE_ARG_OBJECT(0, "geojson", geojson_obj);

  std::string obj_type = geojson_obj->GetConstructorName(.As<Napi::String>().Utf8Value().c_str());

  if (obj_type != "Buffer" && obj_type != "Uint8Array") {
    Napi::Error::New(env, "Argument must be a buffer object").ThrowAsJavaScriptException();
    return env.Null();
  }

  char *data = Buffer::Data(geojson_obj);
  size_t length = Buffer::Length(geojson_obj);

  GDALAsyncableJob<OGRGeometry *> job(0);
  job.main = [data, length](const GDALExecutionProgress &) {
    CPLErrorReset();
    CPLJSONDocument oDocument;
    if (!oDocument.LoadMemory(reinterpret_cast<const GByte *>(data), length)) {
      throw "Parsing the GeoJSON fragment failed";
    }
    OGRGeometry *geom = OGRGeometryFactory::createFromGeoJson(oDocument.GetRoot());

    if (geom == nullptr) throw CPLGetLastErrorMsg();
    return geom;
  };
  job.rval = [](OGRGeometry *geom, const GetFromPersistentFunc &) { return Geometry::New(geom, true); };
  job.run(info, async, 1);
#endif
}

/**
 * Creates an empty Geometry from a WKB type.
 *
 * @static
 * @method create
 * @instance
 * @memberof Geometry
 * @param {number} type WKB geometry type {@link wkbGeometryType|available options}
 * @return {Geometry}
 */
Napi::Value Geometry::create(const Napi::CallbackInfo &info) {
  OGRwkbGeometryType type = wkbUnknown;
  NODE_ARG_ENUM(0, "type", OGRwkbGeometryType, type);

  return Geometry::New(OGRGeometryFactory::createGeometry(type), true);
}

/**
 * @kind member
 * @name srs
 * @instance
 * @memberof Geometry
 * @type {SpatialReference|null}
 */
Napi::Value Geometry::srsGetter(const Napi::CallbackInfo &info) {
  Geometry *geom = info.This().Unwrap<Geometry>();
  return SpatialReference::New(geom->this_->getSpatialReference(), false);
}

void Geometry::srsSetter(const Napi::CallbackInfo &info, const Napi::Value &value) {
  Geometry *geom = info.This().Unwrap<Geometry>();

  OGRSpatialReference *srs = NULL;
  if (IS_WRAPPED(value, SpatialReference)) {
    SpatialReference *srs_obj = value.As<Napi::Object>().Unwrap<SpatialReference>();
    srs = srs_obj->get();
  } else if (!value.IsNull() && !value.IsNull()) {
    Napi::Error::New(env, "srs must be SpatialReference object").ThrowAsJavaScriptException();
    return env.Null();
  }

  geom->this_->assignSpatialReference(srs);
}

/**
 * @readonly
 * @kind member
 * @name name
 * @instance
 * @memberof Geometry
 * @type {string}
 */
Napi::Value Geometry::nameGetter(const Napi::CallbackInfo &info) {
  Geometry *geom = info.This().Unwrap<Geometry>();
  return SafeString::New(geom->this_->getGeometryName());
}

/**
 * See {@link wkbGeometryType}.
 * @readonly
 * @kind member
 * @name wkbType
 * @static
 * @memberof Geometry
 * @type {number}
 */

/**
 * See {@link wkbGeometryType}.
 * @readonly
 * @kind member
 * @name wkbType
 * @instance
 * @memberof Geometry
 * @type {number}
 */
Napi::Value Geometry::typeGetter(const Napi::CallbackInfo &info) {
  Geometry *geom = info.This().Unwrap<Geometry>();
  return Napi::Number::New(env, getGeometryType_fixed(geom->this_));
}

/**
 * @readonly
 * @kind member
 * @name wkbSize
 * @instance
 * @memberof Geometry
 * @type {number}
 */
Napi::Value Geometry::wkbSizeGetter(const Napi::CallbackInfo &info) {
  Geometry *geom = info.This().Unwrap<Geometry>();
  return Napi::Number::New(env, static_cast<int>(geom->this_->WkbSize()));
}

/**
 * @readonly
 * @kind member
 * @name dimension
 * @instance
 * @memberof Geometry
 * @type {number}
 */
Napi::Value Geometry::dimensionGetter(const Napi::CallbackInfo &info) {
  Geometry *geom = info.This().Unwrap<Geometry>();
  return Napi::Number::New(env, geom->this_->getDimension());
}

/**
 * @kind member
 * @name coordinateDimension
 * @instance
 * @memberof Geometry
 * @type {number}
 */
Napi::Value Geometry::coordinateDimensionGetter(const Napi::CallbackInfo &info) {
  Geometry *geom = info.This().Unwrap<Geometry>();
  return Napi::Number::New(env, geom->this_->getCoordinateDimension());
}

void Geometry::coordinateDimensionSetter(const Napi::CallbackInfo &info, const Napi::Value &value) {
  Geometry *geom = info.This().Unwrap<Geometry>();

  if (!value.IsNumber()) {
    Napi::Error::New(env, "coordinateDimension must be an integer").ThrowAsJavaScriptException();
    return env.Null();
  }
  int dim = value.As<Napi::Number>().Int64Value().ToChecked();
  if (dim != 2 && dim != 3) {
    Napi::Error::New(env, "coordinateDimension must be 2 or 3").ThrowAsJavaScriptException();
    return env.Null();
  }

  geom->this_->setCoordinateDimension(dim);
}

Napi::Value Geometry::_getConstructor(OGRwkbGeometryType type) {
  Napi::EscapableHandleScope scope(env);

  type = wkbFlatten(type);
  switch (type) {
    case wkbPoint: return scope.Escape(Napi::GetFunction(Napi::New(env, Point::constructor)));
    case wkbLineString: return scope.Escape(Napi::GetFunction(Napi::New(env, LineString::constructor)));
    case wkbLinearRing: return scope.Escape(Napi::GetFunction(Napi::New(env, LinearRing::constructor)));
    case wkbPolygon: return scope.Escape(Napi::GetFunction(Napi::New(env, Polygon::constructor)));
    case wkbGeometryCollection: return scope.Escape(Napi::GetFunction(Napi::New(env, GeometryCollection::constructor)));
    case wkbMultiPoint: return scope.Escape(Napi::GetFunction(Napi::New(env, MultiPoint::constructor)));
    case wkbMultiLineString: return scope.Escape(Napi::GetFunction(Napi::New(env, MultiLineString::constructor)));
    case wkbMultiPolygon: return scope.Escape(Napi::GetFunction(Napi::New(env, MultiPolygon::constructor)));
    case wkbCircularString: return scope.Escape(Napi::GetFunction(Napi::New(env, CircularString::constructor)));
    case wkbCompoundCurve: return scope.Escape(Napi::GetFunction(Napi::New(env, CompoundCurve::constructor)));
    case wkbMultiCurve: return scope.Escape(Napi::GetFunction(Napi::New(env, MultiCurve::constructor)));
    default: return scope.Escape(env.Null());
  }
}

/**
 * Returns the Geometry subclass that matches the
 * given WKB geometry type.
 *
 * @static
 * @method getConstructor
 * @instance
 * @memberof Geometry
 * @param {number} type WKB geometry type {@link wkbGeometryType|available options}
 * @return {Function}
 */
Napi::Value Geometry::getConstructor(const Napi::CallbackInfo &info) {
  OGRwkbGeometryType type;
  NODE_ARG_ENUM(0, "wkbType", OGRwkbGeometryType, type);
  return _getConstructor(type);
}

/**
 * Returns the Geometry subclass name that matches the
 * given WKB geometry type.
 *
 * @static
 * @method getName
 * @instance
 * @memberof Geometry
 * @param {number} type WKB geometry type {@link wkbGeometryType|available options}
 * @return {string}
 */
Napi::Value Geometry::getName(const Napi::CallbackInfo &info) {
  OGRwkbGeometryType type;
  NODE_ARG_ENUM(0, "wkbType", OGRwkbGeometryType, type);
  return SafeString::New(OGRGeometryTypeToName(type));
}

} // namespace node_gdal
