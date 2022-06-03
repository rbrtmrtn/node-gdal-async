#ifndef __GDAL_COMMON_H__
#define __GDAL_COMMON_H__

#include <cpl_error.h>
#include <gdal_version.h>
#include <thread>
#include <stdio.h>

// nan
#include <napi.h>

#include "utils/ptr_manager.hpp"

#if GDAL_VERSION_MAJOR < 2 || (GDAL_VERSION_MAJOR == 2 && GDAL_VERSION_MINOR < 1)
#error gdal-async now requires GDAL >= 2.1, downgrade to gdal-async@3.2.x for earlier versions
#endif

namespace node_gdal {
extern FILE *log_file;
extern ObjectStore object_store;
extern bool eventLoopWarn;
} // namespace node_gdal

#ifdef ENABLE_LOGGING
#define LOG(fmt, ...)                                                                                                  \
  if (node_gdal::log_file) {                                                                                           \
    fprintf(node_gdal::log_file, fmt "\n", __VA_ARGS__);                                                               \
    fflush(node_gdal::log_file);                                                                                       \
  }
#else
#define LOG(fmt, ...)
#endif

// Napi::New(env, null) -> seg fault
class SafeString {
    public:
  static Napi::Value New(Napi::Env env, const char *data) {
    Napi::EscapableHandleScope scope(env);
    if (!data) {
      return scope.Escape(env.Null());
    } else {
      return scope.Escape(Napi::String::New(env, data));
    }
  }
};

inline const char *getOGRErrMsg(int err) {
  if (err == 6) {
    // get more descriptive error
    // TODO: test if all OGRErr failures report an error msg
    return CPLGetLastErrorMsg();
  }
  switch (err) {
    case 0: return "No error";
    case 1: return "Not enough data";
    case 2: return "Not enough memory";
    case 3: return "Unsupported geometry type";
    case 4: return "Unsupported operation";
    case 5: return "Corrupt Data";
    case 6: return "Failure";
    case 7: return "Unsupported SRS";
    default: return "Invalid Error";
  }
};

// deleter for C++14 shared_ptr which does not have built-in array support
template <typename T> struct array_deleter {
  void operator()(T const *p) {
    delete[] p;
  }
};

template <typename INPUT, typename RETURN>
std::shared_ptr<RETURN> NumberArrayToSharedPtr(Napi::Array array, size_t count = 0) {
  if (array.IsEmpty()) return nullptr;
  if (count != 0 && array->Length() != count) throw "Array size must match the number of dimensions";
  std::shared_ptr<RETURN> ptr(new RETURN[array->Length()], array_deleter<RETURN>());
  for (unsigned i = 0; i < array->Length(); i++) {
    Napi::Value val = (array).Get(i);
    if (!val.IsNumber()) throw "Array must contain only numbers";
    ptr.get()[i] = static_cast<RETURN>(Napi::To<INPUT>(val).ToChecked());
  }

  return ptr;
}

#define NODE_THROW_LAST_CPLERR Napi::ThrowError::New(env, CPLGetLastErrorMsg())

#define NODE_THROW_OGRERR(err) Napi::ThrowError::New(env, getOGRErrMsg(err))

#define ATTR(t, name, get, set) Napi::SetAccessor(t->InstanceTemplate(), Napi::New(env, name), get, set);

#define ATTR_ASYNCABLE(t, name, get, set)                                                                              \
  Napi::SetAccessor(t->InstanceTemplate(), Napi::New(env, name), get, set);                                            \
  Napi::SetAccessor(                                                                                                   \
    t->InstanceTemplate(),                                                                                             \
    Napi::New(env, name "Async"),                                                                                      \
    get##Async,                                                                                                        \
    READ_ONLY_SETTER,                                                                                                  \
    Napi::Value(),                                                                                                     \
    DEFAULT,                                                                                                           \
    DontEnum);

#define ATTR_DONT_ENUM(t, name, get, set)                                                                              \
  Napi::SetAccessor(t->InstanceTemplate(), Napi::New(env, name), get, set, Napi::Value(), DEFAULT, DontEnum);

void READ_ONLY_SETTER(const Napi::CallbackInfo &info, const Napi::Value &value);

#define IS_WRAPPED(obj, type) Napi::New(env, type::constructor)->HasInstance(obj)

// ----- async method definition shortcuts ------
#define InstanceAsyncableMethod(name, method)                                                                          \
  InstanceMethod<&method>(name, static_cast<napi_property_attributes>(napi_writable | napi_configurable)),             \
    InstanceMethod<&method##Async>(                                                                                    \
      name "Async", static_cast<napi_property_attributes>(napi_writable | napi_configurable))

#define PrototypeAsyncableMethod(name, method)                                                                         \
  StaticMethod<&method>(name, static_cast<napi_property_attributes>(napi_writable | napi_configurable)),               \
    StaticMethod<&method##Async>(                                                                                      \
      name "Async", static_cast<napi_property_attributes>(napi_writable | napi_configurable))

#define NODE_UNWRAP_CHECK(type, obj, var)                                                                              \
  if (!obj.IsObject() || obj.IsNull() || !Napi::New(env, type::constructor)->HasInstance(obj)) {                       \
    Napi::TypeError::New(env, "Object must be a " #type " object").ThrowAsJavaScriptException();                       \
    return;                                                                                                            \
  }                                                                                                                    \
  type *var = obj.As<Napi::Object>().Unwrap<type>();                                                                   \
  if (!var->isAlive()) {                                                                                               \
    Napi::Error::New(env, #type " object has already been destroyed").ThrowAsJavaScriptException();                    \
    return;                                                                                                            \
  }

#define GDAL_RAW_CHECK(type, obj, var)                                                                                 \
  type var = obj->get();                                                                                               \
  if (!obj) {                                                                                                          \
    Napi::Error::New(env, #type " object has already been destroyed").ThrowAsJavaScriptException();                    \
    return;                                                                                                            \
  }

// Defines to be used in async getters (returning a Promise)
// DO NOT USE IN A NORMAL ASYNC METHOD
// It will return a rejected Promise on error
#define NODE_UNWRAP_CHECK_ASYNC(type, obj, var)                                                                        \
  if (!obj.IsObject() || obj.IsNull() || !Napi::New(env, type::constructor)->HasInstance(obj)) {                       \
    THROW_OR_REJECT("Object must be a " #type " object");                                                              \
    return;                                                                                                            \
  }                                                                                                                    \
  type *var = obj.As<Napi::Object>().Unwrap<type>();                                                                   \
  if (!var->isAlive()) {                                                                                               \
    THROW_OR_REJECT(#type " object has already been destroyed");                                                       \
    return;                                                                                                            \
  }

#define GDAL_RAW_CHECK_ASYNC(type, obj, var)                                                                           \
  type var = obj->get();                                                                                               \
  if (!obj) { THROW_OR_REJECT(#type " object has already been destroyed"); }

// ----- object property conversion -------

#define NODE_DOUBLE_FROM_OBJ(obj, key, var)                                                                            \
  {                                                                                                                    \
    Napi::String sym = Napi::New(env, key);                                                                            \
    if (!Napi::HasOwnProperty(obj, sym).FromMaybe(false)) {                                                            \
      Napi::Error::New(env, "Object must contain property \"" key "\"").ThrowAsJavaScriptException();                  \
      return;                                                                                                          \
    }                                                                                                                  \
    Napi::Value val = (obj).Get(sym);                                                                                  \
    if (!val.IsNumber()) {                                                                                             \
      Napi::TypeError::New(env, "Property \"" key "\" must be a number").ThrowAsJavaScriptException();                 \
      return;                                                                                                          \
    }                                                                                                                  \
    var = val.As<Napi::Number>().DoubleValue().ToChecked();                                                            \
  }

#define NODE_INT_FROM_OBJ(obj, key, var)                                                                               \
  {                                                                                                                    \
    Napi::String sym = Napi::New(env, key);                                                                            \
    if (!Napi::HasOwnProperty(obj, sym).FromMaybe(false)) {                                                            \
      Napi::Error::New(env, "Object must contain property \"" key "\"").ThrowAsJavaScriptException();                  \
      return;                                                                                                          \
    }                                                                                                                  \
    Napi::Value val = (obj).Get(sym);                                                                                  \
    if (!val.IsNumber()) {                                                                                             \
      Napi::TypeError::New(env, "Property \"" key "\" must be a number").ThrowAsJavaScriptException();                 \
      return;                                                                                                          \
    }                                                                                                                  \
    var = val.As<Napi::Number>().Int32Value().ToChecked();                                                             \
  }

#define NODE_STR_FROM_OBJ(obj, key, var)                                                                               \
  {                                                                                                                    \
    Napi::String sym = Napi::New(env, key);                                                                            \
    if (!Napi::HasOwnProperty(obj, sym).FromMaybe(false)) {                                                            \
      Napi::Error::New(env, "Object must contain property \"" key "\"").ThrowAsJavaScriptException();                  \
      return;                                                                                                          \
    }                                                                                                                  \
    Napi::Value val = (obj).Get(sym);                                                                                  \
    if (!val.IsString()) {                                                                                             \
      Napi::TypeError::New(env, "Property \"" key "\" must be a string").ThrowAsJavaScriptException();                 \
      return;                                                                                                          \
    }                                                                                                                  \
    var = val.As<Napi::String>().Utf8Value().c_str();                                                                  \
  }

#define NODE_ARRAY_FROM_OBJ(obj, key, var)                                                                             \
  {                                                                                                                    \
    Napi::String sym = Napi::New(env, key);                                                                            \
    if (!Napi::HasOwnProperty(obj, sym).FromMaybe(false)) {                                                            \
      Napi::Error::New(env, "Object must contain property \"" key "\"").ThrowAsJavaScriptException();                  \
      return;                                                                                                          \
    }                                                                                                                  \
    Napi::Value val = (obj).Get(sym);                                                                                  \
    if (!val->IsArray()) {                                                                                             \
      Napi::TypeError::New(env, "Property \"" key "\" must be an array").ThrowAsJavaScriptException();                 \
      return;                                                                                                          \
    }                                                                                                                  \
    var = val.As<Napi::Array>();                                                                                       \
  }

#define NODE_ARRAY_FROM_OBJ_OPT(obj, key, var)                                                                         \
  {                                                                                                                    \
    Napi::String sym = Napi::New(env, key);                                                                            \
    if (Napi::HasOwnProperty(obj, sym).FromMaybe(false)) {                                                             \
      Napi::Value val = (obj).Get(sym);                                                                                \
      if (!val.IsNull() && !val.IsNull()) {                                                                            \
        if (!val->IsArray()) {                                                                                         \
          Napi::TypeError::New(env, "Property \"" key "\" must be an array").ThrowAsJavaScriptException();             \
          return;                                                                                                      \
        }                                                                                                              \
        var = val.As<Napi::Array>();                                                                                   \
      }                                                                                                                \
    }                                                                                                                  \
  }

#define NODE_WRAPPED_FROM_OBJ(obj, key, type, var)                                                                     \
  {                                                                                                                    \
    Napi::String sym = Napi::New(env, key);                                                                            \
    if (!Napi::HasOwnProperty(obj, sym).FromMaybe(false)) {                                                            \
      Napi::Error::New(env, "Object must contain property \"" key "\"").ThrowAsJavaScriptException();                  \
      return;                                                                                                          \
    }                                                                                                                  \
    Napi::Value val = (obj).Get(sym);                                                                                  \
    if (!val.IsObject() || val.IsNull() || !Napi::New(env, type::constructor)->HasInstance(val)) {                     \
      Napi::TypeError::New(env, "Property \"" key "\" must be a " #type " object").ThrowAsJavaScriptException();       \
      return;                                                                                                          \
    }                                                                                                                  \
    var = val.As<Napi::Object>().Unwrap<type>();                                                                       \
    if (!var->isAlive()) {                                                                                             \
      Napi::Error::New(env, key ": " #type " object has already been destroyed").ThrowAsJavaScriptException();         \
      return;                                                                                                          \
    }                                                                                                                  \
  }

#define NODE_WRAPPED_FROM_OBJ_OPT(obj, key, type, var)                                                                 \
  {                                                                                                                    \
    Napi::String sym = Napi::New(env, key);                                                                            \
    if (Napi::HasOwnProperty(obj, sym).FromMaybe(false)) {                                                             \
      Napi::Value val = (obj).Get(sym);                                                                                \
      if (val.IsObject() && Napi::New(env, type::constructor)->HasInstance(val)) {                                     \
        var = val.As<Napi::Object>().Unwrap<type>();                                                                   \
        if (!var->isAlive()) {                                                                                         \
          Napi::Error::New(env, key ": " #type " object has already been destroyed").ThrowAsJavaScriptException();     \
          return;                                                                                                      \
        }                                                                                                              \
      } else if (!val.IsNull() && !val.IsNull()) {                                                                     \
        Napi::TypeError::New(env, key "property must be a " #type " object").ThrowAsJavaScriptException();             \
        return;                                                                                                        \
      }                                                                                                                \
    }                                                                                                                  \
  }

#define NODE_DOUBLE_FROM_OBJ_OPT(obj, key, var)                                                                        \
  {                                                                                                                    \
    Napi::String sym = Napi::New(env, key);                                                                            \
    if (Napi::HasOwnProperty(obj, sym).FromMaybe(false)) {                                                             \
      Napi::Value val = (obj).Get(sym);                                                                                \
      if (!val.IsNumber()) {                                                                                           \
        Napi::TypeError::New(env, "Property \"" key "\" must be a number").ThrowAsJavaScriptException();               \
        return;                                                                                                        \
      }                                                                                                                \
      var = val.As<Napi::Number>().DoubleValue().ToChecked();                                                          \
    }                                                                                                                  \
  }

#define NODE_INT_FROM_OBJ_OPT(obj, key, var)                                                                           \
  {                                                                                                                    \
    Napi::String sym = Napi::New(env, key);                                                                            \
    if (Napi::HasOwnProperty(obj, sym).FromMaybe(false)) {                                                             \
      Napi::Value val = (obj).Get(sym);                                                                                \
      if (!val.IsNumber()) {                                                                                           \
        Napi::TypeError::New(env, "Property \"" key "\" must be a number").ThrowAsJavaScriptException();               \
        return;                                                                                                        \
      }                                                                                                                \
      var = val.As<Napi::Number>().Int32Value().ToChecked();                                                           \
    }                                                                                                                  \
  }

#define NODE_INT64_FROM_OBJ_OPT(obj, key, var)                                                                         \
  {                                                                                                                    \
    Napi::String sym = Napi::New(env, key);                                                                            \
    if (Napi::HasOwnProperty(obj, sym).FromMaybe(false)) {                                                             \
      Napi::Value val = (obj).Get(sym);                                                                                \
      if (!val.IsNumber()) {                                                                                           \
        Napi::TypeError::New(env, "Property \"" key "\" must be a number").ThrowAsJavaScriptException();               \
        return;                                                                                                        \
      }                                                                                                                \
      var = val.As<Napi::Number>().Int64Value().ToChecked();                                                           \
    }                                                                                                                  \
  }

#define NODE_STR_FROM_OBJ_OPT(obj, key, var)                                                                           \
  {                                                                                                                    \
    Napi::String sym = Napi::New(env, key);                                                                            \
    if (Napi::HasOwnProperty(obj, sym).FromMaybe(false)) {                                                             \
      Napi::Value val = (obj).Get(sym);                                                                                \
      if (!val.IsString()) {                                                                                           \
        Napi::TypeError::New(env, "Property \"" key "\" must be a string").ThrowAsJavaScriptException();               \
        return;                                                                                                        \
      }                                                                                                                \
      var = val.As<Napi::String>().Utf8Value().c_str();                                                                \
    }                                                                                                                  \
  }

#define NODE_CB_FROM_OBJ_OPT(obj, key, var)                                                                            \
  {                                                                                                                    \
    var = nullptr;                                                                                                     \
    Napi::String sym = Napi::New(env, key);                                                                            \
    if (Napi::HasOwnProperty(obj, sym).FromMaybe(false)) {                                                             \
      Napi::Value val = (obj).Get(sym);                                                                                \
      if (!val->IsFunction()) {                                                                                        \
        Napi::TypeError::New(env, "Property \"" key "\" must be a function").ThrowAsJavaScriptException();             \
        return;                                                                                                        \
      } else {                                                                                                         \
        var = new Napi::FunctionReference(val.As<Napi::Function>());                                                   \
      }                                                                                                                \
    }                                                                                                                  \
  }

// ----- argument conversion -------

// determine field index based on string/numeric js argument
// f -> OGRFeature* or OGRFeatureDefn*

#define ARG_FIELD_ID(num, f, var)                                                                                      \
  {                                                                                                                    \
    if (info[num].IsString()) {                                                                                        \
      std::string field_name = info[num].As<Napi::String>().Utf8Value().c_str();                                       \
      var = f->GetFieldIndex(field_name.c_str());                                                                      \
      if (field_index == -1) {                                                                                         \
        Napi::Error::New(env, "Specified field name does not exist").ThrowAsJavaScriptException();                     \
        return;                                                                                                        \
      }                                                                                                                \
    } else if (info[num].IsNumber()) {                                                                                 \
      var = info[num].As<Napi::Number>().Int32Value().ToChecked();                                                     \
      if (var < 0 || var >= f->GetFieldCount()) {                                                                      \
        Napi::RangeError::New(env, "Invalid field index").ThrowAsJavaScriptException();                                \
        return;                                                                                                        \
      }                                                                                                                \
    } else {                                                                                                           \
      Napi::TypeError::New(env, "Field index must be integer or string").ThrowAsJavaScriptException();                 \
      return;                                                                                                          \
    }                                                                                                                  \
  }

#define NODE_ARG_INT(num, name, var)                                                                                   \
  if (info.Length() < num + 1) {                                                                                       \
    Napi::Error::New(env, name " must be given").ThrowAsJavaScriptException();                                         \
    return;                                                                                                            \
  }                                                                                                                    \
  if (!info[num].IsNumber()) {                                                                                         \
    Napi::TypeError::New(env, name " must be an integer").ThrowAsJavaScriptException();                                \
    return;                                                                                                            \
  }                                                                                                                    \
  var = static_cast<int>(info[num].As<Napi::Number>().Int64Value().ToChecked());

#define NODE_ARG_ENUM(num, name, enum_type, var)                                                                       \
  if (info.Length() < num + 1) {                                                                                       \
    Napi::Error::New(env, name " must be given").ThrowAsJavaScriptException();                                         \
    return;                                                                                                            \
  }                                                                                                                    \
  if (!info[num].IsNumber() && !info[num].IsUint32()) {                                                                \
    Napi::TypeError::New(env, name " must be of type " #enum_type).ThrowAsJavaScriptException();                       \
    return;                                                                                                            \
  }                                                                                                                    \
  var = enum_type(info[num].As<Napi::Number>().Uint32Value().ToChecked());

#define NODE_ARG_BOOL(num, name, var)                                                                                  \
  if (info.Length() < num + 1) {                                                                                       \
    Napi::Error::New(env, name " must be given").ThrowAsJavaScriptException();                                         \
    return;                                                                                                            \
  }                                                                                                                    \
  if (!info[num].IsBoolean()) {                                                                                        \
    Napi::TypeError::New(env, name " must be an boolean").ThrowAsJavaScriptException();                                \
    return;                                                                                                            \
  }                                                                                                                    \
  var = info[num].As<Napi::Boolean>().Value().ToChecked();

#define NODE_ARG_DOUBLE(num, name, var)                                                                                \
  if (info.Length() < num + 1) {                                                                                       \
    Napi::Error::New(env, name " must be given").ThrowAsJavaScriptException();                                         \
                                                                                                                       \
    return;                                                                                                            \
  }                                                                                                                    \
  if (!info[num].IsNumber()) {                                                                                         \
    Napi::TypeError::New(env, name " must be a number").ThrowAsJavaScriptException();                                  \
    return;                                                                                                            \
  }                                                                                                                    \
  var = info[num].As<Napi::Number>().DoubleValue().ToChecked();

#define NODE_ARG_ARRAY(num, name, var)                                                                                 \
  if (info.Length() < num + 1) {                                                                                       \
    Napi::Error::New(env, name " must be given").ThrowAsJavaScriptException();                                         \
    return;                                                                                                            \
  }                                                                                                                    \
  if (!info[num].IsArray()) {                                                                                          \
    Napi::TypeError::New(env, (std::string(name) + " must be an array").c_str()).ThrowAsJavaScriptException();         \
    return env.Null();                                                                                                 \
  }                                                                                                                    \
  var = info[num].As<Napi::Array>();

#define NODE_ARG_OBJECT(num, name, var)                                                                                \
  if (info.Length() < num + 1) {                                                                                       \
    Napi::Error::New(env, name " must be given").ThrowAsJavaScriptException();                                         \
                                                                                                                       \
    return;                                                                                                            \
  }                                                                                                                    \
  if (!info[num].IsObject()) {                                                                                         \
    Napi::TypeError::New(env, (std::string(name) + " must be an object").c_str()).ThrowAsJavaScriptException();        \
    return env.Null();                                                                                                 \
  }                                                                                                                    \
  var = info[num].As<Napi::Object>();

#define NODE_ARG_WRAPPED(num, name, type, var)                                                                         \
  if (info.Length() < num + 1) {                                                                                       \
    Napi::Error::New(env, name " must be given").ThrowAsJavaScriptException();                                         \
    return;                                                                                                            \
  }                                                                                                                    \
  if (info[num].IsNull() || info[num].IsUndefined() || !Napi::New(env, type::constructor)->HasInstance(info[num])) {   \
    Napi::TypeError::New(env, name " must be an instance of " #type).ThrowAsJavaScriptException();                     \
    return;                                                                                                            \
  }                                                                                                                    \
  var = info[num].As<Napi::Object>().Unwrap<type>();                                                                   \
  if (!var->isAlive()) {                                                                                               \
    Napi::Error::New(env, #type " parameter already destroyed").ThrowAsJavaScriptException();                          \
    return;                                                                                                            \
  }

#define NODE_ARG_STR(num, name, var)                                                                                   \
  if (info.Length() < num + 1) {                                                                                       \
    Napi::Error::New(env, name " must be given").ThrowAsJavaScriptException();                                         \
    return;                                                                                                            \
  }                                                                                                                    \
  if (!info[num].IsString()) {                                                                                         \
    Napi::TypeError::New(env, name " must be a string").ThrowAsJavaScriptException();                                  \
    return;                                                                                                            \
  }                                                                                                                    \
  var = (info[num].As<Napi::String>().Utf8Value().c_str())

#define NODE_ARG_STR_INT(num, name, varString, varInt, isString)                                                       \
  bool isString = false;                                                                                               \
  if (info.Length() < num + 1) {                                                                                       \
    Napi::Error::New(env, name " must be given").ThrowAsJavaScriptException();                                         \
    return;                                                                                                            \
  }                                                                                                                    \
  if (info[num].IsString()) {                                                                                          \
    varString = (info[num].As<Napi::String>().Utf8Value().c_str());                                                    \
    isString = true;                                                                                                   \
  } else if (info[num].IsNumber()) {                                                                                   \
    varInt = static_cast<int>(info[num].As<Napi::Number>().Int64Value().ToChecked());                                  \
    isString = false;                                                                                                  \
  } else {                                                                                                             \
    Napi::TypeError::New(env, name " must be a string or a number").ThrowAsJavaScriptException();                      \
    return;                                                                                                            \
  }

#define NODE_ARG_BUFFER(num, name, var)                                                                                \
  if (info.Length() < num + 1) {                                                                                       \
    Napi::Error::New(env, name " must be given").ThrowAsJavaScriptException();                                         \
    return;                                                                                                            \
  }                                                                                                                    \
  if (!info[num].IsArrayBuffer()) {                                                                                    \
    Napi::TypeError::New(env, name " must be a buffer").ThrowAsJavaScriptException();                                  \
    return;                                                                                                            \
  }                                                                                                                    \
  var = info[num].As<Napi::Object>();

// delete callback is in AsyncWorker::~AsyncWorker
#define NODE_ARG_CB(num, name, var)                                                                                    \
  if (info.Length() < num + 1) {                                                                                       \
    Napi::Error::New(env, name " must be given").ThrowAsJavaScriptException();                                         \
    return;                                                                                                            \
  }                                                                                                                    \
  if (!info[num].IsFunction()) {                                                                                       \
    Napi::TypeError::New(env, name " must be a function").ThrowAsJavaScriptException();                                \
    return;                                                                                                            \
  }                                                                                                                    \
  var = new Napi::FunctionReference(info[num].As<Napi::Function>())

// ----- optional argument conversion -------

#define NODE_ARG_INT_OPT(num, name, var)                                                                               \
  if (info.Length() > num) {                                                                                           \
    if (info[num].IsNumber()) {                                                                                        \
      var = static_cast<int>(info[num].As<Napi::Number>().Int64Value().ToChecked());                                   \
    } else if (!info[num].IsNull() && !info[num].IsUndefined()) {                                                      \
      Napi::TypeError::New(env, name " must be an integer").ThrowAsJavaScriptException();                              \
      return;                                                                                                          \
    }                                                                                                                  \
  }

#define NODE_ARG_ENUM_OPT(num, name, enum_type, var)                                                                   \
  if (info.Length() > num) {                                                                                           \
    if (info[num].IsNumber() || info[num].IsUint32()) {                                                                \
      var = static_cast<enum_type>(info[num].As<Napi::Number>().Uint32Value().ToChecked());                            \
    } else if (!info[num].IsNull() && !info[num].IsUndefined()) {                                                      \
      Napi::TypeError::New(env, name " must be an integer").ThrowAsJavaScriptException();                              \
                                                                                                                       \
      return;                                                                                                          \
    }                                                                                                                  \
  }

#define NODE_ARG_BOOL_OPT(num, name, var)                                                                              \
  if (info.Length() > num) {                                                                                           \
    if (info[num].IsBoolean()) {                                                                                       \
      var = info[num].As<Napi::Boolean>().Value().ToChecked();                                                         \
    } else if (!info[num].IsNull() && !info[num].IsUndefined()) {                                                      \
      Napi::TypeError::New(env, name " must be an boolean").ThrowAsJavaScriptException();                              \
                                                                                                                       \
      return;                                                                                                          \
    }                                                                                                                  \
  }

#define NODE_ARG_OPT_STR(num, name, var)                                                                               \
  if (info.Length() > num) {                                                                                           \
    if (info[num].IsString()) {                                                                                        \
      var = info[num].As<Napi::String>().Utf8Value().c_str();                                                          \
    } else if (!info[num].IsNull() && !info[num].IsUndefined()) {                                                      \
      Napi::TypeError::New(env, name " must be a string").ThrowAsJavaScriptException();                                \
      return;                                                                                                          \
    }                                                                                                                  \
  }

#define NODE_ARG_DOUBLE_OPT(num, name, var)                                                                            \
  if (info.Length() > num) {                                                                                           \
    if (info[num].IsNumber()) {                                                                                        \
      var = info[num].As<Napi::Number>().DoubleValue().ToChecked();                                                    \
    } else if (!info[num].IsNull() && !info[num].IsUndefined()) {                                                      \
      Napi::TypeError::New(env, name " must be a number").ThrowAsJavaScriptException();                                \
      return;                                                                                                          \
    }                                                                                                                  \
  }

#define NODE_ARG_WRAPPED_OPT(num, name, type, var)                                                                     \
  if (info.Length() > num && !info[num].IsNull() && !info[num].IsUndefined()) {                                        \
    if (!Napi::New(env, type::constructor)->HasInstance(info[num])) {                                                  \
      Napi::TypeError::New(env, name " must be an instance of " #type).ThrowAsJavaScriptException();                   \
      return;                                                                                                          \
    }                                                                                                                  \
    var = info[num].As<Napi::Object>().Unwrap<type>();                                                                 \
    if (!var->isAlive()) {                                                                                             \
      Napi::Error::New(env, #type " parameter already destroyed").ThrowAsJavaScriptException();                        \
      return;                                                                                                          \
    }                                                                                                                  \
  }

#define NODE_ARG_ARRAY_OPT(num, name, var)                                                                             \
  if (info.Length() > num) {                                                                                           \
    if (info[num].IsArray()) {                                                                                         \
      var = info[num].As<Napi::Array>();                                                                               \
    } else if (!info[num].IsNull() && !info[num].IsUndefined()) {                                                      \
      Napi::TypeError::New(env, name " must be an array").ThrowAsJavaScriptException();                                \
      return;                                                                                                          \
    }                                                                                                                  \
  }

#define NODE_ARG_OBJECT_OPT(num, name, var)                                                                            \
  if (info.Length() > num && !info[num].IsNull() && !info[num].IsUndefined()) {                                        \
    if (!info[num].IsObject()) {                                                                                       \
      Napi::TypeError::New(env, (std::string(name) + " must be an object").c_str()).ThrowAsJavaScriptException();      \
      return env.Null();                                                                                               \
    }                                                                                                                  \
    var = info[num].As<Napi::Object>();                                                                                \
  }

// delete is in GDALAsyncWorker::~GDALAsyncWorker
#define NODE_ARG_CB_OPT(num, name, var)                                                                                \
  if (!info[num].IsNull() && !info[num].IsUndefined()) {                                                               \
    if (info[num].IsFunction()) {                                                                                      \
      var = new Napi::FunctionReference(info[num].As<Napi::Function>());                                               \
    } else {                                                                                                           \
      Napi::TypeError::New(env, name " must be a function").ThrowAsJavaScriptException();                              \
      return;                                                                                                          \
    }                                                                                                                  \
  }

// ----- special case: progress callback in [options] argument -------

#define NODE_PROGRESS_CB_OPT(num, progress_cb, job)                                                                    \
  {                                                                                                                    \
    Napi::Object progress_obj;                                                                                         \
    progress_cb = nullptr;                                                                                             \
    if (info.Length() > num && !info[num].IsNull() && !info[num].IsUndefined()) {                                      \
      if (!info[num].IsObject()) {                                                                                     \
        Napi::TypeError::New(env, "options must be an object").ThrowAsJavaScriptException();                           \
        return env.Null();                                                                                             \
      }                                                                                                                \
      progress_obj = info[num].As<Napi::Object>();                                                                     \
      NODE_CB_FROM_OBJ_OPT(progress_obj, "progress_cb", progress_cb);                                                  \
    }                                                                                                                  \
    if (progress_cb) { job.progress = progress_cb; }                                                                   \
  }

// ----- wrapped methods w/ results-------

#define NODE_WRAPPED_METHOD_WITH_RESULT(klass, method, result_type, wrapped_method)                                    \
  Napi::Value klass::method(const Napi::CallbackInfo &info) {                                                          \
    klass *obj = info.This().Unwrap<klass>();                                                                          \
    if (!obj->isAlive()) {                                                                                             \
      Napi::Error::New(env, #klass " object has already been destroyed").ThrowAsJavaScriptException();                 \
      return;                                                                                                          \
    }                                                                                                                  \
    return Napi::result_type::New(env, obj->this_->wrapped_method());                                                  \
  }

#define NODE_WRAPPED_METHOD_WITH_RESULT_1_WRAPPED_PARAM(                                                               \
  klass, method, result_type, wrapped_method, param_type, param_name)                                                  \
  Napi::Value klass::method(const Napi::CallbackInfo &info) {                                                          \
    param_type *param;                                                                                                 \
    NODE_ARG_WRAPPED(0, #param_name, param_type, param);                                                               \
    klass *obj = info.This().Unwrap<klass>();                                                                          \
    if (!obj->isAlive())                                                                                               \
      Napi::Error::New(env, #klass " object has already been destroyed").ThrowAsJavaScriptException();                 \
    return env.Null();                                                                                                 \
    return Napi::result_type::New(env, obj->this_->wrapped_method(param->get()));                                      \
  }

#define NODE_WRAPPED_METHOD_WITH_RESULT_1_ENUM_PARAM(                                                                  \
  klass, method, result_type, wrapped_method, enum_type, param_name)                                                   \
  Napi::Value klass::method(const Napi::CallbackInfo &info) {                                                          \
    enum_type param;                                                                                                   \
    NODE_ARG_ENUM(0, #param_name, enum_type, param);                                                                   \
    klass *obj = info.This().Unwrap<klass>();                                                                          \
    if (!obj->isAlive()) {                                                                                             \
      Napi::Error::New(env, #klass " object has already been destroyed").ThrowAsJavaScriptException();                 \
      return;                                                                                                          \
    }                                                                                                                  \
    return Napi::result_type::New(env, obj->this_->wrapped_method(param));                                             \
  }

#define NODE_WRAPPED_METHOD_WITH_RESULT_1_STRING_PARAM(klass, method, result_type, wrapped_method, param_name)         \
  Napi::Value klass::method(const Napi::CallbackInfo &info) {                                                          \
    std::string param;                                                                                                 \
    NODE_ARG_STR(0, #param_name, param);                                                                               \
    klass *obj = info.This().Unwrap<klass>();                                                                          \
    if (!obj->isAlive()) {                                                                                             \
      Napi::Error::New(env, #klass " object has already been destroyed").ThrowAsJavaScriptException();                 \
      return;                                                                                                          \
    }                                                                                                                  \
    return Napi::result_type::New(env, obj->this_->wrapped_method(param.c_str()));                                     \
  }

#define NODE_WRAPPED_METHOD_WITH_RESULT_1_INTEGER_PARAM(klass, method, result_type, wrapped_method, param_name)        \
  Napi::Value klass::method(const Napi::CallbackInfo &info) {                                                          \
    int param;                                                                                                         \
    NODE_ARG_INT(0, #param_name, param);                                                                               \
    klass *obj = info.This().Unwrap<klass>();                                                                          \
    if (!obj->isAlive()) {                                                                                             \
      Napi::Error::New(env, #klass " object has already been destroyed").ThrowAsJavaScriptException();                 \
      return;                                                                                                          \
    }                                                                                                                  \
    return Napi::result_type::New(env, obj->this_->wrapped_method(param));                                             \
  }

#define NODE_WRAPPED_METHOD_WITH_RESULT_1_DOUBLE_PARAM(klass, method, result_type, wrapped_method, param_name)         \
  Napi::Value klass::method(const Napi::CallbackInfo &info) {                                                          \
    double param;                                                                                                      \
    NODE_ARG_DOUBLE(0, #param_name, param);                                                                            \
    klass *obj = info.This().Unwrap<klass>();                                                                          \
    if (!obj->isAlive()) {                                                                                             \
      Napi::Error::New(env, #klass " object has already been destroyed").ThrowAsJavaScriptException();                 \
      return;                                                                                                          \
    }                                                                                                                  \
    return Napi::result_type::New(env, obj->this_->wrapped_method(param));                                             \
  }

// ----- wrapped methods w/ lock -------

#define NODE_WRAPPED_METHOD_WITH_RESULT_LOCKED(klass, method, result_type, wrapped_method)                             \
  Napi::Value klass::method(const Napi::CallbackInfo &info) {                                                          \
    klass *obj = info.This().Unwrap<klass>();                                                                          \
    if (!obj->isAlive()) {                                                                                             \
      Napi::Error::New(env, #klass " object has already been destroyed").ThrowAsJavaScriptException();                 \
      return;                                                                                                          \
    }                                                                                                                  \
    GDAL_LOCK_PARENT(obj);                                                                                             \
    return Napi::result_type::New(env, obj->this_->wrapped_method());                                                  \
  }

#define NODE_WRAPPED_GETTER_WITH_STRING_LOCKED(klass, method, wrapped_method)                                          \
  Napi::Value klass::method(const Napi::CallbackInfo &info) {                                                          \
    klass *obj = info.This().Unwrap<klass>();                                                                          \
    if (!obj->isAlive()) {                                                                                             \
      Napi::Error::New(env, #klass " object has already been destroyed").ThrowAsJavaScriptException();                 \
      return;                                                                                                          \
    }                                                                                                                  \
    GDAL_LOCK_PARENT(obj);                                                                                             \
    auto r = obj->this_->wrapped_method();                                                                             \
    return SafeString::New(r.c_str());                                                                                 \
  }

#define NODE_WRAPPED_GETTER_WITH_RESULT_LOCKED(klass, method, result_type, wrapped_method)                             \
  Napi::Value klass::method(const Napi::CallbackInfo &info) {                                                          \
    klass *obj = info.This().Unwrap<klass>();                                                                          \
    if (!obj->isAlive()) {                                                                                             \
      Napi::Error::New(env, #klass " object has already been destroyed").ThrowAsJavaScriptException();                 \
      return;                                                                                                          \
    }                                                                                                                  \
    GDAL_LOCK_PARENT(obj);                                                                                             \
    auto r = obj->this_->wrapped_method();                                                                             \
    return Napi::result_type::New(env, r);                                                                             \
  }

// ----- wrapped asyncable methods-------
#define NODE_WRAPPED_ASYNC_METHOD(klass, method, wrapped_method)                                                       \
  GDAL_ASYNCABLE_DEFINE(klass::method) {                                                                               \
    klass *obj = info.This().Unwrap<klass>();                                                                          \
    if (!obj->isAlive()) {                                                                                             \
      Napi::Error::New(env, #klass " object has already been destroyed").ThrowAsJavaScriptException();                 \
      return;                                                                                                          \
    }                                                                                                                  \
    auto *gdal_obj = obj->this_;                                                                                       \
    GDALAsyncableJob<int> job(0);                                                                                      \
    job.main = [gdal_obj](const GDALExecutionProgress &) {                                                             \
      gdal_obj->wrapped_method();                                                                                      \
      return 0;                                                                                                        \
    };                                                                                                                 \
    job.rval = [](int, const GetFromPersistentFunc &) { return env.Undefined().As<Napi::Value>(); };                   \
    job.run(info, async, 0);                                                                                           \
  }

// ----- wrapped asyncable methods w/ results-------

#define NODE_WRAPPED_ASYNC_METHOD_WITH_RESULT(klass, async_type, method, result_type, wrapped_method)                  \
  GDAL_ASYNCABLE_DEFINE(klass::method) {                                                                               \
    klass *obj = info.This().Unwrap<klass>();                                                                          \
    if (!obj->isAlive()) {                                                                                             \
      Napi::Error::New(env, #klass " object has already been destroyed").ThrowAsJavaScriptException();                 \
      return;                                                                                                          \
    }                                                                                                                  \
    auto *gdal_obj = obj->this_;                                                                                       \
    GDALAsyncableJob<async_type> job(0);                                                                               \
    job.main = [gdal_obj](const GDALExecutionProgress &) { return gdal_obj->wrapped_method(); };                       \
    job.rval = [](async_type r, const GetFromPersistentFunc &) { return Napi::result_type::New(env, r); };             \
    job.run(info, async, 0);                                                                                           \
  }

// param_type must be a node-gdal type
#define NODE_WRAPPED_ASYNC_METHOD_WITH_RESULT_1_WRAPPED_PARAM(                                                         \
  klass, async_type, method, result_type, wrapped_method, param_type, param_name)                                      \
  GDAL_ASYNCABLE_DEFINE(klass::method) {                                                                               \
    param_type *param;                                                                                                 \
    NODE_ARG_WRAPPED(0, #param_name, param_type, param);                                                               \
    klass *obj = info.This().Unwrap<klass>();                                                                          \
    if (!obj->isAlive())                                                                                               \
      Napi::Error::New(env, #klass " object has already been destroyed").ThrowAsJavaScriptException();                 \
    return env.Null();                                                                                                 \
    auto *gdal_obj = obj->this_;                                                                                       \
    auto *gdal_param = param->get();                                                                                   \
    GDALAsyncableJob<async_type> job(0);                                                                               \
    job.persist(info[0].As<Napi::Object>());                                                                           \
    job.main = [gdal_obj, gdal_param](const GDALExecutionProgress &) { return gdal_obj->wrapped_method(gdal_param); }; \
    job.rval = [](async_type r, const GetFromPersistentFunc &) { return Napi::result_type::New(env, r); };             \
    job.run(info, async, 1);                                                                                           \
  }

#define NODE_WRAPPED_ASYNC_METHOD_WITH_RESULT_1_ENUM_PARAM(                                                            \
  klass, async_type, method, result_type, wrapped_method, enum_type, param_name)                                       \
  GDAL_ASYNCABLE_DEFINE(klass::method) {                                                                               \
    enum_type param;                                                                                                   \
    NODE_ARG_ENUM(0, #param_name, enum_type, param);                                                                   \
    klass *obj = info.This().Unwrap<klass>();                                                                          \
    if (!obj->isAlive()) {                                                                                             \
      Napi::Error::New(env, #klass " object has already been destroyed").ThrowAsJavaScriptException();                 \
      return;                                                                                                          \
    }                                                                                                                  \
    auto *gdal_obj = obj->this_;                                                                                       \
    GDALAsyncableJob<async_type> job(0);                                                                               \
    job.main = [gdal_obj, param](const GDALExecutionProgress &) { return gdal_obj->wrapped_method(param); };           \
    job.rval = [](async_type r, const GetFromPersistentFunc &) { return Napi::result_type::New(env, r); };             \
    job.run(info, async, 1);                                                                                           \
  }

#define NODE_WRAPPED_ASYNC_METHOD_WITH_OGRERR_RESULT_LOCKED(klass, method, wrapped_method)                             \
  GDAL_ASYNCABLE_DEFINE(klass::method) {                                                                               \
    klass *obj = info.This().Unwrap<klass>();                                                                          \
    if (!obj->isAlive()) {                                                                                             \
      Napi::Error::New(env, #klass " object has already been destroyed").ThrowAsJavaScriptException();                 \
      return;                                                                                                          \
    }                                                                                                                  \
    auto gdal_obj = obj->this_;                                                                                        \
    GDALAsyncableJob<OGRErr> job(obj->parent_uid);                                                                     \
    job.main = [gdal_obj](const GDALExecutionProgress &) {                                                             \
      int err = gdal_obj->wrapped_method();                                                                            \
      if (err) throw getOGRErrMsg(err);                                                                                \
      return err;                                                                                                      \
    };                                                                                                                 \
    job.rval = [](OGRErr, const GetFromPersistentFunc &) { return env.Undefined().As<Napi::Value>(); };                \
    job.run(info, async, 0);                                                                                           \
  }

#define NODE_WRAPPED_ASYNC_METHOD_WITH_OGRERR_RESULT_1_WRAPPED_PARAM(                                                  \
  klass, async_type, method, wrapped_method, param_type, param_name)                                                   \
  GDAL_ASYNCABLE_DEFINE(klass::method) {                                                                               \
    param_type *param;                                                                                                 \
    NODE_ARG_WRAPPED(0, #param_name, param_type, param);                                                               \
    klass *obj = info.This().Unwrap<klass>();                                                                          \
    if (!obj->isAlive()) {                                                                                             \
      Napi::Error::New(env, #klass " object has already been destroyed").ThrowAsJavaScriptException();                 \
      return;                                                                                                          \
    }                                                                                                                  \
    auto gdal_obj = obj->this_;                                                                                        \
    auto gdal_param = param->get();                                                                                    \
    GDALAsyncableJob<async_type> job(0);                                                                               \
    job.persist(info[0].As<Napi::Object>());                                                                           \
    job.main = [gdal_obj, gdal_param](const GDALExecutionProgress &) {                                                 \
      int err = gdal_obj->wrapped_method(gdal_param);                                                                  \
      if (err) throw getOGRErrMsg(err);                                                                                \
      return err;                                                                                                      \
    };                                                                                                                 \
    job.rval = [](async_type, const GetFromPersistentFunc &) { return env.Undefined().As<Napi::Value>(); };            \
    job.run(info, async, 1);                                                                                           \
  }

// ----- wrapped methods w/ CPLErr result (throws) -------

#define NODE_WRAPPED_METHOD_WITH_CPLERR_RESULT(klass, method, wrapped_method)                                          \
  Napi::Value klass::method(const Napi::CallbackInfo &info) {                                                          \
    klass *obj = info.This().Unwrap<klass>();                                                                          \
    if (!obj->isAlive()) {                                                                                             \
      Napi::Error::New(env, #klass " object has already been destroyed").ThrowAsJavaScriptException();                 \
      return;                                                                                                          \
    }                                                                                                                  \
    int err = obj->this_->wrapped_method();                                                                            \
    if (err) {                                                                                                         \
      NODE_THROW_LAST_CPLERR;                                                                                          \
      return;                                                                                                          \
    }                                                                                                                  \
    return;                                                                                                            \
  }

#define NODE_WRAPPED_METHOD_WITH_CPLERR_RESULT_1_WRAPPED_PARAM(klass, method, wrapped_method, param_type, param_name)  \
  Napi::Value klass::method(const Napi::CallbackInfo &info) {                                                          \
    param_type *param;                                                                                                 \
    NODE_ARG_WRAPPED(0, #param_name, param_type, param);                                                               \
    klass *obj = info.This().Unwrap<klass>();                                                                          \
    if (!obj->isAlive()) {                                                                                             \
      Napi::Error::New(env, #klass " object has already been destroyed").ThrowAsJavaScriptException();                 \
      return;                                                                                                          \
    }                                                                                                                  \
    int err = obj->this_->wrapped_method(param->get());                                                                \
    if (err) {                                                                                                         \
      NODE_THROW_LAST_CPLERR;                                                                                          \
      return;                                                                                                          \
    }                                                                                                                  \
    return;                                                                                                            \
  }

#define NODE_WRAPPED_METHOD_WITH_CPLERR_RESULT_1_STRING_PARAM(klass, method, wrapped_method, param_name)               \
  Napi::Value klass::method(const Napi::CallbackInfo &info) {                                                          \
    std::string param;                                                                                                 \
    NODE_ARG_STR(0, #param_name, param);                                                                               \
    klass *obj = info.This().Unwrap<klass>();                                                                          \
    if (!obj->isAlive()) {                                                                                             \
      Napi::Error::New(env, #klass " object has already been destroyed").ThrowAsJavaScriptException();                 \
      return;                                                                                                          \
    }                                                                                                                  \
    int err = obj->this_->wrapped_method(param.c_str());                                                               \
    if (err) {                                                                                                         \
      NODE_THROW_LAST_CPLERR;                                                                                          \
      return;                                                                                                          \
    }                                                                                                                  \
    return;                                                                                                            \
  }

#define NODE_WRAPPED_METHOD_WITH_CPLERR_RESULT_1_INTEGER_PARAM(klass, method, wrapped_method, param_name)              \
  Napi::Value klass::method(const Napi::CallbackInfo &info) {                                                          \
    int param;                                                                                                         \
    NODE_ARG_INT(0, #param_name, param);                                                                               \
    klass *obj = info.This().Unwrap<klass>();                                                                          \
    if (!obj->isAlive()) {                                                                                             \
      Napi::Error::New(env, #klass " object has already been destroyed").ThrowAsJavaScriptException();                 \
      return;                                                                                                          \
    }                                                                                                                  \
    int err = obj->this_->wrapped_method(param);                                                                       \
    if (err) {                                                                                                         \
      NODE_THROW_LAST_CPLERR;                                                                                          \
      return;                                                                                                          \
    }                                                                                                                  \
    return;                                                                                                            \
  }

#define NODE_WRAPPED_METHOD_WITH_CPLERR_RESULT_1_INTEGER_PARAM_LOCKED(klass, method, wrapped_method, param_name)       \
  Napi::Value klass::method(const Napi::CallbackInfo &info) {                                                          \
    int param;                                                                                                         \
    NODE_ARG_INT(0, #param_name, param);                                                                               \
    klass *obj = info.This().Unwrap<klass>();                                                                          \
    if (!obj->isAlive()) {                                                                                             \
      Napi::Error::New(env, #klass " object has already been destroyed").ThrowAsJavaScriptException();                 \
      return;                                                                                                          \
    }                                                                                                                  \
    int err = obj->this_->wrapped_method(param);                                                                       \
    if (err) {                                                                                                         \
      NODE_THROW_LAST_CPLERR;                                                                                          \
      return;                                                                                                          \
    }                                                                                                                  \
    return;                                                                                                            \
  }

#define NODE_WRAPPED_METHOD_WITH_CPLERR_RESULT_1_DOUBLE_PARAM(klass, method, wrapped_method, param_name)               \
  Napi::Value klass::method(const Napi::CallbackInfo &info) {                                                          \
    double param;                                                                                                      \
    NODE_ARG_DOUBLE(0, #param_name, param);                                                                            \
    klass *obj = info.This().Unwrap<klass>();                                                                          \
    if (!obj->isAlive()) {                                                                                             \
      Napi::Error::New(env, #klass " object has already been destroyed").ThrowAsJavaScriptException();                 \
      return;                                                                                                          \
    }                                                                                                                  \
    GDAL_LOCK_PARENT(obj);                                                                                             \
    int err = obj->this_->wrapped_method(param);                                                                       \
    if (err) { NODE_THROW_LAST_CPLERR; }                                                                               \
    return;                                                                                                            \
  }

// ----- wrapped methods w/ OGRErr result (throws) -------

#define NODE_WRAPPED_METHOD_WITH_OGRERR_RESULT(klass, method, wrapped_method)                                          \
  Napi::Value klass::method(const Napi::CallbackInfo &info) {                                                          \
    klass *obj = info.This().Unwrap<klass>();                                                                          \
    if (!obj->isAlive()) {                                                                                             \
      Napi::Error::New(env, #klass " object has already been destroyed").ThrowAsJavaScriptException();                 \
      return;                                                                                                          \
    }                                                                                                                  \
    int err = obj->this_->wrapped_method();                                                                            \
    if (err) {                                                                                                         \
      NODE_THROW_OGRERR(err);                                                                                          \
      return;                                                                                                          \
    }                                                                                                                  \
    return;                                                                                                            \
  }

#define NODE_WRAPPED_METHOD_WITH_OGRERR_RESULT_1_WRAPPED_PARAM(klass, method, wrapped_method, param_type, param_name)  \
  Napi::Value klass::method(const Napi::CallbackInfo &info) {                                                          \
    param_type *param;                                                                                                 \
    NODE_ARG_WRAPPED(0, #param_name, param_type, param);                                                               \
    klass *obj = info.This().Unwrap<klass>();                                                                          \
    if (!obj->isAlive()) {                                                                                             \
      Napi::Error::New(env, #klass " object has already been destroyed").ThrowAsJavaScriptException();                 \
      return;                                                                                                          \
    }                                                                                                                  \
    int err = obj->this_->wrapped_method(param->get());                                                                \
    if (err) {                                                                                                         \
      NODE_THROW_OGRERR(err);                                                                                          \
      return;                                                                                                          \
    }                                                                                                                  \
    return;                                                                                                            \
  }

#define NODE_WRAPPED_METHOD_WITH_OGRERR_RESULT_1_STRING_PARAM(klass, method, wrapped_method, param_name)               \
  Napi::Value klass::method(const Napi::CallbackInfo &info) {                                                          \
    std::string param;                                                                                                 \
    NODE_ARG_STR(0, #param_name, param);                                                                               \
    klass *obj = info.This().Unwrap<klass>();                                                                          \
    if (!obj->isAlive()) {                                                                                             \
      Napi::Error::New(env, #klass " object has already been destroyed").ThrowAsJavaScriptException();                 \
      return;                                                                                                          \
    }                                                                                                                  \
    int err = obj->this_->wrapped_method(param.c_str());                                                               \
    if (err) {                                                                                                         \
      NODE_THROW_OGRERR(err);                                                                                          \
      return;                                                                                                          \
    }                                                                                                                  \
    return;                                                                                                            \
  }

#define NODE_WRAPPED_METHOD_WITH_OGRERR_RESULT_1_INTEGER_PARAM(klass, method, wrapped_method, param_name)              \
  Napi::Value klass::method(const Napi::CallbackInfo &info) {                                                          \
    int param;                                                                                                         \
    NODE_ARG_INT(0, #param_name, param);                                                                               \
    klass *obj = info.This().Unwrap<klass>();                                                                          \
    if (!obj->isAlive()) {                                                                                             \
      Napi::Error::New(env, #klass " object has already been destroyed").ThrowAsJavaScriptException();                 \
      return;                                                                                                          \
    }                                                                                                                  \
    int err = obj->this_->wrapped_method(param);                                                                       \
    if (err) {                                                                                                         \
      NODE_THROW_OGRERR(err);                                                                                          \
      return;                                                                                                          \
    }                                                                                                                  \
    return;                                                                                                            \
  }

#define NODE_WRAPPED_METHOD_WITH_OGRERR_RESULT_1_DOUBLE_PARAM(klass, method, wrapped_method, param_name)               \
  Napi::Value klass::method(const Napi::CallbackInfo &info) {                                                          \
    double param;                                                                                                      \
    NODE_ARG_DOUBLE(0, #param_name, param);                                                                            \
    klass *obj = info.This().Unwrap<klass>();                                                                          \
    if (!obj->isAlive()) {                                                                                             \
      Napi::Error::New(env, #klass " object has already been destroyed").ThrowAsJavaScriptException();                 \
      return;                                                                                                          \
    }                                                                                                                  \
    int err = obj->this_->wrapped_method(param);                                                                       \
    if (err) {                                                                                                         \
      NODE_THROW_OGRERR(err);                                                                                          \
      return;                                                                                                          \
    }                                                                                                                  \
    return;                                                                                                            \
  }

// ----- wrapped methods w/ OGRErr result (throws) -------

#define NODE_WRAPPED_METHOD_WITH_OGRERR_RESULT_LOCKED(klass, method, wrapped_method)                                   \
  Napi::Value klass::method(const Napi::CallbackInfo &info) {                                                          \
    klass *obj = info.This().Unwrap<klass>();                                                                          \
    if (!obj->isAlive()) {                                                                                             \
      Napi::Error::New(env, #klass " object has already been destroyed").ThrowAsJavaScriptException();                 \
      return;                                                                                                          \
    }                                                                                                                  \
    GDAL_LOCK_PARENT(obj);                                                                                             \
    int err = obj->this_->wrapped_method();                                                                            \
    if (err) {                                                                                                         \
      NODE_THROW_OGRERR(err);                                                                                          \
      return;                                                                                                          \
    }                                                                                                                  \
    return;                                                                                                            \
  }

#define NODE_WRAPPED_METHOD_WITH_RESULT_1_STRING_PARAM_LOCKED(klass, method, result_type, wrapped_method, param_name)  \
  Napi::Value klass::method(const Napi::CallbackInfo &info) {                                                          \
    std::string param;                                                                                                 \
    NODE_ARG_STR(0, #param_name, param);                                                                               \
    klass *obj = info.This().Unwrap<klass>();                                                                          \
    if (!obj->isAlive()) {                                                                                             \
      Napi::Error::New(env, #klass " object has already been destroyed").ThrowAsJavaScriptException();                 \
      return;                                                                                                          \
    }                                                                                                                  \
    GDAL_LOCK_PARENT(obj);                                                                                             \
    auto r = obj->this_->wrapped_method(param.c_str());                                                                \
    return Napi::result_type::New(env, r);                                                                             \
  }

// ----- wrapped methods -------

#define NODE_WRAPPED_METHOD(klass, method, wrapped_method)                                                             \
  Napi::Value klass::method(const Napi::CallbackInfo &info) {                                                          \
    klass *obj = info.This().Unwrap<klass>();                                                                          \
    if (!obj->isAlive()) {                                                                                             \
      Napi::Error::New(env, #klass " object has already been destroyed").ThrowAsJavaScriptException();                 \
      return;                                                                                                          \
    }                                                                                                                  \
    obj->this_->wrapped_method();                                                                                      \
    return;                                                                                                            \
  }

#define NODE_WRAPPED_METHOD_WITH_1_WRAPPED_PARAM(klass, method, wrapped_method, param_type, param_name)                \
  Napi::Value klass::method(const Napi::CallbackInfo &info) {                                                          \
    param_type *param;                                                                                                 \
    NODE_ARG_WRAPPED(0, #param_name, param_type, param);                                                               \
    klass *obj = info.This().Unwrap<klass>();                                                                          \
    if (!obj->isAlive()) {                                                                                             \
      Napi::Error::New(env, #klass " object has already been destroyed").ThrowAsJavaScriptException();                 \
      return;                                                                                                          \
    }                                                                                                                  \
    obj->this_->wrapped_method(param->get());                                                                          \
    return;                                                                                                            \
  }

#define NODE_WRAPPED_METHOD_WITH_1_INTEGER_PARAM(klass, method, wrapped_method, param_name)                            \
  Napi::Value klass::method(const Napi::CallbackInfo &info) {                                                          \
    int param;                                                                                                         \
    NODE_ARG_INT(0, #param_name, param);                                                                               \
    klass *obj = info.This().Unwrap<klass>();                                                                          \
    if (!obj->isAlive()) {                                                                                             \
      Napi::Error::New(env, #klass " object has already been destroyed").ThrowAsJavaScriptException();                 \
      return;                                                                                                          \
    }                                                                                                                  \
    obj->this_->wrapped_method(param);                                                                                 \
    return;                                                                                                            \
  }

#define NODE_WRAPPED_METHOD_WITH_1_DOUBLE_PARAM(klass, method, wrapped_method, param_name)                             \
  Napi::Value klass::method(const Napi::CallbackInfo &info) {                                                          \
    double param;                                                                                                      \
    NODE_ARG_DOUBLE(0, #param_name, param);                                                                            \
    klass *obj = info.This().Unwrap<klass>();                                                                          \
    if (!obj->isAlive()) {                                                                                             \
      Napi::Error::New(env, #klass " object has already been destroyed").ThrowAsJavaScriptException();                 \
      return;                                                                                                          \
    }                                                                                                                  \
    obj->this_->wrapped_method(param);                                                                                 \
    return;                                                                                                            \
  }

#define NODE_WRAPPED_METHOD_WITH_1_BOOLEAN_PARAM(klass, method, wrapped_method, param_name)                            \
  Napi::Value klass::method(const Napi::CallbackInfo &info) {                                                          \
    bool param;                                                                                                        \
    NODE_ARG_BOOL(0, #param_name, param);                                                                              \
    klass *obj = info.This().Unwrap<klass>();                                                                          \
    if (!obj->isAlive()) {                                                                                             \
      Napi::Error::New(env, #klass " object has already been destroyed").ThrowAsJavaScriptException();                 \
      return;                                                                                                          \
    }                                                                                                                  \
    obj->this_->wrapped_method(param);                                                                                 \
    return;                                                                                                            \
  }

#define NODE_WRAPPED_METHOD_WITH_1_ENUM_PARAM(klass, method, wrapped_method, enum_type, param_name)                    \
  Napi::Value klass::method(const Napi::CallbackInfo &info) {                                                          \
    enum_type param;                                                                                                   \
    NODE_ARG_ENUM(0, #param_name, enum_type, param);                                                                   \
    klass *obj = info.This().Unwrap<klass>();                                                                          \
    if (!obj->isAlive()) {                                                                                             \
      Napi::Error::New(env, #klass " object has already been destroyed").ThrowAsJavaScriptException();                 \
      return;                                                                                                          \
    }                                                                                                                  \
    obj->this_->wrapped_method(param);                                                                                 \
    return;                                                                                                            \
  }

#define NODE_WRAPPED_METHOD_WITH_1_STRING_PARAM(klass, method, wrapped_method, param_name)                             \
  Napi::Value klass::method(const Napi::CallbackInfo &info) {                                                          \
    std::string param;                                                                                                 \
    NODE_ARG_STR(0, #param_name, param);                                                                               \
    klass *obj = info.This().Unwrap<klass>();                                                                          \
    if (!obj->isAlive()) {                                                                                             \
      Napi::Error::New(env, #klass " object has already been destroyed").ThrowAsJavaScriptException();                 \
      return;                                                                                                          \
    }                                                                                                                  \
    obj->this_->wrapped_method(param.c_str());                                                                         \
    return;                                                                                                            \
  }

#define MEASURE_EXECUTION_TIME(msg, op)                                                                                \
  {                                                                                                                    \
    auto start = std::chrono::high_resolution_clock::now();                                                            \
    if (msg != nullptr) fprintf(stderr, "%s", msg);                                                                    \
    op;                                                                                                                \
    auto elapsed = std::chrono::high_resolution_clock::now() - start;                                                  \
    if (msg != nullptr)                                                                                                \
      fprintf(                                                                                                         \
        stderr,                                                                                                        \
        "%ld µs\n",                                                                                                    \
        static_cast<long>(std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count()));                    \
  }

#endif
