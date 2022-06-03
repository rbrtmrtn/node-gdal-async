
#include "gdal_geometrycollection.hpp"
#include "../collections/geometry_collection_children.hpp"
#include "../gdal_common.hpp"
#include "gdal_geometry.hpp"

#include <stdlib.h>

namespace node_gdal {

Napi::FunctionReference GeometryCollection::constructor;

/**
 * A collection of 1 or more geometry objects.
 *
 * @constructor
 * @class GeometryCollection
 * @extends Geometry
 */
void GeometryCollection::Initialize(Napi::Object target) {
  Napi::HandleScope scope(env);

  Napi::FunctionReference lcons = Napi::Function::New(env, GeometryCollection::New);
  lcons->Inherit(Napi::New(env, Geometry::constructor));

  lcons->SetClassName(Napi::String::New(env, "GeometryCollection"));

  InstanceMethod("toString", &toString),
  InstanceMethod("getArea", &getArea),
  InstanceMethod("getLength", &getLength),

  ATTR(lcons, "children", childrenGetter, READ_ONLY_SETTER);

  (target).Set(Napi::String::New(env, "GeometryCollection"), Napi::GetFunction(lcons));

  constructor.Reset(lcons);
}

Napi::Value GeometryCollection::toString(const Napi::CallbackInfo& info) {
  return Napi::String::New(env, "GeometryCollection");
}

/**
 * Computes the combined area of the geometries.
 *
 * @method getArea
 * @instance
 * @memberof GeometryCollection
 * @return {number}
 */
NODE_WRAPPED_METHOD_WITH_RESULT(GeometryCollection, getArea, Number, get_Area);

/**
 * Compute the length of a multicurve.
 *
 * @method getLength
 * @instance
 * @memberof GeometryCollection
 * @return {number}
 */
NODE_WRAPPED_METHOD_WITH_RESULT(GeometryCollection, getLength, Number, get_Length);

/**
 * All geometries represented by this collection.
 *
 * @kind member
 * @name children
 * @instance
 * @memberof GeometryCollection
 * @type {GeometryCollectionChildren}
 */
Napi::Value GeometryCollection::childrenGetter(const Napi::CallbackInfo& info) {
  return Napi::GetPrivate(info.This(), Napi::String::New(env, "children_"));
}

} // namespace node_gdal
