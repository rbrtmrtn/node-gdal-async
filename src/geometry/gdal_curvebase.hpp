#ifndef __NODE_OGR_CURVEBASE_H__
#define __NODE_OGR_CURVEBASE_H__

// node
#include <napi.h>
#include <uv.h>
#include <node_object_wrap.h>

// nan
#include <napi.h>

// ogr
#include <ogrsf_frmts.h>

#include "gdal_geometrybase.hpp"

using namespace Napi;
using namespace Napi;

namespace node_gdal {

template <class T, class OGRT, class COLLECTIONT> class CurveBase : public GeometryBase<T, OGRT> {
    public:
  using GeometryBase<T, OGRT>::GeometryBase;
  static Napi::Value New(const Napi::CallbackInfo &info);
  using GeometryBase<T, OGRT>::New;

    protected:
  static void SetPrivate(Napi::ADDON_REGISTER_FUNCTION_ARGS_TYPE, Napi::Value);
};

template <class T, class OGRT, class COLLECTIONT> NAN_METHOD((CurveBase<T, OGRT, COLLECTIONT>::New)) {
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
      Napi::Error::New(env, "String constructor doesn't take any arguments").ThrowAsJavaScriptException();
      return env.Null();
    }
    f = new T(new OGRT());
  }

  Napi::Value points = COLLECTIONT::New(info.This());
  T::SetPrivate(info.This(), points);

  f->Wrap(info.This());
  return info.This();
}

template <class T, class OGRT, class COLLECTIONT>
void CurveBase<T, OGRT, COLLECTIONT>::SetPrivate(Napi::ADDON_REGISTER_FUNCTION_ARGS_TYPE _this, Napi::Value value) {
  Napi::SetPrivate(_this, Napi::String::New(env, "points_"), value);
};

} // namespace node_gdal
#endif
