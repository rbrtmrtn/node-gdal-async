
#include "gdal_multilinestring.hpp"
#include "../collections/geometry_collection_children.hpp"
#include "../gdal_common.hpp"
#include "gdal_geometry.hpp"
#include "gdal_geometrycollection.hpp"
#include "gdal_linestring.hpp"

#include <stdlib.h>

namespace node_gdal {

Napi::FunctionReference MultiLineString::constructor;

void MultiLineString::Initialize(Napi::Object target) {
  Napi::HandleScope scope(env);

  Napi::FunctionReference lcons = Napi::Function::New(env, MultiLineString::New);
  lcons->Inherit(Napi::New(env, GeometryCollection::constructor));

  lcons->SetClassName(Napi::String::New(env, "MultiLineString"));

  InstanceMethod("toString", &toString),
  InstanceMethod("polygonize", &polygonize),

  (target).Set(Napi::String::New(env, "MultiLineString"), Napi::GetFunction(lcons));

  constructor.Reset(lcons);
}

/**
 * @constructor
 * @class MultiLineString
 * @extends GeometryCollection
 */

Napi::Value MultiLineString::toString(const Napi::CallbackInfo& info) {
  return Napi::String::New(env, "MultiLineString");
}

/**
 * Converts it to a polygon.
 *
 * @method polygonize
 * @instance
 * @memberof MultiLineString
 * @return {Polygon}
 */
Napi::Value MultiLineString::polygonize(const Napi::CallbackInfo& info) {

  MultiLineString *geom = info.This().Unwrap<MultiLineString>();

  return Geometry::New(geom->this_->Polygonize());
}

} // namespace node_gdal
