
#include "gdal_polygon.hpp"
#include "../collections/polygon_rings.hpp"
#include "../gdal_common.hpp"
#include "gdal_geometry.hpp"

#include <stdlib.h>

namespace node_gdal {

Napi::FunctionReference Polygon::constructor;

void Polygon::Initialize(Napi::Object target) {
  Napi::HandleScope scope(env);

  Napi::FunctionReference lcons = Napi::Function::New(env, Polygon::New);
  lcons->Inherit(Napi::New(env, Geometry::constructor));

  lcons->SetClassName(Napi::String::New(env, "Polygon"));

  InstanceMethod("toString", &toString),
  InstanceMethod("getArea", &getArea),

  ATTR(lcons, "rings", ringsGetter, READ_ONLY_SETTER);

  (target).Set(Napi::String::New(env, "Polygon"), Napi::GetFunction(lcons));

  constructor.Reset(lcons);
}

void Polygon::SetPrivate(Napi::ADDON_REGISTER_FUNCTION_ARGS_TYPE _this, Napi::Value value) {
  Napi::SetPrivate(_this, Napi::String::New(env, "rings_"), value);
};

/**
 * Concrete class representing polygons.
 *
 * @constructor
 * @class Polygon
 * @extends Geometry
 */

Napi::Value Polygon::toString(const Napi::CallbackInfo& info) {
  return Napi::String::New(env, "Polygon");
}

/**
 * Computes the area of the polygon.
 *
 * @method getArea
 * @instance
 * @memberof Polygon
 * @return {number}
 */
NODE_WRAPPED_METHOD_WITH_RESULT(Polygon, getArea, Number, get_Area);

/**
 * The rings that make up the polygon geometry.
 *
 * @kind member
 * @name rings
 * @instance
 * @memberof Polygon
 * @type {PolygonRings}
 */
Napi::Value Polygon::ringsGetter(const Napi::CallbackInfo& info) {
  return Napi::GetPrivate(info.This(), Napi::String::New(env, "rings_"));
}

} // namespace node_gdal
