#include "linestring_points.hpp"
#include "../gdal_common.hpp"
#include "../geometry/gdal_geometry.hpp"
#include "../geometry/gdal_linestring.hpp"
#include "../geometry/gdal_point.hpp"

namespace node_gdal {

Napi::FunctionReference LineStringPoints::constructor;

void LineStringPoints::Initialize(Napi::Object target) {
  Napi::HandleScope scope(env);

  Napi::FunctionReference lcons = Napi::Function::New(env, LineStringPoints::New);

  lcons->SetClassName(Napi::String::New(env, "LineStringPoints"));

  InstanceMethod("toString", &toString),
  InstanceMethod("count", &count),
  InstanceMethod("get", &get),
  InstanceMethod("set", &set),
  InstanceMethod("add", &add),
  InstanceMethod("reverse", &reverse),
  InstanceMethod("resize", &resize),

  (target).Set(Napi::String::New(env, "LineStringPoints"), Napi::GetFunction(lcons));

  constructor.Reset(lcons);
}

LineStringPoints::LineStringPoints() : Napi::ObjectWrap<LineStringPoints>(){
}

LineStringPoints::~LineStringPoints() {
}

/**
 * An encapsulation of a {@link LineString}'s points.
 *
 * @class LineStringPoints
 */
Napi::Value LineStringPoints::New(const Napi::CallbackInfo& info) {

  if (!info.IsConstructCall()) {
    Napi::Error::New(env, "Cannot call constructor as function, you need to use 'new' keyword").ThrowAsJavaScriptException();
    return env.Null();
  }
  if (info[0].IsExternal()) {
    Napi::External ext = info[0].As<Napi::External>();
    void *ptr = ext->Value();
    LineStringPoints *geom = static_cast<LineStringPoints *>(ptr);
    geom->Wrap(info.This());
    return info.This();
    return;
  } else {
    Napi::Error::New(env, "Cannot create LineStringPoints directly").ThrowAsJavaScriptException();
    return env.Null();
  }
}

Napi::Value LineStringPoints::New(Napi::Value geom) {
  Napi::Env env = geom.Env();
  Napi::EscapableHandleScope scope(env);

  LineStringPoints *wrapped = new LineStringPoints();

  Napi::Value ext = Napi::External::New(env, wrapped);
  Napi::Object obj =
    Napi::NewInstance(Napi::GetFunction(Napi::New(env, LineStringPoints::constructor)), 1, &ext)
      ;
  Napi::SetPrivate(obj, Napi::String::New(env, "parent_"), geom);

  return scope.Escape(obj);
}

Napi::Value LineStringPoints::toString(const Napi::CallbackInfo& info) {
  return Napi::String::New(env, "LineStringPoints");
}

/**
 * Returns the number of points that are part of the line string.
 *
 * @method count
 * @instance
 * @memberof LineStringPoints
 * @return {number}
 */
Napi::Value LineStringPoints::count(const Napi::CallbackInfo& info) {

  Napi::Object parent =
    Napi::GetPrivate(info.This(), Napi::String::New(env, "parent_")).As<Napi::Object>();
  LineString *geom = parent.Unwrap<LineString>();

  return Napi::Number::New(env, geom->get()->getNumPoints());
}

/**
 * Reverses the order of all the points.
 *
 * @method reverse
 * @instance
 * @memberof LineStringPoints
 */
Napi::Value LineStringPoints::reverse(const Napi::CallbackInfo& info) {

  Napi::Object parent =
    Napi::GetPrivate(info.This(), Napi::String::New(env, "parent_")).As<Napi::Object>();
  LineString *geom = parent.Unwrap<LineString>();

  geom->get()->reversePoints();

  return;
}

/**
 * Adjusts the number of points that make up the line string.
 *
 * @method resize
 * @instance
 * @memberof LineStringPoints
 * @param {number} count
 */
Napi::Value LineStringPoints::resize(const Napi::CallbackInfo& info) {

  Napi::Object parent =
    Napi::GetPrivate(info.This(), Napi::String::New(env, "parent_")).As<Napi::Object>();
  LineString *geom = parent.Unwrap<LineString>();

  int count;
  NODE_ARG_INT(0, "point count", count)
  geom->get()->setNumPoints(count);

  return;
}

/**
 * Returns the point at the specified index.
 *
 * @method get
 * @instance
 * @memberof LineStringPoints
 * @param {number} index 0-based index
 * @throws {Error}
 * @return {Point}
 */
Napi::Value LineStringPoints::get(const Napi::CallbackInfo& info) {

  Napi::Object parent =
    Napi::GetPrivate(info.This(), Napi::String::New(env, "parent_")).As<Napi::Object>();
  LineString *geom = parent.Unwrap<LineString>();

  OGRPoint pt;
  int i;

  NODE_ARG_INT(0, "index", i);
  if (i < 0 || i >= geom->get()->getNumPoints()) {
    Napi::Error::New(env, "Invalid point requested").ThrowAsJavaScriptException();
    return env.Null();
  }

  geom->get()->getPoint(i, &pt);

  // New will copy the point with GDAL clone()
  return Point::New(&pt, false);
}

/**
 * Sets the point at the specified index.
 *
 * @example
 *
 * lineString.points.set(0, new gdal.Point(1, 2));
 *
 * @method set
 * @instance
 * @memberof LineStringPoints
 * @throws {Error}
 * @param {number} index 0-based index
 * @param {Point|xyz} point
 */

/**
 * @method set
 * @instance
 * @memberof LineStringPoints
 * @throws {Error}
 * @param {number} index 0-based index
 * @param {number} x
 * @param {number} y
 * @param {number} [z]
 */
Napi::Value LineStringPoints::set(const Napi::CallbackInfo& info) {

  Napi::Object parent =
    Napi::GetPrivate(info.This(), Napi::String::New(env, "parent_")).As<Napi::Object>();
  LineString *geom = parent.Unwrap<LineString>();

  int i;
  NODE_ARG_INT(0, "index", i);
  if (i < 0 || i >= geom->get()->getNumPoints()) {
    Napi::Error::New(env, "Point index out of range").ThrowAsJavaScriptException();
    return env.Null();
  }

  int n = info.Length() - 1;

  if (n == 0) {
    Napi::Error::New(env, "Point must be given").ThrowAsJavaScriptException();
    return env.Null();
  } else if (n == 1) {
    if (!info[1].IsObject()) {
      Napi::Error::New(env, "Point or object expected for second argument").ThrowAsJavaScriptException();
      return env.Null();
    }
    if (IS_WRAPPED(info[1], Point)) {
      // set from Point object
      Point *pt = info[1].As<Napi::Object>().Unwrap<Point>();
      geom->get()->setPoint(i, pt->get());
    } else {
      Napi::Object obj = info[1].As<Napi::Object>();
      // set from object {x: 0, y: 5}
      double x, y;
      NODE_DOUBLE_FROM_OBJ(obj, "x", x);
      NODE_DOUBLE_FROM_OBJ(obj, "y", y);

      Napi::String z_prop_name = Napi::String::New(env, "z");
      if (Napi::HasOwnProperty(obj, z_prop_name).FromMaybe(false)) {
        Napi::Value z_val = (obj).Get(z_prop_name);
        if (!z_val.IsNumber()) {
          Napi::Error::New(env, "z property must be number").ThrowAsJavaScriptException();
          return env.Null();
        }
        geom->get()->setPoint(i, x, y, z_val.As<Napi::Number>().DoubleValue().ToChecked());
      } else {
        geom->get()->setPoint(i, x, y);
      }
    }
  } else {
    // set x, y, z from numeric arguments
    if (!info[1].IsNumber()) {
      Napi::Error::New(env, "Number expected for second argument").ThrowAsJavaScriptException();
      return env.Null();
    }
    if (!info[2].IsNumber()) {
      Napi::Error::New(env, "Number expected for third argument").ThrowAsJavaScriptException();
      return env.Null();
    }
    if (n == 2) {
      geom->get()->setPoint(i, info[1].As<Napi::Number>().DoubleValue().ToChecked(), info[2].As<Napi::Number>().DoubleValue().ToChecked());
    } else {
      if (!info[3].IsNumber()) {
        Napi::Error::New(env, "Number expected for fourth argument").ThrowAsJavaScriptException();
        return env.Null();
      }

      geom->get()->setPoint(
        i,
        info[1].As<Napi::Number>().DoubleValue().ToChecked(),
        info[2].As<Napi::Number>().DoubleValue().ToChecked(),
        info[3].As<Napi::Number>().DoubleValue().ToChecked());
    }
  }

  return;
}

/**
 * Adds point(s) to the line string. Also accepts any object with an x and y
 * property.
 *
 * @example
 *
 * lineString.points.add(new gdal.Point(1, 2));
 * lineString.points.add([
 *     new gdal.Point(1, 2)
 *     new gdal.Point(3, 4)
 * ]);
 *
 * @method add
 * @instance
 * @memberof LineStringPoints
 * @throws {Error}
 * @param {Point|xyz|(Point|xyz)[]} points
 */

/**
 *
 * @method add
 * @instance
 * @memberof LineStringPoints
 * @throws {Error}
 * @param {number} x
 * @param {number} y
 * @param {number} [z]
 */
Napi::Value LineStringPoints::add(const Napi::CallbackInfo& info) {

  Napi::Object parent =
    Napi::GetPrivate(info.This(), Napi::String::New(env, "parent_")).As<Napi::Object>();
  LineString *geom = parent.Unwrap<LineString>();

  int n = info.Length();

  if (n == 0) {
    Napi::Error::New(env, "Point must be given").ThrowAsJavaScriptException();
    return env.Null();
  } else if (n == 1) {
    if (!info[0].IsObject()) {
      Napi::Error::New(env, "Point, object, or array of points expected").ThrowAsJavaScriptException();
      return env.Null();
    }
    if (IS_WRAPPED(info[0], Point)) {
      // set from Point object
      Point *pt = info[0].As<Napi::Object>().Unwrap<Point>();
      geom->get()->addPoint(pt->get());
    } else if (info[0].IsArray()) {
      // set from array of points
      Napi::Array array = info[0].As<Napi::Array>();
      int length = array->Length();
      for (int i = 0; i < length; i++) {
        Napi::Value element = (array).Get(i);
        if (!element.IsObject()) {
          Napi::Error::New(env, "All points must be Point objects or objects").ThrowAsJavaScriptException();
          return env.Null();
        }
        Napi::Object element_obj = element.As<Napi::Object>();
        if (IS_WRAPPED(element_obj, Point)) {
          // set from Point object
          Point *pt = element_obj.Unwrap<Point>();
          geom->get()->addPoint(pt->get());
        } else {
          // set from object {x: 0, y: 5}
          double x, y;
          NODE_DOUBLE_FROM_OBJ(element_obj, "x", x);
          NODE_DOUBLE_FROM_OBJ(element_obj, "y", y);

          Napi::String z_prop_name = Napi::String::New(env, "z");
          if (Napi::HasOwnProperty(element_obj, z_prop_name).FromMaybe(false)) {
            Napi::Value z_val = (element_obj).Get(z_prop_name);
            if (!z_val.IsNumber()) {
              Napi::Error::New(env, "z property must be number").ThrowAsJavaScriptException();
              return env.Null();
            }
            geom->get()->addPoint(x, y, z_val.As<Napi::Number>().DoubleValue().ToChecked());
          } else {
            geom->get()->addPoint(x, y);
          }
        }
      }
    } else {
      // set from object {x: 0, y: 5}
      Napi::Object obj = info[0].As<Napi::Object>();
      double x, y;
      NODE_DOUBLE_FROM_OBJ(obj, "x", x);
      NODE_DOUBLE_FROM_OBJ(obj, "y", y);

      Napi::String z_prop_name = Napi::String::New(env, "z");
      if (Napi::HasOwnProperty(obj, z_prop_name).FromMaybe(false)) {
        Napi::Value z_val = (obj).Get(z_prop_name);
        if (!z_val.IsNumber()) {
          Napi::Error::New(env, "z property must be number").ThrowAsJavaScriptException();
          return env.Null();
        }
        geom->get()->addPoint(x, y, z_val.As<Napi::Number>().DoubleValue().ToChecked());
      } else {
        geom->get()->addPoint(x, y);
      }
    }
  } else {
    // set x, y, z from numeric arguments
    if (!info[0].IsNumber()) {
      Napi::Error::New(env, "Number expected for first argument").ThrowAsJavaScriptException();
      return env.Null();
    }
    if (!info[1].IsNumber()) {
      Napi::Error::New(env, "Number expected for second argument").ThrowAsJavaScriptException();
      return env.Null();
    }
    if (n == 2) {
      geom->get()->addPoint(info[0].As<Napi::Number>().DoubleValue().ToChecked(), info[1].As<Napi::Number>().DoubleValue().ToChecked());
    } else {
      if (!info[2].IsNumber()) {
        Napi::Error::New(env, "Number expected for third argument").ThrowAsJavaScriptException();
        return env.Null();
      }

      geom->get()->addPoint(
        info[0].As<Napi::Number>().DoubleValue().ToChecked(),
        info[1].As<Napi::Number>().DoubleValue().ToChecked(),
        info[2].As<Napi::Number>().DoubleValue().ToChecked());
    }
  }

  return;
}

} // namespace node_gdal
