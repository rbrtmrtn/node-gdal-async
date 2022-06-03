
#include "gdal_linearring.hpp"
#include "../collections/linestring_points.hpp"
#include "../gdal_common.hpp"
#include "gdal_geometry.hpp"
#include "gdal_linestring.hpp"

#include <stdlib.h>

namespace node_gdal {

Napi::FunctionReference LinearRing::constructor;

void LinearRing::Initialize(Napi::Object target) {
  Napi::HandleScope scope(env);

  Napi::FunctionReference lcons = Napi::Function::New(env, LinearRing::New);
  lcons->Inherit(Napi::New(env, LineString::constructor));

  lcons->SetClassName(Napi::String::New(env, "LinearRing"));

  InstanceMethod("toString", &toString),
  InstanceMethod("getArea", &getArea),
  InstanceMethod("addSubLineString", &addSubLineString),

  (target).Set(Napi::String::New(env, "LinearRing"), Napi::GetFunction(lcons));

  constructor.Reset(lcons);
}

Napi::Value LinearRing::New(OGRLinearRing *geom, bool owned) {
  Napi::EscapableHandleScope scope(env);

  if (!geom) { return scope.Escape(env.Null()); }

  // make a copy of geometry owned by a feature
  // + no need to track when a feature is destroyed
  // + no need to throw errors when a method trys to modify an owned read-only
  // geometry
  // - is slower

  if (!owned) { geom = static_cast<OGRLinearRing *>(geom->clone()); }

  LinearRing *wrapped = new LinearRing(geom);
  wrapped->owned_ = true;

  Napi::Value ext = Napi::External::New(env, wrapped);
  Napi::Object obj =
    Napi::NewInstance(Napi::GetFunction(Napi::New(env, LinearRing::constructor)), 1, &ext);

  return scope.Escape(obj);
}

/**
 * Concrete representation of a closed ring.
 *
 * @constructor
 * @class LinearRing
 * @extends LineString
 */

Napi::Value LinearRing::toString(const Napi::CallbackInfo& info) {
  return Napi::String::New(env, "LinearRing");
}

/**
 * Computes the area enclosed by the ring.
 *
 * @method getArea
 * @instance
 * @memberof LinearRing
 * @return {number}
 */
NODE_WRAPPED_METHOD_WITH_RESULT(LinearRing, getArea, Number, get_Area);

Napi::Value LinearRing::addSubLineString(const Napi::CallbackInfo& info) {

  LinearRing *geom = info.This().Unwrap<LinearRing>();
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

  return;
}

} // namespace node_gdal
