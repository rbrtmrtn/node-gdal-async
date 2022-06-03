#ifndef __NODE_OGR_FEATURE_H__
#define __NODE_OGR_FEATURE_H__

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

namespace node_gdal {

class Feature : public Napi::ObjectWrap<Feature> {
    public:
  static Napi::FunctionReference constructor;
  static void Initialize(Napi::Object target);
  static Napi::Value New(const Napi::CallbackInfo &info);
  static Napi::Value New(OGRFeature *feature);
  static Napi::Value New(OGRFeature *feature, bool owned);
  static Napi::Value toString(const Napi::CallbackInfo &info);
  static Napi::Value getGeometry(const Napi::CallbackInfo &info);
  //	static Napi::Value setGeometryDirectly(const Napi::CallbackInfo& info);
  static Napi::Value setGeometry(const Napi::CallbackInfo &info);
  //  static Napi::Value stealGeometry(const Napi::CallbackInfo& info);
  static Napi::Value clone(const Napi::CallbackInfo &info);
  static Napi::Value equals(const Napi::CallbackInfo &info);
  static Napi::Value getFieldDefn(const Napi::CallbackInfo &info);
  static Napi::Value setFrom(const Napi::CallbackInfo &info);
  static Napi::Value destroy(const Napi::CallbackInfo &info);

  Napi::Value fieldsGetter(const Napi::CallbackInfo &info);
  Napi::Value fidGetter(const Napi::CallbackInfo &info);
  Napi::Value defnGetter(const Napi::CallbackInfo &info);

  void fidSetter(const Napi::CallbackInfo &info, const Napi::Value &value);

  Feature();
  Feature(OGRFeature *feature);
  inline OGRFeature *get() {
    return this_;
  }
  inline bool isAlive() {
    return this_;
  }
  void dispose();

    private:
  ~Feature();
  OGRFeature *this_;
  bool owned_;
  // int size_;
};

} // namespace node_gdal
#endif
