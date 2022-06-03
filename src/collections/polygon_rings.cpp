#include "polygon_rings.hpp"
#include "../gdal_common.hpp"
#include "../geometry/gdal_geometry.hpp"
#include "../geometry/gdal_linearring.hpp"
#include "../geometry/gdal_polygon.hpp"

namespace node_gdal {

Napi::FunctionReference PolygonRings::constructor;

void PolygonRings::Initialize(Napi::Object target) {
  Napi::HandleScope scope(env);

  Napi::FunctionReference lcons = Napi::Function::New(env, PolygonRings::New);

  lcons->SetClassName(Napi::String::New(env, "PolygonRings"));

  InstanceMethod("toString", &toString),
  InstanceMethod("count", &count),
  InstanceMethod("get", &get),
  InstanceMethod("add", &add),

  (target).Set(Napi::String::New(env, "PolygonRings"), Napi::GetFunction(lcons));

  constructor.Reset(lcons);
}

PolygonRings::PolygonRings() : Napi::ObjectWrap<PolygonRings>(){
}

PolygonRings::~PolygonRings() {
}

/**
 * A collection of polygon rings, used by {@link Polygon}.
 *
 * @class PolygonRings
 */
Napi::Value PolygonRings::New(const Napi::CallbackInfo& info) {

  if (!info.IsConstructCall()) {
    Napi::Error::New(env, "Cannot call constructor as function, you need to use 'new' keyword").ThrowAsJavaScriptException();
    return env.Null();
  }
  if (info[0].IsExternal()) {
    Napi::External ext = info[0].As<Napi::External>();
    void *ptr = ext->Value();
    PolygonRings *geom = static_cast<PolygonRings *>(ptr);
    geom->Wrap(info.This());
    return info.This();
    return;
  } else {
    Napi::Error::New(env, "Cannot create PolygonRings directly").ThrowAsJavaScriptException();
    return env.Null();
  }
}

Napi::Value PolygonRings::New(Napi::Value geom) {
  Napi::Env env = geom.Env();
  Napi::EscapableHandleScope scope(env);

  PolygonRings *wrapped = new PolygonRings();

  Napi::Value ext = Napi::External::New(env, wrapped);
  Napi::Object obj =
    Napi::NewInstance(Napi::GetFunction(Napi::New(env, PolygonRings::constructor)), 1, &ext);
  Napi::SetPrivate(obj, Napi::String::New(env, "parent_"), geom);

  return scope.Escape(obj);
}

Napi::Value PolygonRings::toString(const Napi::CallbackInfo& info) {
  return Napi::String::New(env, "PolygonRings");
}

/**
 * Returns the number of rings that exist in the collection.
 *
 * @method count
 * @instance
 * @memberof PolygonRings
 * @return {number}
 */
Napi::Value PolygonRings::count(const Napi::CallbackInfo& info) {

  Napi::Object parent =
    Napi::GetPrivate(info.This(), Napi::String::New(env, "parent_")).As<Napi::Object>();
  Polygon *geom = parent.Unwrap<Polygon>();

  int i = geom->get()->getExteriorRing() ? 1 : 0;
  i += geom->get()->getNumInteriorRings();

  return Napi::Number::New(env, i);
}

/**
 * Returns the ring at the specified index. The ring
 * at index `0` will always be the polygon's exterior ring.
 *
 * @example
 *
 * var exterior = polygon.rings.get(0);
 * var interior = polygon.rings.get(1);
 *
 * @method get
 * @instance
 * @memberof PolygonRings
 * @param {number} index
 * @throws {Error}
 * @return {LinearRing}
 */
Napi::Value PolygonRings::get(const Napi::CallbackInfo& info) {

  Napi::Object parent =
    Napi::GetPrivate(info.This(), Napi::String::New(env, "parent_")).As<Napi::Object>();
  Polygon *geom = parent.Unwrap<Polygon>();

  int i;
  NODE_ARG_INT(0, "index", i);

  OGRLinearRing *r;
  if (i == 0) {
    r = geom->get()->getExteriorRing();
  } else {
    r = geom->get()->getInteriorRing(i - 1);
  }
  if (r == nullptr) {
    NODE_THROW_LAST_CPLERR;
    return;
  }
  return LinearRing::New(r, false);
}

/**
 * Adds a ring to the collection.
 *
 * @example
 *
 * var ring1 = new gdal.LinearRing();
 * ring1.points.add(0,0);
 * ring1.points.add(1,0);
 * ring1.points.add(1,1);
 * ring1.points.add(0,1);
 * ring1.points.add(0,0);
 *
 * // one at a time:
 * polygon.rings.add(ring1);
 *
 * // many at once:
 * polygon.rings.add([ring1, ...]);
 *
 * @method add
 * @instance
 * @memberof PolygonRings
 * @param {LinearRing|LinearRing[]} rings
 */
Napi::Value PolygonRings::add(const Napi::CallbackInfo& info) {

  Napi::Object parent =
    Napi::GetPrivate(info.This(), Napi::String::New(env, "parent_")).As<Napi::Object>();
  Polygon *geom = parent.Unwrap<Polygon>();

  LinearRing *ring;

  if (info.Length() < 1) {
    Napi::Error::New(env, "ring(s) must be given").ThrowAsJavaScriptException();
    return env.Null();
  }
  if (info[0].IsArray()) {
    // set from array of geometry objects
    Napi::Array array = info[0].As<Napi::Array>();
    int length = array->Length();
    for (int i = 0; i < length; i++) {
      Napi::Value element = (array).Get(i);
      if (IS_WRAPPED(element, LinearRing)) {
        ring = element.As<Napi::Object>().Unwrap<LinearRing>();
        OGRErr err = geom->get()->addRing(ring->get());
        if (err) {
          NODE_THROW_OGRERR(err);
          return;
        }
      } else {
        Napi::Error::New(env, "All array elements must be LinearRings").ThrowAsJavaScriptException();
        return env.Null();
      }
    }
  } else if (IS_WRAPPED(info[0], LinearRing)) {
    ring = info[0].As<Napi::Object>().Unwrap<LinearRing>();
    OGRErr err = geom->get()->addRing(ring->get());
    if (err) {
      NODE_THROW_OGRERR(err);
      return;
    }
  } else {
    Napi::Error::New(env, "ring(s) must be a LinearRing or array of LinearRings").ThrowAsJavaScriptException();
    return env.Null();
  }

  return;
}

} // namespace node_gdal
