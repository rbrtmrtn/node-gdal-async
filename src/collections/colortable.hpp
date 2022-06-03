#ifndef __NODE_GDAL_COLORTABLE_H__
#define __NODE_GDAL_COLORTABLE_H__

// node
#include <napi.h>
#include <uv.h>
#include <node_object_wrap.h>

// nan
#include <napi.h>

// gdal
#include <gdal_priv.h>

#include "../async.hpp"

using namespace Napi;
using namespace Napi;

namespace node_gdal {

class ColorTable : public Napi::ObjectWrap<ColorTable> {
    public:
  static Napi::FunctionReference constructor;

  static void Initialize(Napi::Object target);
  static Napi::Value New(const Napi::CallbackInfo &info);
  static Napi::Value New(GDALColorTable *raw, Napi::Value band);
  static Napi::Value New(GDALColorTable *raw);
  static Napi::Value toString(const Napi::CallbackInfo &info);

  static Napi::Value isSame(const Napi::CallbackInfo &info);
  static Napi::Value clone(const Napi::CallbackInfo &info);
  static Napi::Value get(const Napi::CallbackInfo &info);
  static Napi::Value count(const Napi::CallbackInfo &info);
  static Napi::Value set(const Napi::CallbackInfo &info);
  static Napi::Value ramp(const Napi::CallbackInfo &info);

  Napi::Value interpretationGetter(const Napi::CallbackInfo &info);
  Napi::Value bandGetter(const Napi::CallbackInfo &info);

  ColorTable(GDALColorTable *, long);
  inline GDALColorTable *get() {
    return this_;
  }

  void dispose();
  long uid;
  long parent_uid;

  inline bool isAlive() {
    return this_ && object_store.isAlive(uid) && (parent_uid == 0 || object_store.isAlive(parent_uid));
  }

    private:
  ~ColorTable();
  GDALColorTable *this_;
};

} // namespace node_gdal
#endif
