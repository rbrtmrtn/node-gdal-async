#include "compound_curves.hpp"
#include "../gdal_common.hpp"
#include "../geometry/gdal_geometry.hpp"
#include "../geometry/gdal_linearring.hpp"
#include "../geometry/gdal_simplecurve.hpp"
#include "../geometry/gdal_compoundcurve.hpp"

namespace node_gdal {

Napi::FunctionReference CompoundCurveCurves::constructor;

void CompoundCurveCurves::Initialize(Napi::Object target) {
  Napi::HandleScope scope(env);

  Napi::FunctionReference lcons = Napi::Function::New(env, CompoundCurveCurves::New);

  lcons->SetClassName(Napi::String::New(env, "CompoundCurveCurves"));

  InstanceMethod("toString", &toString),
  InstanceMethod("count", &count),
  InstanceMethod("get", &get),
  InstanceMethod("add", &add),

  (target).Set(Napi::String::New(env, "CompoundCurveCurves"), Napi::GetFunction(lcons));

  constructor.Reset(lcons);
}

CompoundCurveCurves::CompoundCurveCurves() : Napi::ObjectWrap<CompoundCurveCurves>(){
}

CompoundCurveCurves::~CompoundCurveCurves() {
}

/**
 * A collection of connected curves, used by {@link CompoundCurve}
 *
 * @class CompoundCurveCurves
 */
Napi::Value CompoundCurveCurves::New(const Napi::CallbackInfo& info) {

  if (!info.IsConstructCall()) {
    Napi::Error::New(env, "Cannot call constructor as function, you need to use 'new' keyword").ThrowAsJavaScriptException();
    return env.Null();
  }
  if (info[0].IsExternal()) {
    Napi::External ext = info[0].As<Napi::External>();
    void *ptr = ext->Value();
    CompoundCurveCurves *geom = static_cast<CompoundCurveCurves *>(ptr);
    geom->Wrap(info.This());
    return info.This();
    return;
  } else {
    Napi::Error::New(env, "Cannot create CompoundCurveCurves directly").ThrowAsJavaScriptException();
    return env.Null();
  }
}

Napi::Value CompoundCurveCurves::New(Napi::Value geom) {
  Napi::Env env = geom.Env();
  Napi::EscapableHandleScope scope(env);

  CompoundCurveCurves *wrapped = new CompoundCurveCurves();

  Napi::Value ext = Napi::External::New(env, wrapped);
  Napi::Object obj =
    Napi::NewInstance(Napi::GetFunction(Napi::New(env, CompoundCurveCurves::constructor)), 1, &ext)
      ;
  Napi::SetPrivate(obj, Napi::String::New(env, "parent_"), geom);

  return scope.Escape(obj);
}

Napi::Value CompoundCurveCurves::toString(const Napi::CallbackInfo& info) {
  return Napi::String::New(env, "CompoundCurveCurves");
}

/**
 * Returns the number of curves that exist in the collection.
 *
 * @method count
 * @instance
 * @memberof CompoundCurveCurves
 * @return {number}
 */
Napi::Value CompoundCurveCurves::count(const Napi::CallbackInfo& info) {

  Napi::Object parent =
    Napi::GetPrivate(info.This(), Napi::String::New(env, "parent_")).As<Napi::Object>();
  CompoundCurve *geom = parent.Unwrap<CompoundCurve>();

  return Napi::Number::New(env, geom->get()->getNumCurves());
}

/**
 * Returns the curve at the specified index.
 *
 * @example
 *
 * var curve0 = compound.curves.get(0);
 * var curve1 = compound.curves.get(1);
 *
 * @method get
 * @instance
 * @memberof CompoundCurveCurves
 * @param {number} index
 * @throws {Error}
 * @return {CompoundCurve|SimpleCurve}
 */
Napi::Value CompoundCurveCurves::get(const Napi::CallbackInfo& info) {

  Napi::Object parent =
    Napi::GetPrivate(info.This(), Napi::String::New(env, "parent_")).As<Napi::Object>();
  CompoundCurve *geom = parent.Unwrap<CompoundCurve>();

  int i;
  NODE_ARG_INT(0, "index", i);

  if (i >= 0 && i < geom->get()->getNumCurves())
    return Geometry::New(geom->get()->getCurve(i), false);
  else
    Napi::RangeError::New(env, "Invalid curve requested").ThrowAsJavaScriptException();

}

/**
 * Adds a curve to the collection.
 *
 * @example
 *
 * var ring1 = new gdal.CircularString();
 * ring1.points.add(0,0);
 * ring1.points.add(1,0);
 * ring1.points.add(1,1);
 * ring1.points.add(0,1);
 * ring1.points.add(0,0);
 *
 * // one at a time:
 * compound.curves.add(ring1);
 *
 * // many at once:
 * compound.curves.add([ring1, ...]);
 *
 * @method add
 * @instance
 * @memberof CompoundCurveCurves
 * @param {SimpleCurve|SimpleCurve[]} curves
 */
Napi::Value CompoundCurveCurves::add(const Napi::CallbackInfo& info) {

  Napi::Object parent =
    Napi::GetPrivate(info.This(), Napi::String::New(env, "parent_")).As<Napi::Object>();
  CompoundCurve *geom = parent.Unwrap<CompoundCurve>();

  SimpleCurve *ring;

  if (info.Length() < 1) {
    Napi::Error::New(env, "curve(s) must be given").ThrowAsJavaScriptException();
    return env.Null();
  }
  if (info[0].IsArray()) {
    // set from array of geometry objects
    Napi::Array array = info[0].As<Napi::Array>();
    int length = array->Length();
    for (int i = 0; i < length; i++) {
      Napi::Value element = (array).Get(i);
      if (IS_WRAPPED(element, SimpleCurve)) {
        ring = element.As<Napi::Object>().Unwrap<SimpleCurve>();
        OGRErr err = geom->get()->addCurve(ring->get());
        if (err) {
          NODE_THROW_OGRERR(err);
          return;
        }
      } else {
        Napi::Error::New(env, "All array elements must be SimpleCurves").ThrowAsJavaScriptException();
        return env.Null();
      }
    }
  } else if (IS_WRAPPED(info[0], SimpleCurve)) {
    ring = info[0].As<Napi::Object>().Unwrap<SimpleCurve>();
    OGRErr err = geom->get()->addCurve(ring->get());
    if (err) {
      NODE_THROW_OGRERR(err);
      return;
    }
  } else {
    Napi::Error::New(env, "curve(s) must be a SimpleCurve or array of SimpleCurves").ThrowAsJavaScriptException();
    return env.Null();
  }

  return;
}

} // namespace node_gdal
