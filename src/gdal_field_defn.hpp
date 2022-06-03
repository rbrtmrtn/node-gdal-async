#ifndef __NODE_OGR_FIELD_DEFN_H__
#define __NODE_OGR_FIELD_DEFN_H__

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

class FieldDefn : public Napi::ObjectWrap<FieldDefn> {
    public:
  static Napi::FunctionReference constructor;
  static void Initialize(Napi::Object target);
  static Napi::Value New(const Napi::CallbackInfo &info);
  static Napi::Value New(OGRFieldDefn *def);
  static Napi::Value New(OGRFieldDefn *def, bool owned);
  static Napi::Value toString(const Napi::CallbackInfo &info);

  Napi::Value nameGetter(const Napi::CallbackInfo &info);
  Napi::Value typeGetter(const Napi::CallbackInfo &info);
  Napi::Value justificationGetter(const Napi::CallbackInfo &info);
  Napi::Value precisionGetter(const Napi::CallbackInfo &info);
  Napi::Value widthGetter(const Napi::CallbackInfo &info);
  Napi::Value ignoredGetter(const Napi::CallbackInfo &info);

  void nameSetter(const Napi::CallbackInfo &info, const Napi::Value &value);
  void typeSetter(const Napi::CallbackInfo &info, const Napi::Value &value);
  void justificationSetter(const Napi::CallbackInfo &info, const Napi::Value &value);
  void precisionSetter(const Napi::CallbackInfo &info, const Napi::Value &value);
  void widthSetter(const Napi::CallbackInfo &info, const Napi::Value &value);
  void ignoredSetter(const Napi::CallbackInfo &info, const Napi::Value &value);

  FieldDefn();
  FieldDefn(OGRFieldDefn *def);
  inline OGRFieldDefn *get() {
    return this_;
  }
  inline bool isAlive() {
    return this_;
  }

    private:
  ~FieldDefn();
  OGRFieldDefn *this_;
  bool owned_;
};

} // namespace node_gdal
#endif
