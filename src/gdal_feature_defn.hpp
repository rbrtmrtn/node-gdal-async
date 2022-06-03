#ifndef __NODE_OGR_FEATURE_DEFN_H__
#define __NODE_OGR_FEATURE_DEFN_H__

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

class FeatureDefn : public Napi::ObjectWrap<FeatureDefn> {
    public:
  static Napi::FunctionReference constructor;
  static void Initialize(Napi::Object target);
  static Napi::Value New(const Napi::CallbackInfo &info);
  static Napi::Value New(OGRFeatureDefn *def);
  static Napi::Value New(OGRFeatureDefn *def, bool owned);
  static Napi::Value toString(const Napi::CallbackInfo &info);
  static Napi::Value clone(const Napi::CallbackInfo &info);

  Napi::Value fieldsGetter(const Napi::CallbackInfo &info);
  Napi::Value nameGetter(const Napi::CallbackInfo &info);
  Napi::Value geomTypeGetter(const Napi::CallbackInfo &info);
  Napi::Value geomIgnoredGetter(const Napi::CallbackInfo &info);
  Napi::Value styleIgnoredGetter(const Napi::CallbackInfo &info);

  void geomTypeSetter(const Napi::CallbackInfo &info, const Napi::Value &value);
  void geomIgnoredSetter(const Napi::CallbackInfo &info, const Napi::Value &value);
  void styleIgnoredSetter(const Napi::CallbackInfo &info, const Napi::Value &value);

  FeatureDefn();
  FeatureDefn(OGRFeatureDefn *def);
  inline OGRFeatureDefn *get() {
    return this_;
  }
  inline bool isAlive() {
    return this_;
  }

    private:
  ~FeatureDefn();
  OGRFeatureDefn *this_;
  bool owned_;
};

} // namespace node_gdal
#endif
