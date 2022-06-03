#include "feature_defn_fields.hpp"
#include "../gdal_common.hpp"
#include "../gdal_feature_defn.hpp"
#include "../gdal_field_defn.hpp"

namespace node_gdal {

Napi::FunctionReference FeatureDefnFields::constructor;

void FeatureDefnFields::Initialize(Napi::Object target) {
  Napi::HandleScope scope(env);

  Napi::FunctionReference lcons = Napi::Function::New(env, FeatureDefnFields::New);

  lcons->SetClassName(Napi::String::New(env, "FeatureDefnFields"));

  InstanceMethod("toString", &toString),
  InstanceMethod("count", &count),
  InstanceMethod("get", &get),
  InstanceMethod("remove", &remove),
  InstanceMethod("getNames", &getNames),
  InstanceMethod("indexOf", &indexOf),
  InstanceMethod("reorder", &reorder),
  InstanceMethod("add", &add),
  // InstanceMethod("alter", &alter),

  ATTR_DONT_ENUM(lcons, "featureDefn", featureDefnGetter, READ_ONLY_SETTER);

  (target).Set(Napi::String::New(env, "FeatureDefnFields"), Napi::GetFunction(lcons));

  constructor.Reset(lcons);
}

FeatureDefnFields::FeatureDefnFields() : Napi::ObjectWrap<FeatureDefnFields>(){
}

FeatureDefnFields::~FeatureDefnFields() {
}

/**
 * An encapsulation of a {@link FeatureDefn}'s fields.
 *
 * @class FeatureDefnFields
 */
Napi::Value FeatureDefnFields::New(const Napi::CallbackInfo& info) {

  if (!info.IsConstructCall()) {
    Napi::Error::New(env, "Cannot call constructor as function, you need to use 'new' keyword").ThrowAsJavaScriptException();
    return env.Null();
  }
  if (info[0].IsExternal()) {
    Napi::External ext = info[0].As<Napi::External>();
    void *ptr = ext->Value();
    FeatureDefnFields *feature_def = static_cast<FeatureDefnFields *>(ptr);
    feature_def->Wrap(info.This());
    return info.This();
    return;
  } else {
    Napi::Error::New(env, "Cannot create FeatureDefnFields directly").ThrowAsJavaScriptException();
    return env.Null();
  }
}

Napi::Value FeatureDefnFields::New(Napi::Value feature_defn) {
  Napi::Env env = feature_defn.Env();
  Napi::EscapableHandleScope scope(env);

  FeatureDefnFields *wrapped = new FeatureDefnFields();

  Napi::Value ext = Napi::External::New(env, wrapped);
  Napi::Object obj =
    Napi::NewInstance(Napi::GetFunction(Napi::New(env, FeatureDefnFields::constructor)), 1, &ext)
      ;
  Napi::SetPrivate(obj, Napi::String::New(env, "parent_"), feature_defn);

  return scope.Escape(obj);
}

Napi::Value FeatureDefnFields::toString(const Napi::CallbackInfo& info) {
  return Napi::String::New(env, "FeatureDefnFields");
}

/**
 * Returns the number of fields.
 *
 * @method count
 * @instance
 * @memberof FeatureDefnFields
 * @return {number}
 */
Napi::Value FeatureDefnFields::count(const Napi::CallbackInfo& info) {

  Napi::Object parent =
    Napi::GetPrivate(info.This(), Napi::String::New(env, "parent_")).As<Napi::Object>();
  FeatureDefn *feature_def = parent.Unwrap<FeatureDefn>();
  if (!feature_def->isAlive()) {
    Napi::Error::New(env, "FeatureDefn object already destroyed").ThrowAsJavaScriptException();
    return env.Null();
  }

  return Napi::Number::New(env, feature_def->get()->GetFieldCount());
}

/**
 * Returns the index of field definition.
 *
 * @method indexOf
 * @instance
 * @memberof FeatureDefnFields
 * @param {string} name
 * @return {number} Index or `-1` if not found.
 */
Napi::Value FeatureDefnFields::indexOf(const Napi::CallbackInfo& info) {

  Napi::Object parent =
    Napi::GetPrivate(info.This(), Napi::String::New(env, "parent_")).As<Napi::Object>();
  FeatureDefn *feature_def = parent.Unwrap<FeatureDefn>();
  if (!feature_def->isAlive()) {
    Napi::Error::New(env, "FeatureDefn object already destroyed").ThrowAsJavaScriptException();
    return env.Null();
  }

  std::string name("");
  NODE_ARG_STR(0, "field name", name);

  return Napi::Number::New(env, feature_def->get()->GetFieldIndex(name.c_str()));
}

/**
 * Returns a field definition.
 *
 * @method get
 * @instance
 * @memberof FeatureDefnFields
 * @param {string|number} key Field name or index
 * @throws {Error}
 * @return {FieldDefn}
 */
Napi::Value FeatureDefnFields::get(const Napi::CallbackInfo& info) {

  Napi::Object parent =
    Napi::GetPrivate(info.This(), Napi::String::New(env, "parent_")).As<Napi::Object>();
  FeatureDefn *feature_def = parent.Unwrap<FeatureDefn>();
  if (!feature_def->isAlive()) {
    Napi::Error::New(env, "FeatureDefn object already destroyed").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (info.Length() < 1) {
    Napi::Error::New(env, "Field index or name must be given").ThrowAsJavaScriptException();
    return env.Null();
  }

  int field_index;
  ARG_FIELD_ID(0, feature_def->get(), field_index);

  CPLErrorReset();
  auto r = feature_def->get()->GetFieldDefn(field_index);
  if (r == nullptr) { throw CPLGetLastErrorMsg(); }
  return FieldDefn::New(r);
}

/**
 * Returns a list of field names.
 *
 * @method getNames
 * @instance
 * @memberof FeatureDefnFields
 * @return {string[]} List of field names.
 */
Napi::Value FeatureDefnFields::getNames(const Napi::CallbackInfo& info) {

  Napi::Object parent =
    Napi::GetPrivate(info.This(), Napi::String::New(env, "parent_")).As<Napi::Object>();
  FeatureDefn *feature_def = parent.Unwrap<FeatureDefn>();
  if (!feature_def->isAlive()) {
    Napi::Error::New(env, "FeatureDefn object already destroyed").ThrowAsJavaScriptException();
    return env.Null();
  }

  int n = feature_def->get()->GetFieldCount();
  Napi::Array result = Napi::Array::New(env, n);

  for (int i = 0; i < n; i++) {
    OGRFieldDefn *field_def = feature_def->get()->GetFieldDefn(i);
    (result).Set(i, SafeString::New(field_def->GetNameRef()));
  }

  return result;
}

/**
 * Removes a field definition.
 *
 * @method remove
 * @instance
 * @memberof FeatureDefnFields
 * @throws {Error}
 * @param {string|number} key Field name or index
 */
Napi::Value FeatureDefnFields::remove(const Napi::CallbackInfo& info) {

  Napi::Object parent =
    Napi::GetPrivate(info.This(), Napi::String::New(env, "parent_")).As<Napi::Object>();
  FeatureDefn *feature_def = parent.Unwrap<FeatureDefn>();
  if (!feature_def->isAlive()) {
    Napi::Error::New(env, "FeatureDefn object already destroyed").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (info.Length() < 1) {
    Napi::Error::New(env, "Field index or name must be given").ThrowAsJavaScriptException();
    return env.Null();
  }

  int field_index;
  ARG_FIELD_ID(0, feature_def->get(), field_index);

  int err = feature_def->get()->DeleteFieldDefn(field_index);
  if (err) {
    NODE_THROW_OGRERR(err);
    return;
  }

  return;
}

/**
 * Adds field definition(s).
 *
 * @method add
 * @instance
 * @memberof FeatureDefnFields
 * @throws {Error}
 * @param {FieldDefn|FieldDefn[]} fields
 */
Napi::Value FeatureDefnFields::add(const Napi::CallbackInfo& info) {

  Napi::Object parent =
    Napi::GetPrivate(info.This(), Napi::String::New(env, "parent_")).As<Napi::Object>();
  FeatureDefn *feature_def = parent.Unwrap<FeatureDefn>();
  if (!feature_def->isAlive()) {
    Napi::Error::New(env, "FeatureDefn object already destroyed").ThrowAsJavaScriptException();
    return env.Null();
  }
  if (info.Length() < 1) {
    Napi::Error::New(env, "field definition(s) must be given").ThrowAsJavaScriptException();
    return env.Null();
  }

  FieldDefn *field_def;

  if (info[0].IsArray()) {
    Napi::Array array = info[0].As<Napi::Array>();
    int n = array->Length();
    for (int i = 0; i < n; i++) {
      Napi::Value element = (array).Get(i);
      if (IS_WRAPPED(element, FieldDefn)) {
        field_def = element.As<Napi::Object>().Unwrap<FieldDefn>();
        feature_def->get()->AddFieldDefn(field_def->get());
      } else {
        Napi::Error::New(env, "All array elements must be FieldDefn objects").ThrowAsJavaScriptException();
        return env.Null();
      }
    }
  } else if (IS_WRAPPED(info[0], FieldDefn)) {
    field_def = info[0].As<Napi::Object>().Unwrap<FieldDefn>();
    feature_def->get()->AddFieldDefn(field_def->get());
  } else {
    Napi::Error::New(env, "field definition(s) must be a FieldDefn object or array of FieldDefn objects").ThrowAsJavaScriptException();
    return env.Null();
  }

  return;
}

/**
 * Reorders the fields.
 *
 * @example
 *
 * // reverse fields:
 * featureDef.fields.reorder([2, 1, 0]);
 *
 * @method reorder
 * @instance
 * @memberof FeatureDefnFields
 * @throws {Error}
 * @param {number[]} map An array representing the new field order.
 */
Napi::Value FeatureDefnFields::reorder(const Napi::CallbackInfo& info) {

  Napi::Object parent =
    Napi::GetPrivate(info.This(), Napi::String::New(env, "parent_")).As<Napi::Object>();
  FeatureDefn *feature_def = parent.Unwrap<FeatureDefn>();
  if (!feature_def->isAlive()) {
    Napi::Error::New(env, "FeatureDefn object already destroyed").ThrowAsJavaScriptException();
    return env.Null();
  }

  Napi::Array field_map = Napi::Array::New(env, 0);
  NODE_ARG_ARRAY(0, "field map", field_map);

  int n = feature_def->get()->GetFieldCount();
  OGRErr err = 0;

  if ((int)field_map->Length() != n) {
    Napi::Error::New(env, "Array length must match field count").ThrowAsJavaScriptException();
    return env.Null();
  }

  int *field_map_array = new int[n];

  for (int i = 0; i < n; i++) {
    Napi::Value val = (field_map).Get(i);
    if (!val.IsNumber()) {
      delete[] field_map_array;
      Napi::Error::New(env, "Array must only contain integers").ThrowAsJavaScriptException();
      return env.Null();
    }

    int key = val.As<Napi::Number>().Int64Value().ToChecked();
    if (key < 0 || key >= n) {
      delete[] field_map_array;
      Napi::Error::New(env, "Values must be between 0 and field count - 1").ThrowAsJavaScriptException();
      return env.Null();
    }

    field_map_array[i] = key;
  }

  err = feature_def->get()->ReorderFieldDefns(field_map_array);

  delete[] field_map_array;

  if (err) {
    NODE_THROW_OGRERR(err);
    return;
  }
  return;
}

/**
 * Returns the parent feature definition.
 *
 * @readonly
 * @kind member
 * @name featureDefn
 * @instance
 * @memberof FeatureDefnFields
 * @type {FeatureDefn}
 */
Napi::Value FeatureDefnFields::featureDefnGetter(const Napi::CallbackInfo& info) {
  return Napi::GetPrivate(info.This(), Napi::String::New(env, "parent_"));
}

} // namespace node_gdal
