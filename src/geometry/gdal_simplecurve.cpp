
#include "gdal_simplecurve.hpp"
#include "../collections/linestring_points.hpp"
#include "../gdal_common.hpp"
#include "gdal_geometry.hpp"
#include "gdal_point.hpp"
#include "gdal_linestring.hpp"

#include <stdlib.h>

namespace node_gdal {

Napi::FunctionReference SimpleCurve::constructor;

void SimpleCurve::Initialize(Napi::Object target) {
  Napi::HandleScope scope(env);

  Napi::FunctionReference lcons = Napi::Function::New(env, SimpleCurve::New);
  lcons->Inherit(Napi::New(env, Geometry::constructor));

  lcons->SetClassName(Napi::String::New(env, "SimpleCurve"));

  InstanceMethod("toString", &toString),
  InstanceMethod("getLength", &getLength),
  InstanceMethod("value", &value),
  InstanceMethod("addSubLineString", &addSubLineString),

  ATTR(lcons, "points", pointsGetter, READ_ONLY_SETTER);

  (target).Set(Napi::String::New(env, "SimpleCurve"), Napi::GetFunction(lcons));

  constructor.Reset(lcons);
}

/**
 * Abstract class representing all SimpleCurves.
 *
 * @constructor
 * @class SimpleCurve
 * @extends Geometry
 */
Napi::Value SimpleCurve::New(const Napi::CallbackInfo& info) {
  Napi::Error::New(env, "SimpleCurve is an abstract class and cannot be instantiated").ThrowAsJavaScriptException();

}

Napi::Value SimpleCurve::toString(const Napi::CallbackInfo& info) {
  return Napi::String::New(env, "SimpleCurve");
}

/**
 * Returns the point at the specified distance along the SimpleCurve.
 *
 * @method value
 * @instance
 * @memberof SimpleCurve
 * @param {number} distance
 * @return {Point}
 */
Napi::Value SimpleCurve::value(const Napi::CallbackInfo& info) {

  SimpleCurve *geom = info.This().Unwrap<SimpleCurve>();

  OGRPoint *pt = new OGRPoint();
  double dist;

  NODE_ARG_DOUBLE(0, "distance", dist);

  geom->this_->Value(dist, pt);

  return Point::New(pt);
}

/**
 * Compute the length of a multiSimpleCurve.
 *
 * @method getLength
 * @instance
 * @memberof SimpleCurve
 * @return {number}
 */
NODE_WRAPPED_METHOD_WITH_RESULT(SimpleCurve, getLength, Number, get_Length);

/**
 * The points that make up the SimpleCurve geometry.
 *
 * @kind member
 * @name points
 * @instance
 * @memberof Geometry
 * @type {LineStringPoints}
 */
Napi::Value SimpleCurve::pointsGetter(const Napi::CallbackInfo& info) {
  return Napi::GetPrivate(info.This(), Napi::String::New(env, "points_"));
}

/**
 * Add a segment of another LineString to this SimpleCurve subtype.
 *
 * Adds the request range of vertices to the end of this compound curve in an
 * efficient manner. If the start index is larger than the end index then the
 * vertices will be reversed as they are copied.
 *
 * @method addSubLineString
 * @instance
 * @memberof SimpleCurve
 * @param {LineString} LineString to be added
 * @param {number} [start=0] the first vertex to copy, defaults to 0 to start with
 * the first vertex in the other LineString
 * @param {number} [end=-1] the last vertex to copy, defaults to -1 indicating the
 * last vertex of the other LineString
 * @return {void}
 */
Napi::Value SimpleCurve::addSubLineString(const Napi::CallbackInfo& info) {

  SimpleCurve *geom = info.This().Unwrap<SimpleCurve>();
  LineString *other;
  int start = 0;
  int end = -1;

  NODE_ARG_WRAPPED(0, "line", LineString, other);
  NODE_ARG_INT_OPT(1, "start", start);
  NODE_ARG_INT_OPT(2, "end", end);

  int n = other->get()->getNumPoints();

  if (start < 0 || end < -1 || start >= n || end >= n) {
    Napi::RangeError::New(env, "Invalid start or end index for LineString").ThrowAsJavaScriptException();
    return env.Null();
  }

  geom->this_->addSubLineString(other->get(), start, end);

  UPDATE_AMOUNT_OF_GEOMETRY_MEMORY(geom);

  return;
}

} // namespace node_gdal
