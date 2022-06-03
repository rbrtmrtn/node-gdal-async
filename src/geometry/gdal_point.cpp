#include "gdal_point.hpp"
#include "../gdal_common.hpp"
#include "gdal_geometry.hpp"

#include <stdlib.h>

namespace node_gdal {

Napi::FunctionReference Point::constructor;

void Point::Initialize(Napi::Object target) {
  Napi::HandleScope scope(env);

  Napi::FunctionReference lcons = Napi::Function::New(env, Point::New);
  lcons->Inherit(Napi::New(env, Geometry::constructor));

  lcons->SetClassName(Napi::String::New(env, "Point"));

  InstanceMethod("toString", &toString),

  // properties
  ATTR(lcons, "x", xGetter, xSetter);
  ATTR(lcons, "y", yGetter, ySetter);
  ATTR(lcons, "z", zGetter, zSetter);

  (target).Set(Napi::String::New(env, "Point"), Napi::GetFunction(lcons));

  constructor.Reset(lcons);
}

/**
 * Point class.
 *
 * @constructor
 * @class Point
 * @extends Geometry
 * @param {number} x
 * @param {number} y
 * @param {number} [z]
 */
Napi::Value Point::New(const Napi::CallbackInfo& info) {
  Point *f;
  OGRPoint *geom;
  double x = 0, y = 0, z = 0;

  if (!info.IsConstructCall()) {
    Napi::Error::New(env, "Cannot call constructor as function, you need to use 'new' keyword").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (info[0].IsExternal()) {
    Napi::External ext = info[0].As<Napi::External>();
    void *ptr = ext->Value();
    f = static_cast<Point *>(ptr);

  } else {
    NODE_ARG_DOUBLE_OPT(0, "x", x);
    NODE_ARG_DOUBLE_OPT(1, "y", y);
    NODE_ARG_DOUBLE_OPT(2, "z", z);

    if (info.Length() == 1) {
      Napi::Error::New(env, "Point constructor must be given 0, 2, or 3 arguments").ThrowAsJavaScriptException();
      return env.Null();
    }

    if (info.Length() == 3) {
      geom = new OGRPoint(x, y, z);
    } else {
      geom = new OGRPoint(x, y);
    }

    f = new Point(geom);
  }

  f->Wrap(info.This());
  return info.This();
}

Napi::Value Point::toString(const Napi::CallbackInfo& info) {
  return Napi::String::New(env, "Point");
}

/**
 * @kind member
 * @name x
 * @instance
 * @memberof Point
 * @type {number}
 */
Napi::Value Point::xGetter(const Napi::CallbackInfo& info) {
  Point *geom = info.This().Unwrap<Point>();
  return Napi::Number::New(env, (geom->this_)->getX());
}

void Point::xSetter(const Napi::CallbackInfo& info, const Napi::Value& value) {
  Point *geom = info.This().Unwrap<Point>();

  if (!value.IsNumber()) {
    Napi::Error::New(env, "y must be a number").ThrowAsJavaScriptException();
    return env.Null();
  }
  double x = value.As<Napi::Number>().DoubleValue().ToChecked();

  ((OGRPoint *)geom->this_)->setX(x);
}

/**
 * @kind member
 * @name y
 * @instance
 * @memberof Point
 * @type {number}
 */
Napi::Value Point::yGetter(const Napi::CallbackInfo& info) {
  Point *geom = info.This().Unwrap<Point>();
  return Napi::Number::New(env, (geom->this_)->getY());
}

void Point::ySetter(const Napi::CallbackInfo& info, const Napi::Value& value) {
  Point *geom = info.This().Unwrap<Point>();

  if (!value.IsNumber()) {
    Napi::Error::New(env, "y must be a number").ThrowAsJavaScriptException();
    return env.Null();
  }
  double y = value.As<Napi::Number>().DoubleValue().ToChecked();

  ((OGRPoint *)geom->this_)->setY(y);
}

/**
 * @kind member
 * @name z
 * @instance
 * @memberof Point
 * @type {number}
 */
Napi::Value Point::zGetter(const Napi::CallbackInfo& info) {
  Point *geom = info.This().Unwrap<Point>();
  return Napi::Number::New(env, (geom->this_)->getZ());
}

void Point::zSetter(const Napi::CallbackInfo& info, const Napi::Value& value) {
  Point *geom = info.This().Unwrap<Point>();

  if (!value.IsNumber()) {
    Napi::Error::New(env, "z must be a number").ThrowAsJavaScriptException();
    return env.Null();
  }
  double z = value.As<Napi::Number>().DoubleValue().ToChecked();

  ((OGRPoint *)geom->this_)->setZ(z);
}

} // namespace node_gdal
