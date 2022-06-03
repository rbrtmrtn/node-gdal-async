#include "geometry_collection_children.hpp"
#include "../gdal_common.hpp"
#include "../geometry/gdal_geometry.hpp"
#include "../geometry/gdal_geometrycollection.hpp"

namespace node_gdal {

Napi::FunctionReference GeometryCollectionChildren::constructor;

void GeometryCollectionChildren::Initialize(Napi::Object target) {
  Napi::HandleScope scope(env);

  Napi::FunctionReference lcons = Napi::Function::New(env, GeometryCollectionChildren::New);

  lcons->SetClassName(Napi::String::New(env, "GeometryCollectionChildren"));

  InstanceMethod("toString", &toString),
  InstanceMethod("count", &count),
  InstanceMethod("get", &get),
  InstanceMethod("remove", &remove),
  InstanceMethod("add", &add),

  (target).Set(Napi::String::New(env, "GeometryCollectionChildren"), Napi::GetFunction(lcons));

  constructor.Reset(lcons);
}

GeometryCollectionChildren::GeometryCollectionChildren() : Napi::ObjectWrap<GeometryCollectionChildren>(){
}

GeometryCollectionChildren::~GeometryCollectionChildren() {
}

/**
 * A collection of Geometries, used by {@link GeometryCollection}.
 *
 * @class GeometryCollectionChildren
 */
Napi::Value GeometryCollectionChildren::New(const Napi::CallbackInfo& info) {

  if (!info.IsConstructCall()) {
    Napi::Error::New(env, "Cannot call constructor as function, you need to use 'new' keyword").ThrowAsJavaScriptException();
    return env.Null();
  }
  if (info[0].IsExternal()) {
    Napi::External ext = info[0].As<Napi::External>();
    void *ptr = ext->Value();
    GeometryCollectionChildren *geom = static_cast<GeometryCollectionChildren *>(ptr);
    geom->Wrap(info.This());
    return info.This();
    return;
  } else {
    Napi::Error::New(env, "Cannot create GeometryCollectionChildren directly").ThrowAsJavaScriptException();
    return env.Null();
  }
}

Napi::Value GeometryCollectionChildren::New(Napi::Value geom) {
  Napi::Env env = geom.Env();
  Napi::EscapableHandleScope scope(env);

  GeometryCollectionChildren *wrapped = new GeometryCollectionChildren();

  Napi::Value ext = Napi::External::New(env, wrapped);
  Napi::Object obj =
    Napi::NewInstance(Napi::GetFunction(Napi::New(env, GeometryCollectionChildren::constructor)), 1, &ext)
      ;
  Napi::SetPrivate(obj, Napi::String::New(env, "parent_"), geom);

  return scope.Escape(obj);
}

Napi::Value GeometryCollectionChildren::toString(const Napi::CallbackInfo& info) {
  return Napi::String::New(env, "GeometryCollectionChildren");
}

/**
 * Returns the number of items.
 *
 * @method count
 * @instance
 * @memberof GeometryCollectionChildren
 * @return {number}
 */
Napi::Value GeometryCollectionChildren::count(const Napi::CallbackInfo& info) {

  Napi::Object parent =
    Napi::GetPrivate(info.This(), Napi::String::New(env, "parent_")).As<Napi::Object>();
  GeometryCollection *geom = parent.Unwrap<GeometryCollection>();

  return Napi::Number::New(env, geom->get()->getNumGeometries());
}

/**
 * Returns the geometry at the specified index.
 *
 * @method get
 * @instance
 * @memberof GeometryCollectionChildren
 * @param {number} index 0-based index
 * @throws {Error}
 * @return {Geometry}
 */
Napi::Value GeometryCollectionChildren::get(const Napi::CallbackInfo& info) {

  Napi::Object parent =
    Napi::GetPrivate(info.This(), Napi::String::New(env, "parent_")).As<Napi::Object>();
  GeometryCollection *geom = parent.Unwrap<GeometryCollection>();

  int i;
  NODE_ARG_INT(0, "index", i);

  auto r = geom->get()->getGeometryRef(i);
  if (r == nullptr) {
    NODE_THROW_LAST_CPLERR;
    return;
  }
  return Geometry::New(r, false);
}

/**
 * Removes the geometry at the specified index.
 *
 * @method remove
 * @instance
 * @memberof GeometryCollectionChildren
 * @param {number} index 0-based index, -1 for all geometries
 */
Napi::Value GeometryCollectionChildren::remove(const Napi::CallbackInfo& info) {

  Napi::Object parent =
    Napi::GetPrivate(info.This(), Napi::String::New(env, "parent_")).As<Napi::Object>();
  GeometryCollection *geom = parent.Unwrap<GeometryCollection>();

  int i;
  NODE_ARG_INT(0, "index", i);

  OGRErr err = geom->get()->removeGeometry(i);
  if (err) {
    NODE_THROW_OGRERR(err);
    return;
  }

  return;
}

/**
 * Adds geometry(s) to the collection.
 *
 * @example
 *
 * // one at a time:
 * geometryCollection.children.add(new Point(0,0,0));
 *
 * // add many at once:
 * geometryCollection.children.add([
 *     new Point(1,0,0),
 *     new Point(1,0,0)
 * ]);
 *
 * @method add
 * @instance
 * @memberof GeometryCollectionChildren
 * @param {Geometry|Geometry[]} geometry
 */
Napi::Value GeometryCollectionChildren::add(const Napi::CallbackInfo& info) {

  Napi::Object parent =
    Napi::GetPrivate(info.This(), Napi::String::New(env, "parent_")).As<Napi::Object>();
  GeometryCollection *geom = parent.Unwrap<GeometryCollection>();

  Geometry *child;

  if (info.Length() < 1) {
    Napi::Error::New(env, "child(ren) must be given").ThrowAsJavaScriptException();
    return env.Null();
  }
  if (info[0].IsArray()) {
    // set from array of geometry objects
    Napi::Array array = info[0].As<Napi::Array>();
    int length = array->Length();
    for (int i = 0; i < length; i++) {
      Napi::Value element = (array).Get(i);
      if (IS_WRAPPED(element, Geometry)) {
        child = element.As<Napi::Object>().Unwrap<Geometry>();
        OGRErr err = geom->get()->addGeometry(child->get());
        if (err) {
          NODE_THROW_OGRERR(err);
          return;
        }
      } else {
        Napi::Error::New(env, "All array elements must be geometry objects").ThrowAsJavaScriptException();
        return env.Null();
      }
    }
  } else if (IS_WRAPPED(info[0], Geometry)) {
    child = info[0].As<Napi::Object>().Unwrap<Geometry>();
    OGRErr err = geom->get()->addGeometry(child->get());
    if (err) {
      NODE_THROW_OGRERR(err);
      return;
    }
  } else {
    Napi::Error::New(env, "child must be a geometry object or array of geometry objects").ThrowAsJavaScriptException();
    return env.Null();
  }

  return;
}

} // namespace node_gdal
