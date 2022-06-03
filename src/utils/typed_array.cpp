#include "typed_array.hpp"

#include <sstream>

namespace node_gdal {

// https://github.com/joyent/node/issues/4201#issuecomment-9837340

// length is number of elements
Napi::Value TypedArray::New(Napi::Env env, GDALDataType type, unsigned int length) {
  Napi::EscapableHandleScope scope(env);

  Napi::TypedArray array;

  switch (type) {
    case GDT_Byte: array = Napi::Uint8Array::New(env, length); break;
    case GDT_Int16: array = Napi::Int16Array::New(env, length); break;
    case GDT_UInt16: array = Napi::Uint16Array::New(env, length); break;
    case GDT_Int32: array = Napi::Int32Array::New(env, length); break;
    case GDT_UInt32: array = Napi::Uint32Array::New(env, length); break;
    case GDT_Float32: array = Napi::Float32Array::New(env, length); break;
    case GDT_Float64: array = Napi::Float64Array::New(env, length); break;
    default:
      Napi::Error::New(env, "Unsupported array type").ThrowAsJavaScriptException();
      return scope.Escape(env.Undefined());
  }

  if (array.IsEmpty() || !array.IsObject()) {
    Napi::Error::New(env, "Error allocating ArrayBuffer").ThrowAsJavaScriptException();
    return scope.Escape(env.Undefined());
  }

  array.Set("_gdal_type", Napi::Number::New(env, type));

  return scope.Escape(array);
}

// Create a new TypedArray view over an existing memory buffer
// This function throws because it is meant to be used inside a pixel function
// size is number of bytes
Napi::Value TypedArray::New(Napi::Env env, GDALDataType type, void *data, unsigned int size) {
  Napi::EscapableHandleScope scope(env);
  Napi::TypedArray array;

  Napi::ArrayBuffer buffer = Napi::ArrayBuffer::New(env, data, size);
  size_t length = size / GDALGetDataTypeSizeBytes(type);

  switch (type) {
    case GDT_Byte: array = Napi::Uint8Array::New(env, length, buffer, 0, napi_uint8_array); break;
    case GDT_Int16: array = Napi::Int16Array::New(env, length, buffer, 0, napi_int16_array); break;
    case GDT_UInt16: array = Napi::Uint16Array::New(env, length, buffer, 0, napi_uint16_array); break;
    case GDT_Int32: array = Napi::Int32Array::New(env, length, buffer, 0, napi_int32_array); break;
    case GDT_UInt32: array = Napi::Uint32Array::New(env, length, buffer, 0, napi_uint32_array); break;
    case GDT_Float32: array = Napi::Float32Array::New(env, length, buffer, 0, napi_float32_array); break;
    case GDT_Float64: array = Napi::Float64Array::New(env, length, buffer, 0, napi_float64_array); break;
    default:
      Napi::Error::New(env, "Unsupported array type").ThrowAsJavaScriptException();
      return scope.Escape(env.Undefined());
  }

  if (array.IsEmpty() || !array.IsObject()) { throw "Error creating TypedArray"; }
  array.Set("_gdal_type", Napi::Number::New(env, type));

  return scope.Escape(array);
}

GDALDataType TypedArray::Identify(Napi::Env env, Napi::Object obj) {
  if (!obj.HasOwnProperty("_gdal_type")) return GDT_Unknown;
  Napi::Value val = obj.Get("_gdal_type");
  if (!val.IsNumber()) return GDT_Unknown;

  return static_cast<GDALDataType>(val.ToNumber().Int32Value());
}

// min_length is number of elements
void *TypedArray::Validate(Napi::Env env, Napi::Object obj, GDALDataType type, int min_length) {
  GDALDataType src_type = TypedArray::Identify(obj);
  if (src_type == GDT_Unknown) {
    Napi::TypeError::New(env, "Unable to identify GDAL datatype of passed array object").ThrowAsJavaScriptException();

    return nullptr;
  }
  if (src_type != type) {
    std::ostringstream ss;
    ss << "Array type does not match band data type ("
       << "input: " << GDALGetDataTypeName(src_type) << ", target: " << GDALGetDataTypeName(type) << ")";

    Napi::TypeError::New(env, ss.str().c_str()).ThrowAsJavaScriptException();

    return nullptr;
  }

  if (!obj.IsTypedArray()) {
    Napi::TypeError::New(env, "Object is not a TypedArray").ThrowAsJavaScriptException();
    return nullptr;
  }

  auto array = obj.As<Napi::TypedArray>();
  if (array.ElementLength() < min_length) {
    std::ostringstream ss;
    ss << "Array length must be greater than or equal to " << min_length;

    Napi::Error::New(env, ss.str().c_str()).ThrowAsJavaScriptException();

    return nullptr;
  }

  // ByteOffset is important because of function such as TypedArray.prototype.subarray()
  // that return a new TypedArray pointing to the same ArrayBuffer with an offset
  return array.ArrayBuffer().Data() + array.ByteOffset();
}

} // namespace node_gdal
