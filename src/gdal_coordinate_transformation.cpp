#include <string>
#include "gdal_coordinate_transformation.hpp"
#include "gdal_common.hpp"
#include "gdal_dataset.hpp"
#include "gdal_spatial_reference.hpp"
#ifdef BUNDLED_GDAL
#include "proj.h"
#endif

namespace node_gdal {

Napi::FunctionReference CoordinateTransformation::constructor;

void CoordinateTransformation::Initialize(Napi::Object target) {
  Napi::HandleScope scope(env);

  Napi::FunctionReference lcons = Napi::Function::New(env, CoordinateTransformation::New);

  lcons->SetClassName(Napi::String::New(env, "CoordinateTransformation"));

  InstanceMethod("toString", &toString),
  InstanceMethod("transformPoint", &transformPoint),

  (target).Set(Napi::String::New(env, "CoordinateTransformation"), Napi::GetFunction(lcons));

  constructor.Reset(lcons);
}

CoordinateTransformation::CoordinateTransformation(OGRCoordinateTransformation *transform) : Napi::ObjectWrap<CoordinateTransformation>(), this_(transform) {
  LOG("Created CoordinateTransformation [%p]", transform);
}

CoordinateTransformation::CoordinateTransformation() : Napi::ObjectWrap<CoordinateTransformation>(), this_(0) {
}

CoordinateTransformation::~CoordinateTransformation() {
  if (this_) {
    LOG("Disposing CoordinateTransformation [%p]", this_);
    OGRCoordinateTransformation::DestroyCT(this_);
    LOG("Disposed CoordinateTransformation [%p]", this_);
    this_ = NULL;
  }
}

/**
 * Object for transforming between coordinate systems.
 *
 * @throws {Error}
 * @constructor
 * @class CoordinateTransformation
 * @param {SpatialReference} source
 * @param {SpatialReference|Dataset} target If a raster Dataset, the
 * conversion will represent a conversion to pixel coordinates.
 */
Napi::Value CoordinateTransformation::New(const Napi::CallbackInfo& info) {
  CoordinateTransformation *f;
  SpatialReference *source, *target;

  if (!info.IsConstructCall()) {
    Napi::Error::New(env, "Cannot call constructor as function, you need to use 'new' keyword").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (info[0].IsExternal()) {
    Napi::External ext = info[0].As<Napi::External>();
    void *ptr = ext->Value();
    f = static_cast<CoordinateTransformation *>(ptr);
  } else {
    if (info.Length() < 2) {
      Napi::Error::New(env, "Invalid number of arguments").ThrowAsJavaScriptException();
      return env.Null();
    }

    NODE_ARG_WRAPPED(0, "source", SpatialReference, source);

    if (!info[1].IsObject() || info[1].IsNull()) {
      Napi::TypeError::New(env, "target must be a SpatialReference or Dataset object").ThrowAsJavaScriptException();
      return env.Null();
    }
    if (Napi::New(env, SpatialReference::constructor)->HasInstance(info[1])) {
      // srs -> srs
      NODE_ARG_WRAPPED(1, "target", SpatialReference, target);

      OGRCoordinateTransformation *transform = OGRCreateCoordinateTransformation(source->get(), target->get());
      if (!transform) {
        NODE_THROW_LAST_CPLERR;
        return;
      }
      f = new CoordinateTransformation(transform);
    } else if (Napi::New(env, Dataset::constructor)->HasInstance(info[1])) {
      // srs -> px/line
      // todo: allow additional options using StringList

      Dataset *ds;
      char **papszTO = NULL;
      char *src_wkt;

      ds = info[1].As<Napi::Object>().Unwrap<Dataset>();

      if (!ds->get()) {
        Napi::Error::New(env, "Dataset already closed").ThrowAsJavaScriptException();
        return env.Null();
      }

      OGRErr err = source->get()->exportToWkt(&src_wkt);
      if (err) {
        NODE_THROW_OGRERR(err);
        return;
      }

      papszTO = CSLSetNameValue(papszTO, "DST_SRS", src_wkt);
      papszTO = CSLSetNameValue(papszTO, "INSERT_CENTER_LONG", "FALSE");

      GeoTransformTransformer *transform = new GeoTransformTransformer();
      transform->hSrcImageTransformer = GDALCreateGenImgProjTransformer2(ds->get(), NULL, papszTO);
      if (!transform->hSrcImageTransformer) {
        NODE_THROW_LAST_CPLERR;
        return;
      }

      f = new CoordinateTransformation(transform);

      CPLFree(src_wkt);
      CSLDestroy(papszTO);
    } else {
      Napi::TypeError::New(env, "target must be a SpatialReference or Dataset object").ThrowAsJavaScriptException();
      return env.Null();
    }
  }

  f->Wrap(info.This());
  return info.This();
}

Napi::Value CoordinateTransformation::New(OGRCoordinateTransformation *transform) {
  Napi::EscapableHandleScope scope(env);

  if (!transform) { return scope.Escape(env.Null()); }

  CoordinateTransformation *wrapped = new CoordinateTransformation(transform);

  Napi::Value ext = Napi::External::New(env, wrapped);
  Napi::Object obj =
    Napi::NewInstance(Napi::GetFunction(Napi::New(env, CoordinateTransformation::constructor)), 1, &ext)
      ;

  return scope.Escape(obj);
}

Napi::Value CoordinateTransformation::toString(const Napi::CallbackInfo& info) {
  return Napi::String::New(env, "CoordinateTransformation");
}

/**
 * Transform point from source to destination space.
 *
 * @example
 *
 * pt = transform.transformPoint(0, 0, 0);
 *
 * @method transformPoint
 * @instance
 * @memberof CoordinateTransformation
 * @param {number} x
 * @param {number} y
 * @param {number} [z]
 * @return {xyz} A regular object containing `x`, `y`, `z` properties.
 */

/**
 * Transform point from source to destination space.
 *
 * @example
 *
 * pt = transform.transformPoint({x: 0, y: 0, z: 0});
 *
 * @method transformPoint
 * @instance
 * @memberof CoordinateTransformation
 * @param {xyz} point
 * @return {xyz} A regular object containing `x`, `y`, `z` properties.
 */
Napi::Value CoordinateTransformation::transformPoint(const Napi::CallbackInfo& info) {
  CoordinateTransformation *transform = info.This().Unwrap<CoordinateTransformation>();

  double x, y, z = 0;

  if (info.Length() == 1 && info[0].IsObject()) {
    Napi::Object obj = info[0].As<Napi::Object>();
    Napi::Value arg_x = (obj).Get(Napi::String::New(env, "x"));
    Napi::Value arg_y = (obj).Get(Napi::String::New(env, "y"));
    Napi::Value arg_z = (obj).Get(Napi::String::New(env, "z"));
    if (!arg_x.IsNumber() || !arg_y.IsNumber()) {
      Napi::Error::New(env, "point must contain numerical properties x and y").ThrowAsJavaScriptException();
      return env.Null();
    }
    x = static_cast<double>(arg_x.As<Napi::Number>().DoubleValue().ToChecked());
    y = static_cast<double>(arg_y.As<Napi::Number>().DoubleValue().ToChecked());
    if (arg_z.IsNumber()) { z = static_cast<double>(arg_z.As<Napi::Number>().DoubleValue().ToChecked()); }
  } else {
    NODE_ARG_DOUBLE(0, "x", x);
    NODE_ARG_DOUBLE(1, "y", y);
    NODE_ARG_DOUBLE_OPT(2, "z", z);
  }

#ifdef BUNDLED_GDAL
  int proj_error_code = 0;
  int r = transform->this_->TransformWithErrorCodes(1, &x, &y, &z, nullptr, &proj_error_code);
  if (!r || proj_error_code != 0) {
    Napi::ThrowError(
      ("Error transforming point: " + std::string(proj_context_errno_string(nullptr, proj_error_code))).c_str());
    return;
  }
#else
  if (!transform->this_->Transform(1, &x, &y, &z)) {
    Napi::Error::New(env, "Error transforming point").ThrowAsJavaScriptException();
    return env.Null();
  }
#endif

  Napi::Object result = Napi::Object::New(env);
  (result).Set(Napi::String::New(env, "x"), Napi::Number::New(env, x));
  (result).Set(Napi::String::New(env, "y"), Napi::Number::New(env, y));
  (result).Set(Napi::String::New(env, "z"), Napi::Number::New(env, z));

  return result;
}

} // namespace node_gdal
