#ifndef __NODE_GDAL_FIELD_DEFN_COLLECTION_H__
#define __NODE_GDAL_FIELD_DEFN_COLLECTION_H__

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

// FeatureDefn.fields : FeatureDefnFields

namespace node_gdal {

class FeatureDefnFields : public Napi::ObjectWrap<FeatureDefnFields> {
    public:
  static Napi::FunctionReference constructor;

  static void Initialize(Napi::Object target);
  static Napi::Value New(const Napi::CallbackInfo &info);
  static Napi::Value New(Napi::Value layer_obj);
  static Napi::Value toString(const Napi::CallbackInfo &info);

  static Napi::Value get(const Napi::CallbackInfo &info);
  static Napi::Value getNames(const Napi::CallbackInfo &info);
  static Napi::Value count(const Napi::CallbackInfo &info);
  static Napi::Value add(const Napi::CallbackInfo &info);
  static Napi::Value remove(const Napi::CallbackInfo &info);
  static Napi::Value indexOf(const Napi::CallbackInfo &info);
  static Napi::Value reorder(const Napi::CallbackInfo &info);

  // - implement in the future -
  // static Napi::Value alter(const Napi::CallbackInfo& info);

  Napi::Value featureDefnGetter(const Napi::CallbackInfo &info);

  FeatureDefnFields();

    private:
  ~FeatureDefnFields();
};

} // namespace node_gdal
#endif
