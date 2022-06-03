
#include "gdal_feature_defn.hpp"
#include "collections/feature_defn_fields.hpp"
#include "gdal_common.hpp"
#include "gdal_field_defn.hpp"

namespace node_gdal {

Napi::FunctionReference FeatureDefn::constructor;

void FeatureDefn::Initialize(Napi::Object target) {
  Napi::HandleScope scope(env);

  Napi::FunctionReference lcons = Napi::Function::New(env, FeatureDefn::New);

  lcons->SetClassName(Napi::String::New(env, "FeatureDefn"));

  InstanceMethod("toString", &toString),
  InstanceMethod("clone", &clone),

  ATTR(lcons, "name", nameGetter, READ_ONLY_SETTER);
  ATTR(lcons, "fields", fieldsGetter, READ_ONLY_SETTER);
  ATTR(lcons, "styleIgnored", styleIgnoredGetter, styleIgnoredSetter);
  ATTR(lcons, "geomIgnored", geomIgnoredGetter, geomIgnoredSetter);
  ATTR(lcons, "geomType", geomTypeGetter, geomTypeSetter);

  (target).Set(Napi::String::New(env, "FeatureDefn"), Napi::GetFunction(lcons));

  constructor.Reset(lcons);
}

FeatureDefn::FeatureDefn(OGRFeatureDefn *def) : Napi::ObjectWrap<FeatureDefn>(), this_(def), owned_(true) {
  LOG("Created FeatureDefn [%p]", def);
}

FeatureDefn::FeatureDefn() : Napi::ObjectWrap<FeatureDefn>(), this_(0), owned_(true) {
}

FeatureDefn::~FeatureDefn() {
  if (this_) {
    LOG("Disposing FeatureDefn [%p] (%s)", this_, owned_ ? "owned" : "unowned");
    if (owned_) this_->Release();
    this_ = NULL;
    LOG("Disposed FeatureDefn [%p]", this_);
  }
}

/**
 * Definition of a feature class or feature layer.
 *
 * @constructor
 * @class FeatureDefn
 */
Napi::Value FeatureDefn::New(const Napi::CallbackInfo& info) {
  FeatureDefn *f;

  if (!info.IsConstructCall()) {
    Napi::Error::New(env, "Cannot call constructor as function, you need to use 'new' keyword").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (info[0].IsExternal()) {
    Napi::External ext = info[0].As<Napi::External>();
    void *ptr = ext->Value();
    f = static_cast<FeatureDefn *>(ptr);
  } else {
    if (info.Length() != 0) {
      Napi::Error::New(env, "FeatureDefn constructor doesn't take any arguments").ThrowAsJavaScriptException();
      return env.Null();
    }
    f = new FeatureDefn(new OGRFeatureDefn());
    f->this_->Reference();
  }

  Napi::Value fields = FeatureDefnFields::New(info.This());
  Napi::SetPrivate(info.This(), Napi::String::New(env, "fields_"), fields);

  f->Wrap(info.This());
  return info.This();
}

Napi::Value FeatureDefn::New(OGRFeatureDefn *def) {
  Napi::EscapableHandleScope scope(env);
  return scope.Escape(FeatureDefn::New(def, false));
}

Napi::Value FeatureDefn::New(OGRFeatureDefn *def, bool owned) {
  Napi::EscapableHandleScope scope(env);

  if (!def) { return scope.Escape(env.Null()); }

  // make a copy of featuredefn owned by a layer
  // + no need to track when a layer is destroyed
  // + no need to throw errors when a method trys to modify an owned read-only
  // featuredefn
  // - is slower

  // TODO: cloning maybe unnecessary if reference counting is done right.
  //      def->Reference(); def->Release();

  if (!owned) { def = def->Clone(); }

  FeatureDefn *wrapped = new FeatureDefn(def);
  wrapped->owned_ = true;
  def->Reference();

  Napi::Value ext = Napi::External::New(env, wrapped);
  Napi::Object obj =
    Napi::NewInstance(Napi::GetFunction(Napi::New(env, FeatureDefn::constructor)), 1, &ext);

  return scope.Escape(obj);
}

Napi::Value FeatureDefn::toString(const Napi::CallbackInfo& info) {
  return Napi::String::New(env, "FeatureDefn");
}

/**
 * Clones the feature definition.
 *
 * @method clone
 * @instance
 * @memberof FeatureDefn
 * @return {FeatureDefn}
 */
Napi::Value FeatureDefn::clone(const Napi::CallbackInfo& info) {
  FeatureDefn *def = info.This().Unwrap<FeatureDefn>();
  return FeatureDefn::New(def->this_->Clone());
}

/**
 * @readonly
 * @kind member
 * @name name
 * @instance
 * @memberof FeatureDefn
 * @type {string}
 */
Napi::Value FeatureDefn::nameGetter(const Napi::CallbackInfo& info) {
  FeatureDefn *def = info.This().Unwrap<FeatureDefn>();
  return SafeString::New(def->this_->GetName());
}

/**
 * WKB geometry type ({@link wkbGeometryType|see table}
 *
 * @kind member
 * @name geomType
 * @instance
 * @memberof FeatureDefn
 * @type {number}
 */
Napi::Value FeatureDefn::geomTypeGetter(const Napi::CallbackInfo& info) {
  FeatureDefn *def = info.This().Unwrap<FeatureDefn>();
  return Napi::Number::New(env, def->this_->GetGeomType());
}

/**
 * @kind member
 * @name geomIgnored
 * @instance
 * @memberof FeatureDefn
 * @type {boolean}
 */
Napi::Value FeatureDefn::geomIgnoredGetter(const Napi::CallbackInfo& info) {
  FeatureDefn *def = info.This().Unwrap<FeatureDefn>();
  return Napi::Boolean::New(env, def->this_->IsGeometryIgnored());
}

/**
 * @kind member
 * @name styleIgnored
 * @instance
 * @memberof FeatureDefn
 * @type {boolean}
 */
Napi::Value FeatureDefn::styleIgnoredGetter(const Napi::CallbackInfo& info) {
  FeatureDefn *def = info.This().Unwrap<FeatureDefn>();
  return Napi::Boolean::New(env, def->this_->IsStyleIgnored());
}

/**
 * @readonly
 * @kind member
 * @name fields
 * @instance
 * @memberof FeatureDefn
 * @type {FeatureDefnFields}
 */
Napi::Value FeatureDefn::fieldsGetter(const Napi::CallbackInfo& info) {
  return Napi::GetPrivate(info.This(), Napi::String::New(env, "fields_"));
}

void FeatureDefn::geomTypeSetter(const Napi::CallbackInfo& info, const Napi::Value& value) {
  FeatureDefn *def = info.This().Unwrap<FeatureDefn>();
  if (!value.IsNumber()) {
    Napi::Error::New(env, "geomType must be an integer").ThrowAsJavaScriptException();
    return env.Null();
  }
  def->this_->SetGeomType(OGRwkbGeometryType(value.As<Napi::Number>().Int64Value().ToChecked()));
}

void FeatureDefn::geomIgnoredSetter(const Napi::CallbackInfo& info, const Napi::Value& value) {
  FeatureDefn *def = info.This().Unwrap<FeatureDefn>();
  if (!value->IsBoolean()) {
    Napi::Error::New(env, "geomIgnored must be a boolean").ThrowAsJavaScriptException();
    return env.Null();
  }
  def->this_->SetGeometryIgnored(value.As<Napi::Number>().Int64Value().ToChecked());
}

void FeatureDefn::styleIgnoredSetter(const Napi::CallbackInfo& info, const Napi::Value& value) {
  FeatureDefn *def = info.This().Unwrap<FeatureDefn>();
  if (!value->IsBoolean()) {
    Napi::Error::New(env, "styleIgnored must be a boolean").ThrowAsJavaScriptException();
    return env.Null();
  }
  def->this_->SetStyleIgnored(value.As<Napi::Number>().Int64Value().ToChecked());
}

} // namespace node_gdal
