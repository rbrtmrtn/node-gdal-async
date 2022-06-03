#ifndef __NODE_GDAL_FIELD_COLLECTION_H__
#define __NODE_GDAL_FIELD_COLLECTION_H__

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

namespace node_gdal {

class FeatureFields : public Napi::ObjectWrap<FeatureFields> {
    public:
  static Napi::FunctionReference constructor;

  static void Initialize(Napi::Object target);
  static Napi::Value New(const Napi::CallbackInfo &info);
  static Napi::Value New(Napi::Value layer_obj);
  static Napi::Value toString(const Napi::CallbackInfo &info);
  static Napi::Value toArray(const Napi::CallbackInfo &info);
  static Napi::Value toObject(const Napi::CallbackInfo &info);

  static Napi::Value get(const Napi::CallbackInfo &info);
  static Napi::Value getNames(const Napi::CallbackInfo &info);
  static Napi::Value set(const Napi::CallbackInfo &info);
  static Napi::Value reset(const Napi::CallbackInfo &info);
  static Napi::Value count(const Napi::CallbackInfo &info);
  static Napi::Value indexOf(const Napi::CallbackInfo &info);

  static Napi::Value get(OGRFeature *f, int field_index);
  static Napi::Value getFieldAsIntegerList(OGRFeature *feature, int field_index);
  static Napi::Value getFieldAsInteger64List(OGRFeature *feature, int field_index);
  static Napi::Value getFieldAsDoubleList(OGRFeature *feature, int field_index);
  static Napi::Value getFieldAsStringList(OGRFeature *feature, int field_index);
  static Napi::Value getFieldAsBinary(OGRFeature *feature, int field_index);
  static Napi::Value getFieldAsDateTime(OGRFeature *feature, int field_index);

  Napi::Value featureGetter(const Napi::CallbackInfo &info);

  FeatureFields();

    private:
  ~FeatureFields();
};

} // namespace node_gdal
#endif
