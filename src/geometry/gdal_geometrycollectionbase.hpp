#ifndef __NODE_OGR_GEOMETRYCOLLECTIONBASE_H__
#define __NODE_OGR_GEOMETRYCOLLECTIONBASE_H__

// node
#include <napi.h>
#include <uv.h>
#include <node_object_wrap.h>

// nan
#include <napi.h>

// ogr
#include <ogrsf_frmts.h>

using namespace Napi;
using namespace Napi;

#include "gdal_geometrybase.hpp"
#include "../collections/geometry_collection_children.hpp"

namespace node_gdal {

template <class T, class OGRT> class GeometryCollectionBase : public GeometryBase<T, OGRT> {

    public:
  using GeometryBase<T, OGRT>::GeometryBase;

  static Napi::Value New(const Napi::CallbackInfo &info);
  using GeometryBase<T, OGRT>::New;
};

template <class T, class OGRT> NAN_METHOD((GeometryCollectionBase<T, OGRT>::New)) {
  T *f;

  if (!info.IsConstructCall()) {
    Napi::Error::New(env, "Cannot call constructor as function, you need to use 'new' keyword")
      .ThrowAsJavaScriptException();
    return env.Null();
  }

  if (info[0].IsExternal()) {
    Napi::External ext = info[0].As<Napi::External>();
    void *ptr = ext->Value();
    f = static_cast<T *>(ptr);

  } else {
    if (info.Length() != 0) {
      Napi::Error::New(env, "GeometryCollection constructor doesn't take any arguments").ThrowAsJavaScriptException();
      return env.Null();
    }
    f = new T(new OGRT());
  }

  Napi::Value children = GeometryCollectionChildren::New(info.This());
  Napi::SetPrivate(info.This(), Napi::String::New(env, "children_"), children);

  f->Wrap(info.This());
  return info.This();
}

} // namespace node_gdal
#endif
