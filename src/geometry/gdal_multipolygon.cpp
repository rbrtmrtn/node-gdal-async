#include "gdal_multipolygon.hpp"
#include "../collections/geometry_collection_children.hpp"
#include "../gdal_common.hpp"
#include "gdal_geometry.hpp"
#include "gdal_geometrycollection.hpp"
#include "gdal_polygon.hpp"

#include <stdlib.h>

namespace node_gdal {

Napi::FunctionReference MultiPolygon::constructor;

void MultiPolygon::Initialize(Napi::Object target) {
  Napi::HandleScope scope(env);

  Napi::FunctionReference lcons = Napi::Function::New(env, MultiPolygon::New);
  lcons->Inherit(Napi::New(env, GeometryCollection::constructor));

  lcons->SetClassName(Napi::String::New(env, "MultiPolygon"));

  InstanceMethod("toString", &toString),
  InstanceMethod("unionCascaded", &unionCascaded),
  InstanceMethod("getArea", &getArea),

  (target).Set(Napi::String::New(env, "MultiPolygon"), Napi::GetFunction(lcons));

  constructor.Reset(lcons);
}

/**
 * @constructor
 * @class MultiPolygon
 * @extends GeometryCollection
 */

Napi::Value MultiPolygon::toString(const Napi::CallbackInfo& info) {
  return Napi::String::New(env, "MultiPolygon");
}

/**
 * Unions all the geometries and returns the result.
 *
 * @method unionCascaded
 * @instance
 * @memberof MultiPolygon
 * @return {Geometry}
 */
Napi::Value MultiPolygon::unionCascaded(const Napi::CallbackInfo& info) {

  MultiPolygon *geom = info.This().Unwrap<MultiPolygon>();
  auto r = geom->this_->UnionCascaded();
  if (r == nullptr) {
    NODE_THROW_LAST_CPLERR;
    return;
  }

  return Geometry::New(r);
}

/**
 * Computes the combined area of the collection.
 *
 * @method getArea
 * @instance
 * @memberof MultiPolygon
 * @return {number}
 */
NODE_WRAPPED_METHOD_WITH_RESULT(MultiPolygon, getArea, Number, get_Area);

} // namespace node_gdal
