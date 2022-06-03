#include "gdal_drivers.hpp"
#include "../gdal_common.hpp"
#include "../gdal_driver.hpp"

namespace node_gdal {

Napi::FunctionReference GDALDrivers::constructor;

void GDALDrivers::Initialize(Napi::Object target) {
  Napi::HandleScope scope(env);

  Napi::FunctionReference lcons = Napi::Function::New(env, GDALDrivers::New);

  lcons->SetClassName(Napi::String::New(env, "GDALDrivers"));

  InstanceMethod("toString", &toString),
  InstanceMethod("count", &count),
  InstanceMethod("get", &get),
  InstanceMethod("getNames", &getNames),

  GDALAllRegister();

  (target).Set(Napi::String::New(env, "GDALDrivers"), Napi::GetFunction(lcons));

  constructor.Reset(lcons);
}

GDALDrivers::GDALDrivers() : Napi::ObjectWrap<GDALDrivers>(){
}

GDALDrivers::~GDALDrivers() {
}

/**
 * An collection of all {@link Driver}
 * registered with GDAL.
 *
 * @class GDALDrivers
 */
Napi::Value GDALDrivers::New(const Napi::CallbackInfo& info) {

  if (!info.IsConstructCall()) {
    Napi::Error::New(env, "Cannot call constructor as function, you need to use 'new' keyword").ThrowAsJavaScriptException();
    return env.Null();
  }
  if (info[0].IsExternal()) {
    Napi::External ext = info[0].As<Napi::External>();
    void *ptr = ext->Value();
    GDALDrivers *f = static_cast<GDALDrivers *>(ptr);
    f->Wrap(info.This());
    return info.This();
    return;
  } else {
    Napi::Error::New(env, "Cannot create GDALDrivers directly").ThrowAsJavaScriptException();
    return env.Null();
  }
}

Napi::Value GDALDrivers::New() {
  Napi::EscapableHandleScope scope(env);

  GDALDrivers *wrapped = new GDALDrivers();

  Napi::Value ext = Napi::External::New(env, wrapped);
  Napi::Object obj =
    Napi::NewInstance(Napi::GetFunction(Napi::New(env, GDALDrivers::constructor)), 1, &ext);

  return scope.Escape(obj);
}

Napi::Value GDALDrivers::toString(const Napi::CallbackInfo& info) {
  return Napi::String::New(env, "GDALDrivers");
}

/**
 * Returns a driver with the specified name.
 *
 * Note: Prior to GDAL2.x there is a separate driver for vector VRTs and raster
 * VRTs. Use `"VRT:vector"` to fetch the vector VRT driver and `"VRT:raster"` to
 * fetch the raster VRT driver.
 *
 * @method get
 * @instance
 * @memberof GDALDrivers
 * @param {number|string} index 0-based index or driver name
 * @throws {Error}
 * @return {Driver}
 */
Napi::Value GDALDrivers::get(const Napi::CallbackInfo& info) {

  GDALDriver *gdal_driver;

  if (info.Length() == 0) {
    Napi::Error::New(env, "Either driver name or index must be provided").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (info[0].IsString()) {
    // try getting OGR driver first, and then GDAL driver if it fails
    // A driver named "VRT" exists for both GDAL and OGR, so if building
    // with <2.0 require user to specify which driver to pick
    std::string name = info[0].As<Napi::String>().Utf8Value().c_str();

    if (name == "VRT:vector") { name = "VRT"; }

    if (name == "VRT:raster") { name = "VRT"; }
    gdal_driver = GetGDALDriverManager()->GetDriverByName(name.c_str());
    if (gdal_driver) {
      return Driver::New(gdal_driver);
      return;
    }

  } else if (info[0].IsNumber()) {
    int i = static_cast<int>(info[0].As<Napi::Number>().Int64Value().ToChecked());

    gdal_driver = GetGDALDriverManager()->GetDriver(i);
    if (gdal_driver) {
      return Driver::New(gdal_driver);
      return;
    }

  } else {
    Napi::Error::New(env, "Argument must be string or integer").ThrowAsJavaScriptException();
    return env.Null();
  }

  NODE_THROW_LAST_CPLERR;
}

/**
 * Returns an array with the names of all the drivers registered with GDAL.
 *
 * @method getNames
 * @instance
 * @memberof GDALDrivers
 * @return {string[]}
 */
Napi::Value GDALDrivers::getNames(const Napi::CallbackInfo& info) {
  int gdal_count = GetGDALDriverManager()->GetDriverCount();
  int i, ogr_count = 0;
  std::string name;

  int n = gdal_count + ogr_count;

  Napi::Array driver_names = Napi::Array::New(env, n);

  for (i = 0; i < gdal_count; ++i) {
    GDALDriver *driver = GetGDALDriverManager()->GetDriver(i);
    name = driver->GetDescription();
    (driver_names).Set(i, SafeString::New(name.c_str()));
  }

  return driver_names;
}

/**
 * Returns the number of drivers registered with GDAL.
 *
 * @method count
 * @instance
 * @memberof GDALDrivers
 * @return {number}
 */
Napi::Value GDALDrivers::count(const Napi::CallbackInfo& info) {

  int count = GetGDALDriverManager()->GetDriverCount();

  return Napi::Number::New(env, count);
}

} // namespace node_gdal
