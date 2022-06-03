#ifndef __NODE_GDAL_LYR_FIELD_DEFN_COLLECTION_H__
#define __NODE_GDAL_LYR_FIELD_DEFN_COLLECTION_H__

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

// Layer.fields : LayerFields

// Identical to FeatureDefn.fields object from the outside
// but on the inside it uses the parent layer
// to create/modify fields instead of illegally
// adding them directly to the layer definition

namespace node_gdal {

class LayerFields : public Napi::ObjectWrap<LayerFields> {
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

  Napi::Value layerGetter(const Napi::CallbackInfo &info);

  LayerFields();

    private:
  ~LayerFields();
};

} // namespace node_gdal
#endif
