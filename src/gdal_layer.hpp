#ifndef __NODE_OGR_LAYER_H__
#define __NODE_OGR_LAYER_H__

// node
#include <napi.h>
#include <uv.h>
#include <node_object_wrap.h>

// nan
#include <napi.h>

// ogr
#include <ogrsf_frmts.h>

#include "gdal_dataset.hpp"

using namespace Napi;
using namespace Napi;

namespace node_gdal {

class Layer : public Napi::ObjectWrap<Layer> {
    public:
  static Napi::FunctionReference constructor;
  static void Initialize(Napi::Object target);
  static Napi::Value New(const Napi::CallbackInfo &info);
  static Napi::Value New(OGRLayer *raw, GDALDataset *raw_parent);
  static Napi::Value New(OGRLayer *raw, GDALDataset *raw_parent, bool result_set);
  static Napi::Value toString(const Napi::CallbackInfo &info);
  static Napi::Value getExtent(const Napi::CallbackInfo &info);
  static Napi::Value setAttributeFilter(const Napi::CallbackInfo &info);
  static Napi::Value setSpatialFilter(const Napi::CallbackInfo &info);
  static Napi::Value getSpatialFilter(const Napi::CallbackInfo &info);
  static Napi::Value testCapability(const Napi::CallbackInfo &info);
  GDAL_ASYNCABLE_DECLARE(syncToDisk);

  void dsSetter(const Napi::CallbackInfo &info, const Napi::Value &value);
  Napi::Value dsGetter(const Napi::CallbackInfo &info);
  Napi::Value srsGetter(const Napi::CallbackInfo &info);
  Napi::Value featuresGetter(const Napi::CallbackInfo &info);
  Napi::Value fieldsGetter(const Napi::CallbackInfo &info);
  Napi::Value nameGetter(const Napi::CallbackInfo &info);
  Napi::Value fidColumnGetter(const Napi::CallbackInfo &info);
  Napi::Value geomColumnGetter(const Napi::CallbackInfo &info);
  Napi::Value geomTypeGetter(const Napi::CallbackInfo &info);
  Napi::Value uidGetter(const Napi::CallbackInfo &info);

  Layer();
  Layer(OGRLayer *ds);
  inline OGRLayer *get() {
    return this_;
  }
  inline bool isAlive() {
    return this_ && object_store.isAlive(uid);
  }
  inline GDALDataset *getParent() {
    return parent_ds;
  }
  void dispose();
  long uid;
  long parent_uid;

    private:
  ~Layer();
  OGRLayer *this_;
  GDALDataset *parent_ds;
};

} // namespace node_gdal
#endif
