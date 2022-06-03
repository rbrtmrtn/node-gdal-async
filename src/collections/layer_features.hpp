#ifndef __NODE_GDAL_FEATURE_COLLECTION_H__
#define __NODE_GDAL_FEATURE_COLLECTION_H__

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

class LayerFeatures : public Napi::ObjectWrap<LayerFeatures> {
    public:
  static Napi::FunctionReference constructor;

  static void Initialize(Napi::Object target);
  static Napi::Value New(const Napi::CallbackInfo &info);
  static Napi::Value New(Napi::Value layer_obj);
  static Napi::Value toString(const Napi::CallbackInfo &info);

  GDAL_ASYNCABLE_DECLARE(get);
  GDAL_ASYNCABLE_DECLARE(first);
  GDAL_ASYNCABLE_DECLARE(next);
  GDAL_ASYNCABLE_DECLARE(count);
  GDAL_ASYNCABLE_DECLARE(add);
  GDAL_ASYNCABLE_DECLARE(set);
  GDAL_ASYNCABLE_DECLARE(remove);

  Napi::Value layerGetter(const Napi::CallbackInfo &info);

  LayerFeatures();

    private:
  ~LayerFeatures();
};

} // namespace node_gdal
#endif
