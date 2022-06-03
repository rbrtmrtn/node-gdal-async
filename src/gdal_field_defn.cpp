
#include "gdal_field_defn.hpp"
#include "gdal_common.hpp"
#include "utils/field_types.hpp"

namespace node_gdal {

Napi::FunctionReference FieldDefn::constructor;

void FieldDefn::Initialize(Napi::Object target) {
  Napi::HandleScope scope(env);

  Napi::FunctionReference lcons = Napi::Function::New(env, FieldDefn::New);

  lcons->SetClassName(Napi::String::New(env, "FieldDefn"));

  ATTR(lcons, "name", nameGetter, nameSetter);
  ATTR(lcons, "type", typeGetter, typeSetter);
  ATTR(lcons, "justification", justificationGetter, justificationSetter);
  ATTR(lcons, "width", widthGetter, widthSetter);
  ATTR(lcons, "precision", precisionGetter, precisionSetter);
  ATTR(lcons, "ignored", ignoredGetter, ignoredSetter);
  InstanceMethod("toString", &toString),

    (target).Set(Napi::String::New(env, "FieldDefn"), Napi::GetFunction(lcons));

  constructor.Reset(lcons);
}

FieldDefn::FieldDefn(OGRFieldDefn *def) : Napi::ObjectWrap<FieldDefn>(), this_(def), owned_(false) {
  LOG("Created FieldDefn [%p]", def);
}

FieldDefn::FieldDefn() : Napi::ObjectWrap<FieldDefn>(), this_(0), owned_(false) {
}

FieldDefn::~FieldDefn() {
  if (this_) {
    LOG("Disposing FieldDefn [%p] (%s)", this_, owned_ ? "owned" : "unowned");
    if (owned_) delete this_;
    LOG("Disposed FieldDefn [%p]", this_);
    this_ = NULL;
  }
}

/**
 * @constructor
 * @class FieldDefn
 * @param {string} name Field name
 * @param {string} type Data type (see {@link Constants (OFT)|OFT}
 */
Napi::Value FieldDefn::New(const Napi::CallbackInfo &info) {

  if (!info.IsConstructCall()) {
    Napi::Error::New(env, "Cannot call constructor as function, you need to use 'new' keyword")
      .ThrowAsJavaScriptException();
    return env.Null();
  }

  if (info[0].IsExternal()) {
    Napi::External ext = info[0].As<Napi::External>();
    void *ptr = ext->Value();
    FieldDefn *f = static_cast<FieldDefn *>(ptr);
    f->Wrap(info.This());
    return info.This();
    return;
  } else {
    std::string field_name("");
    std::string type_name("string");

    NODE_ARG_STR(0, "field name", field_name);
    NODE_ARG_STR(1, "field type", type_name);

    int field_type = getFieldTypeByName(type_name);
    if (field_type < 0) {
      Napi::Error::New(env, "Unrecognized field type").ThrowAsJavaScriptException();
      return env.Null();
    }

    FieldDefn *def = new FieldDefn(new OGRFieldDefn(field_name.c_str(), static_cast<OGRFieldType>(field_type)));
    def->owned_ = true;
    def->Wrap(info.This());
  }

  return info.This();
}

Napi::Value FieldDefn::New(OGRFieldDefn *def) {
  Napi::EscapableHandleScope scope(env);
  return scope.Escape(FieldDefn::New(def, false));
}

Napi::Value FieldDefn::New(OGRFieldDefn *def, bool owned) {
  Napi::EscapableHandleScope scope(env);

  if (!def) { return scope.Escape(env.Null()); }

  // make a copy of fielddefn owned by a featuredefn
  // + no need to track when a featuredefn is destroyed
  // + no need to throw errors when a method trys to modify an owned read-only
  // fielddefn
  // - is slower

  if (!owned) { def = new OGRFieldDefn(def); }

  FieldDefn *wrapped = new FieldDefn(def);
  wrapped->owned_ = true;

  Napi::Value ext = Napi::External::New(env, wrapped);
  Napi::Object obj = Napi::NewInstance(Napi::GetFunction(Napi::New(env, FieldDefn::constructor)), 1, &ext);

  return scope.Escape(obj);
}

Napi::Value FieldDefn::toString(const Napi::CallbackInfo &info) {
  return Napi::String::New(env, "FieldDefn");
}

/**
 * @kind member
 * @name name
 * @instance
 * @memberof FieldDefn
 * @type {string}
 */
Napi::Value FieldDefn::nameGetter(const Napi::CallbackInfo &info) {
  FieldDefn *def = info.This().Unwrap<FieldDefn>();
  return SafeString::New(def->this_->GetNameRef());
}

/**
 * Data type (see {@link OFT|OFT constants}
 *
 * @kind member
 * @name type
 * @instance
 * @memberof FieldDefn
 * @type {string}
 */
Napi::Value FieldDefn::typeGetter(const Napi::CallbackInfo &info) {
  FieldDefn *def = info.This().Unwrap<FieldDefn>();
  return SafeString::New(getFieldTypeName(def->this_->GetType()));
}

/**
 * @kind member
 * @name ignored
 * @instance
 * @memberof FieldDefn
 * @type {boolean}
 */
Napi::Value FieldDefn::ignoredGetter(const Napi::CallbackInfo &info) {
  FieldDefn *def = info.This().Unwrap<FieldDefn>();
  return Napi::Boolean::New(env, def->this_->IsIgnored());
}

/**
 * Field justification (see {@link OJ|OJ constants})
 *
 * @kind member
 * @name justification
 * @instance
 * @memberof FieldDefn
 * @type {string}
 */
Napi::Value FieldDefn::justificationGetter(const Napi::CallbackInfo &info) {
  FieldDefn *def = info.This().Unwrap<FieldDefn>();
  OGRJustification justification = def->this_->GetJustify();
  if (justification == OJRight) {
    return Napi::String::New(env, "Right");
    return;
  }
  if (justification == OJLeft) {
    return Napi::String::New(env, "Left");
    return;
  }
  return env.Undefined();
}

/**
 * @kind member
 * @name width
 * @instance
 * @memberof FieldDefn
 * @type {number}
 */
Napi::Value FieldDefn::widthGetter(const Napi::CallbackInfo &info) {
  FieldDefn *def = info.This().Unwrap<FieldDefn>();
  return Napi::Number::New(env, def->this_->GetWidth());
}

/**
 * @kind member
 * @name precision
 * @instance
 * @memberof FieldDefn
 * @type {number}
 */
Napi::Value FieldDefn::precisionGetter(const Napi::CallbackInfo &info) {
  FieldDefn *def = info.This().Unwrap<FieldDefn>();
  return Napi::Number::New(env, def->this_->GetPrecision());
}

void FieldDefn::nameSetter(const Napi::CallbackInfo &info, const Napi::Value &value) {
  FieldDefn *def = info.This().Unwrap<FieldDefn>();
  if (!value.IsString()) {
    Napi::Error::New(env, "Name must be string").ThrowAsJavaScriptException();
    return env.Null();
  }
  std::string name = value.As<Napi::String>().Utf8Value().c_str();
  def->this_->SetName(name.c_str());
}

void FieldDefn::typeSetter(const Napi::CallbackInfo &info, const Napi::Value &value) {
  FieldDefn *def = info.This().Unwrap<FieldDefn>();
  if (!value.IsString()) {
    Napi::Error::New(env, "type must be a string").ThrowAsJavaScriptException();
    return env.Null();
  }
  std::string name = value.As<Napi::String>().Utf8Value().c_str();
  int type = getFieldTypeByName(name.c_str());
  if (type < 0) {
    Napi::Error::New(env, "Unrecognized field type").ThrowAsJavaScriptException();

  } else {
    def->this_->SetType(OGRFieldType(type));
  }
}

void FieldDefn::justificationSetter(const Napi::CallbackInfo &info, const Napi::Value &value) {
  FieldDefn *def = info.This().Unwrap<FieldDefn>();

  OGRJustification justification;
  std::string str = value.As<Napi::String>().Utf8Value().c_str();
  if (value.IsString()) {
    if (str == "Left") {
      justification = OJLeft;
    } else if (str == "Right") {
      justification = OJRight;
    } else if (str == "Undefined") {
      justification = OJUndefined;
    } else {
      Napi::Error::New(env, "Unrecognized justification").ThrowAsJavaScriptException();
      return env.Null();
    }
  } else if (value.IsNull() || value.IsNull()) {
    justification = OJUndefined;
  } else {
    Napi::Error::New(env, "justification must be a string or undefined").ThrowAsJavaScriptException();
    return env.Null();
  }

  def->this_->SetJustify(justification);
}

void FieldDefn::widthSetter(const Napi::CallbackInfo &info, const Napi::Value &value) {
  FieldDefn *def = info.This().Unwrap<FieldDefn>();
  if (!value.IsNumber()) {
    Napi::Error::New(env, "width must be an integer").ThrowAsJavaScriptException();
    return env.Null();
  }
  def->this_->SetWidth(value.As<Napi::Number>().Int64Value().ToChecked());
}

void FieldDefn::precisionSetter(const Napi::CallbackInfo &info, const Napi::Value &value) {
  FieldDefn *def = info.This().Unwrap<FieldDefn>();
  if (!value.IsNumber()) {
    Napi::Error::New(env, "precision must be an integer").ThrowAsJavaScriptException();
    return env.Null();
  }
  def->this_->SetPrecision(value.As<Napi::Number>().Int64Value().ToChecked());
}

void FieldDefn::ignoredSetter(const Napi::CallbackInfo &info, const Napi::Value &value) {
  FieldDefn *def = info.This().Unwrap<FieldDefn>();
  if (!value->IsBoolean()) {
    Napi::Error::New(env, "ignored must be a boolean").ThrowAsJavaScriptException();
    return env.Null();
  }
  def->this_->SetIgnored(value.As<Napi::Number>().Int64Value().ToChecked());
}

} // namespace node_gdal
