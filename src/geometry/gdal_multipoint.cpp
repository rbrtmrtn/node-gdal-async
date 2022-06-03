
#include "gdal_multipoint.hpp"
#include "../collections/geometry_collection_children.hpp"
#include "../gdal_common.hpp"
#include "gdal_geometry.hpp"
#include "gdal_geometrycollection.hpp"
#include "gdal_point.hpp"

#include <stdlib.h>

namespace node_gdal {

Napi::FunctionReference MultiPoint::constructor;

void MultiPoint::Initialize(Napi::Object target) {
  Napi::HandleScope scope(env);

  Napi::FunctionReference lcons = Napi::Function::New(env, MultiPoint::New);
  lcons->Inherit(Napi::New(env, GeometryCollection::constructor));

  lcons->SetClassName(Napi::String::New(env, "MultiPoint"));

  InstanceMethod("toString", &toString),

  (target).Set(Napi::String::New(env, "MultiPoint"), Napi::GetFunction(lcons));

  constructor.Reset(lcons);
}

/**
 * @constructor
 * @class MultiPoint
 * @extends GeometryCollection
 */

Napi::Value MultiPoint::toString(const Napi::CallbackInfo& info) {
  return Napi::String::New(env, "MultiPoint");
}

} // namespace node_gdal
