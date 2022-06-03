// node
#include <napi.h>
#include <uv.h>

// nan
#include <napi.h>

#include <string>

#include "gdal_common.hpp"

using namespace Napi;

void READ_ONLY_SETTER(const Napi::CallbackInfo &info, const Napi::Value &value) {
  std::string name = property.As<Napi::String>().Utf8Value().c_str();
  std::string err = name + " is a read-only property";
  Napi::Error::New(env, err.c_str()).ThrowAsJavaScriptException();
}
