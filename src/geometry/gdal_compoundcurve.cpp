
#include "gdal_compoundcurve.hpp"
#include "../collections/linestring_points.hpp"
#include "../gdal_common.hpp"
#include "gdal_geometry.hpp"
#include "gdal_point.hpp"

#include <stdlib.h>

namespace node_gdal {

Napi::FunctionReference CompoundCurve::constructor;

void CompoundCurve::Initialize(Napi::Object target) {
  Napi::HandleScope scope(env);

  Napi::FunctionReference lcons = Napi::Function::New(env, CompoundCurve::New);
  lcons->Inherit(Napi::New(env, Geometry::constructor));

  lcons->SetClassName(Napi::String::New(env, "CompoundCurve"));

  InstanceMethod("toString", &toString),

  ATTR(lcons, "curves", curvesGetter, READ_ONLY_SETTER);

  (target).Set(Napi::String::New(env, "CompoundCurve"), Napi::GetFunction(lcons));

  constructor.Reset(lcons);
}

void CompoundCurve::SetPrivate(Napi::ADDON_REGISTER_FUNCTION_ARGS_TYPE _this, Napi::Value value) {
  Napi::SetPrivate(_this, Napi::String::New(env, "curves_"), value);
};

/**
 * Concrete representation of a compound contionuos curve.
 *
 * @example
 *
 * var CompoundCurve = new gdal.CompoundCurve();
 * CompoundCurve.points.add(new gdal.Point(0,0));
 * CompoundCurve.points.add(new gdal.Point(0,10));
 *
 * @constructor
 * @class CompoundCurve
 * @extends Geometry
 */

Napi::Value CompoundCurve::toString(const Napi::CallbackInfo& info) {
  return Napi::String::New(env, "CompoundCurve");
}

/**
 * Points that make up the compound curve.
 *
 * @kind member
 * @name curves
 * @instance
 * @memberof CompoundCurve
 * @type {CompoundCurveCurves}
 */
Napi::Value CompoundCurve::curvesGetter(const Napi::CallbackInfo& info) {
  return Napi::GetPrivate(info.This(), Napi::String::New(env, "curves_"));
}

} // namespace node_gdal
