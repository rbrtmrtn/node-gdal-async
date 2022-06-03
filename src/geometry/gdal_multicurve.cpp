
#include "gdal_multicurve.hpp"
#include "../collections/geometry_collection_children.hpp"
#include "../gdal_common.hpp"
#include "gdal_geometry.hpp"
#include "gdal_geometrycollection.hpp"

#include <stdlib.h>

namespace node_gdal {

Napi::FunctionReference MultiCurve::constructor;

void MultiCurve::Initialize(Napi::Object target) {
  Napi::HandleScope scope(env);

  Napi::FunctionReference lcons = Napi::Function::New(env, MultiCurve::New);
  lcons->Inherit(Napi::New(env, GeometryCollection::constructor));

  lcons->SetClassName(Napi::String::New(env, "MultiCurve"));

  InstanceMethod("toString", &toString),
  InstanceMethod("polygonize", &polygonize),

  (target).Set(Napi::String::New(env, "MultiCurve"), Napi::GetFunction(lcons));

  constructor.Reset(lcons);
}

/**
 * @constructor
 * @class MultiCurve
 * @extends GeometryCollection
 */

Napi::Value MultiCurve::toString(const Napi::CallbackInfo& info) {
  return Napi::String::New(env, "MultiCurve");
}

/**
 * Converts it to a polygon.
 *
 * @method polygonize
 * @instance
 * @memberof MultiCurve
 * @return {Polygon}
 */
Napi::Value MultiCurve::polygonize(const Napi::CallbackInfo& info) {

  MultiCurve *geom = info.This().Unwrap<MultiCurve>();

  return Geometry::New(geom->this_->Polygonize());
}

} // namespace node_gdal
