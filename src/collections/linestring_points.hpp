#ifndef __NODE_GDAL_LINESTRING_POINTS_H__
#define __NODE_GDAL_LINESTRING_POINTS_H__

// node
#include <napi.h>
#include <uv.h>
#include <node_object_wrap.h>

// nan
#include <napi.h>

// gdal
#include <gdal_priv.h>

using namespace Napi;
using namespace Napi;

// LineString.children

namespace node_gdal {

class LineStringPoints : public Napi::ObjectWrap<LineStringPoints> {
    public:
  static Napi::FunctionReference constructor;

  static void Initialize(Napi::Object target);
  static Napi::Value New(const Napi::CallbackInfo &info);
  static Napi::Value New(Napi::Value geom);
  static Napi::Value toString(const Napi::CallbackInfo &info);

  static Napi::Value add(const Napi::CallbackInfo &info);
  static Napi::Value get(const Napi::CallbackInfo &info);
  static Napi::Value set(const Napi::CallbackInfo &info);
  static Napi::Value count(const Napi::CallbackInfo &info);
  static Napi::Value reverse(const Napi::CallbackInfo &info);
  static Napi::Value resize(const Napi::CallbackInfo &info);

  LineStringPoints();

    private:
  ~LineStringPoints();
};

} // namespace node_gdal
#endif
