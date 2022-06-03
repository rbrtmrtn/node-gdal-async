
#include "gdal_circularstring.hpp"
#include "gdal_linestring.hpp"
#include "../collections/linestring_points.hpp"
#include "../gdal_common.hpp"
#include "gdal_geometry.hpp"
#include "gdal_simplecurve.hpp"
#include "gdal_point.hpp"

#include <stdlib.h>

namespace node_gdal {

Napi::FunctionReference CircularString::constructor;

void CircularString::Initialize(Napi::Object target) {
  Napi::HandleScope scope(env);

  Napi::FunctionReference lcons = Napi::Function::New(env, CircularString::New);
  lcons->Inherit(Napi::New(env, SimpleCurve::constructor));

  lcons->SetClassName(Napi::String::New(env, "CircularString"));

  InstanceMethod("toString", &toString),

  (target).Set(Napi::String::New(env, "CircularString"), Napi::GetFunction(lcons));

  constructor.Reset(lcons);
}

/**
 * Concrete representation of an arc.
 *
 * @example
 *
 * var CircularString = new gdal.CircularString();
 * CircularString.points.add(new gdal.Point(0,0));
 * CircularString.points.add(new gdal.Point(0,10));
 *
 * @constructor
 * @class CircularString
 * @extends SimpleCurve
 */

Napi::Value CircularString::toString(const Napi::CallbackInfo& info) {
  return Napi::String::New(env, "CircularString");
}

} // namespace node_gdal
