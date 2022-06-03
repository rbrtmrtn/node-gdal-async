
#include "gdal_linestring.hpp"
#include "../collections/linestring_points.hpp"
#include "../gdal_common.hpp"
#include "gdal_geometry.hpp"
#include "gdal_simplecurve.hpp"
#include "gdal_point.hpp"

#include <stdlib.h>

namespace node_gdal {

Napi::FunctionReference LineString::constructor;

void LineString::Initialize(Napi::Object target) {
  Napi::HandleScope scope(env);

  Napi::FunctionReference lcons = Napi::Function::New(env, LineString::New);
  lcons->Inherit(Napi::New(env, SimpleCurve::constructor));

  lcons->SetClassName(Napi::String::New(env, "LineString"));

  InstanceMethod("toString", &toString),

  (target).Set(Napi::String::New(env, "LineString"), Napi::GetFunction(lcons));

  constructor.Reset(lcons);
}

/**
 * Concrete representation of a multi-vertex line.
 *
 * @example
 *
 * var lineString = new gdal.LineString();
 * lineString.points.add(new gdal.Point(0,0));
 * lineString.points.add(new gdal.Point(0,10));
 *
 * @constructor
 * @class LineString
 * @extends SimpleCurve
 */

Napi::Value LineString::toString(const Napi::CallbackInfo& info) {
  return Napi::String::New(env, "LineString");
}

} // namespace node_gdal
