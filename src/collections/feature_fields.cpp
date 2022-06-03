#include "../gdal_common.hpp"
#include "../gdal_feature.hpp"
#include "feature_fields.hpp"

namespace node_gdal {

Napi::FunctionReference FeatureFields::constructor;

void FeatureFields::Initialize(Napi::Object target) {
  Napi::HandleScope scope(env);

  Napi::FunctionReference lcons = Napi::Function::New(env, FeatureFields::New);

  lcons->SetClassName(Napi::String::New(env, "FeatureFields"));

  InstanceMethod("toString", &toString), InstanceMethod("toObject", &toObject), InstanceMethod("toArray", &toArray),
    InstanceMethod("count", &count), InstanceMethod("get", &get), InstanceMethod("getNames", &getNames),
    InstanceMethod("set", &set), InstanceMethod("reset", &reset), InstanceMethod("indexOf", &indexOf),

    ATTR_DONT_ENUM(lcons, "feature", featureGetter, READ_ONLY_SETTER);

  (target).Set(Napi::String::New(env, "FeatureFields"), Napi::GetFunction(lcons));

  constructor.Reset(lcons);
}

FeatureFields::FeatureFields() : Napi::ObjectWrap<FeatureFields>() {
}

FeatureFields::~FeatureFields() {
}

/**
 * An encapsulation of all field data that makes up a {@link Feature}.
 *
 * @class FeatureFields
 */
Napi::Value FeatureFields::New(const Napi::CallbackInfo &info) {

  if (!info.IsConstructCall()) {
    Napi::Error::New(env, "Cannot call constructor as function, you need to use 'new' keyword")
      .ThrowAsJavaScriptException();
    return env.Null();
  }
  if (info[0].IsExternal()) {
    Napi::External ext = info[0].As<Napi::External>();
    void *ptr = ext->Value();
    FeatureFields *f = static_cast<FeatureFields *>(ptr);
    f->Wrap(info.This());
    return info.This();
    return;
  } else {
    Napi::Error::New(env, "Cannot create FeatureFields directly").ThrowAsJavaScriptException();
    return env.Null();
  }
}

Napi::Value FeatureFields::New(Napi::Value layer_obj) {
  Napi::Env env = layer_obj.Env();
  Napi::EscapableHandleScope scope(env);

  FeatureFields *wrapped = new FeatureFields();

  Napi::Value ext = Napi::External::New(env, wrapped);
  Napi::Object obj = Napi::NewInstance(Napi::GetFunction(Napi::New(env, FeatureFields::constructor)), 1, &ext);
  Napi::SetPrivate(obj, Napi::String::New(env, "parent_"), layer_obj);

  return scope.Escape(obj);
}

Napi::Value FeatureFields::toString(const Napi::CallbackInfo &info) {
  return Napi::String::New(env, "FeatureFields");
}

inline bool setField(OGRFeature *f, int field_index, Napi::Value val) {
  if (val.IsNumber()) {
    f->SetField(field_index, val.As<Napi::Number>().Int32Value().ToChecked());
  } else if (val.IsNumber()) {
    f->SetField(field_index, val.As<Napi::Number>().DoubleValue().ToChecked());
  } else if (val.IsString()) {
    std::string str = val.As<Napi::String>().Utf8Value().c_str();
    f->SetField(field_index, str.c_str());
  } else if (val.IsNull() || val.IsNull()) {
    f->UnsetField(field_index);
  } else {
    return true;
  }
  return false;
}

/**
 * Sets feature field(s).
 *
 * @example
 *
 * // most-efficient, least flexible. requires you to know the ordering of the
 * fields: feature.fields.set(['Something']); feature.fields.set(0,
 * 'Something');
 *
 * // most flexible.
 * feature.fields.set({name: 'Something'});
 * feature.fields.set('name', 'Something');
 *
 *
 * @method set
 * @instance
 * @memberof FeatureFields
 * @throws {Error}
 * @param {string|number} key Field name or index
 * @param {any} value
 */

/**
 * @method set
 * @instance
 * @memberof FeatureFields
 * @throws {Error}
 * @param {object} fields
 */
Napi::Value FeatureFields::set(const Napi::CallbackInfo &info) {
  int field_index;
  unsigned int i, n, n_fields_set;

  Napi::Object parent = Napi::GetPrivate(info.This(), Napi::String::New(env, "parent_")).As<Napi::Object>();
  Feature *f = parent.Unwrap<Feature>();
  if (!f->isAlive()) {
    Napi::Error::New(env, "Feature object already destroyed").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (info.Length() == 1) {
    if (info[0].IsArray()) {
      // set([])
      Napi::Array values = info[0].As<Napi::Array>();

      n = f->get()->GetFieldCount();
      if (values->Length() < n) { n = values->Length(); }

      for (i = 0; i < n; i++) {
        Napi::Value val = (values).Get(i);
        if (setField(f->get(), i, val)) {
          Napi::Error::New(env, "Unsupported type of field value").ThrowAsJavaScriptException();
          return env.Null();
        }
      }

      return Napi::Number::New(env, n);
      return;
    } else if (info[0].IsObject()) {
      // set({})
      Napi::Object values = info[0].As<Napi::Object>();

      n = f->get()->GetFieldCount();
      n_fields_set = 0;

      for (i = 0; i < n; i++) {
        // iterate through field names from field defn,
        // grabbing values from passed object, if not undefined

        OGRFieldDefn *field_def = f->get()->GetFieldDefnRef(i);

        const char *field_name = field_def->GetNameRef();

        field_index = f->get()->GetFieldIndex(field_name);

        // skip value if field name doesnt exist
        // both in the feature definition and the passed object
        if (field_index == -1 || !Napi::HasOwnProperty(values, Napi::New(env, field_name)).FromMaybe(false)) {
          continue;
        }

        Napi::Value val = (values).Get(Napi::New(env, field_name));
        if (setField(f->get(), field_index, val)) {
          Napi::Error::New(env, "Unsupported type of field value").ThrowAsJavaScriptException();
          return env.Null();
        }

        n_fields_set++;
      }

      return Napi::Number::New(env, n_fields_set);
      return;
    } else {
      Napi::Error::New(env, "Method expected an object or array").ThrowAsJavaScriptException();
      return env.Null();
    }

  } else if (info.Length() == 2) {
    // set(name|index, value)
    ARG_FIELD_ID(0, f->get(), field_index);

    // set field value
    if (setField(f->get(), field_index, info[1])) {
      Napi::Error::New(env, "Unsupported type of field value").ThrowAsJavaScriptException();
      return env.Null();
    }

    return Napi::Number::New(env, 1);
    return;
  } else {
    Napi::Error::New(env, "Invalid number of arguments").ThrowAsJavaScriptException();
    return env.Null();
  }
}

/**
 * Resets all fields.
 *
 * @example
 *
 * feature.fields.reset();
 *
 * @method reset
 * @instance
 * @memberof FeatureFields
 * @throws {Error}
 * @param {object} [values]
 * @return {void}
 */
Napi::Value FeatureFields::reset(const Napi::CallbackInfo &info) {
  int field_index;
  unsigned int i, n;

  Napi::Object parent = Napi::GetPrivate(info.This(), Napi::String::New(env, "parent_")).As<Napi::Object>();
  Feature *f = parent.Unwrap<Feature>();
  if (!f->isAlive()) {
    Napi::Error::New(env, "Feature object already destroyed").ThrowAsJavaScriptException();
    return env.Null();
  }

  n = f->get()->GetFieldCount();

  if (info.Length() == 0) {
    for (i = 0; i < n; i++) { f->get()->UnsetField(i); }
    return Napi::Number::New(env, n);
    return;
  }

  if (!info[0].IsObject()) {
    Napi::Error::New(env, "fields must be an object").ThrowAsJavaScriptException();
    return env.Null();
  }

  Napi::Object values = info[0].As<Napi::Object>();

  for (i = 0; i < n; i++) {
    // iterate through field names from field defn,
    // grabbing values from passed object

    OGRFieldDefn *field_def = f->get()->GetFieldDefnRef(i);

    const char *field_name = field_def->GetNameRef();

    field_index = f->get()->GetFieldIndex(field_name);
    if (field_index == -1) continue;

    Napi::Value val = (values).Get(Napi::New(env, field_name));
    if (setField(f->get(), field_index, val)) {
      Napi::Error::New(env, "Unsupported type of field value").ThrowAsJavaScriptException();
      return env.Null();
    }
  }

  return Napi::Number::New(env, n);
}

/**
 * Returns the number of fields.
 *
 * @example
 *
 * feature.fields.count();
 *
 * @method count
 * @instance
 * @memberof FeatureFields
 * @return {number}
 */
Napi::Value FeatureFields::count(const Napi::CallbackInfo &info) {

  Napi::Object parent = Napi::GetPrivate(info.This(), Napi::String::New(env, "parent_")).As<Napi::Object>();
  Feature *f = parent.Unwrap<Feature>();
  if (!f->isAlive()) {
    Napi::Error::New(env, "Feature object already destroyed").ThrowAsJavaScriptException();
    return env.Null();
  }

  return Napi::Number::New(env, f->get()->GetFieldCount());
}

/**
 * Returns the index of a field, given its name.
 *
 * @example
 *
 * var index = feature.fields.indexOf('field');
 *
 * @method indexOf
 * @instance
 * @memberof FeatureFields
 * @param {string} name
 * @return {number} Index or, `-1` if it cannot be found.
 */
Napi::Value FeatureFields::indexOf(const Napi::CallbackInfo &info) {

  Napi::Object parent = Napi::GetPrivate(info.This(), Napi::String::New(env, "parent_")).As<Napi::Object>();
  Feature *f = parent.Unwrap<Feature>();
  if (!f->isAlive()) {
    Napi::Error::New(env, "Feature object already destroyed").ThrowAsJavaScriptException();
    return env.Null();
  }

  std::string name("");
  NODE_ARG_STR(0, "field name", name);

  return Napi::Number::New(env, f->get()->GetFieldIndex(name.c_str()));
}

/**
 * Outputs the field data as a pure JS object.
 *
 * @throws {Error}
 * @method toObject
 * @instance
 * @memberof FeatureFields
 * @return {any}
 */
Napi::Value FeatureFields::toObject(const Napi::CallbackInfo &info) {

  Napi::Object parent = Napi::GetPrivate(info.This(), Napi::String::New(env, "parent_")).As<Napi::Object>();
  Feature *f = parent.Unwrap<Feature>();
  if (!f->isAlive()) {
    Napi::Error::New(env, "Feature object already destroyed").ThrowAsJavaScriptException();
    return env.Null();
  }

  Napi::Object obj = Napi::Object::New(env);

  int n = f->get()->GetFieldCount();
  for (int i = 0; i < n; i++) {

    // get field name
    OGRFieldDefn *field_def = f->get()->GetFieldDefnRef(i);
    const char *key = field_def->GetNameRef();
    if (!key) {
      Napi::Error::New(env, "Error getting field name").ThrowAsJavaScriptException();
      return env.Null();
    }

    // get field value
    try {
      Napi::Value val = FeatureFields::get(f->get(), i);
      (obj).Set(Napi::New(env, key), val);
    } catch (const char *err) {
      Napi::Error::New(env, err).ThrowAsJavaScriptException();
      return env.Null();
    }
  }
  return obj;
}

/**
 * Outputs the field values as a pure JS array.
 *
 * @throws {Error}
 * @method toArray
 * @instance
 * @memberof FeatureFields
 * @return {any[]}
 */
Napi::Value FeatureFields::toArray(const Napi::CallbackInfo &info) {

  Napi::Object parent = Napi::GetPrivate(info.This(), Napi::String::New(env, "parent_")).As<Napi::Object>();
  Feature *f = parent.Unwrap<Feature>();
  if (!f->isAlive()) {
    Napi::Error::New(env, "Feature object already destroyed").ThrowAsJavaScriptException();
    return env.Null();
  }

  int n = f->get()->GetFieldCount();
  Napi::Array array = Napi::Array::New(env, n);

  for (int i = 0; i < n; i++) {
    // get field value
    try {
      Napi::Value val = FeatureFields::get(f->get(), i);
      (array).Set(i, val);
    } catch (const char *err) {
      Napi::Error::New(env, err).ThrowAsJavaScriptException();
      return env.Null();
    }
  }
  return array;
}

Napi::Value FeatureFields::get(OGRFeature *f, int field_index) {
  //throws
  Napi::EscapableHandleScope scope(env);

  if (field_index < 0 || field_index >= f->GetFieldCount()) throw "Invalid field";
  if (!f->IsFieldSet(field_index)) return scope.Escape(env.Null());

  OGRFieldDefn *field_def = f->GetFieldDefnRef(field_index);
  switch (field_def->GetType()) {
    case OFTInteger: return scope.Escape(Napi::Number::New(env, f->GetFieldAsInteger(field_index)));
    case OFTInteger64: return scope.Escape(Napi::Number::New(env, f->GetFieldAsInteger64(field_index)));
    case OFTInteger64List: return scope.Escape(getFieldAsInteger64List(f, field_index));
    case OFTReal: return scope.Escape(Napi::Number::New(env, f->GetFieldAsDouble(field_index)));
    case OFTString: return scope.Escape(SafeString::New(f->GetFieldAsString(field_index)));
    case OFTIntegerList: return scope.Escape(getFieldAsIntegerList(f, field_index));
    case OFTRealList: return scope.Escape(getFieldAsDoubleList(f, field_index));
    case OFTStringList: return scope.Escape(getFieldAsStringList(f, field_index));
    case OFTBinary: return scope.Escape(getFieldAsBinary(f, field_index));
    case OFTDate:
    case OFTTime:
    case OFTDateTime: return scope.Escape(getFieldAsDateTime(f, field_index));
    default: throw "Unsupported field type";
  }
}

/**
 * Returns a field's value.
 *
 * @example
 *
 * value = feature.fields.get(0);
 * value = feature.fields.get('field');
 *
 * @method get
 * @instance
 * @memberof FeatureFields
 * @param {string|number} key Feature name or index.
 * @throws {Error}
 * @return {any}
 */
Napi::Value FeatureFields::get(const Napi::CallbackInfo &info) {

  Napi::Object parent = Napi::GetPrivate(info.This(), Napi::String::New(env, "parent_")).As<Napi::Object>();
  Feature *f = parent.Unwrap<Feature>();
  if (!f->isAlive()) {
    Napi::Error::New(env, "Feature object already destroyed").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (info.Length() < 1) {
    Napi::Error::New(env, "Field index or name must be given").ThrowAsJavaScriptException();
    return env.Null();
  }

  int field_index;
  ARG_FIELD_ID(0, f->get(), field_index);

  try {
    Napi::Value result = FeatureFields::get(f->get(), field_index);
    return result;
  } catch (const char *err) { Napi::Error::New(env, err).ThrowAsJavaScriptException(); }
}

/**
 * Returns a list of field name.
 *
 * @method getNames
 * @instance
 * @memberof FeatureFields
 * @throws {Error}
 * @return {string[]} List of field names.
 */
Napi::Value FeatureFields::getNames(const Napi::CallbackInfo &info) {

  Napi::Object parent = Napi::GetPrivate(info.This(), Napi::String::New(env, "parent_")).As<Napi::Object>();
  Feature *f = parent.Unwrap<Feature>();
  if (!f->isAlive()) {
    Napi::Error::New(env, "Feature object already destroyed").ThrowAsJavaScriptException();
    return env.Null();
  }

  int n = f->get()->GetFieldCount();
  Napi::Array result = Napi::Array::New(env, n);

  for (int i = 0; i < n; i++) {

    // get field name
    OGRFieldDefn *field_def = f->get()->GetFieldDefnRef(i);
    const char *field_name = field_def->GetNameRef();
    if (!field_name) {
      Napi::Error::New(env, "Error getting field name").ThrowAsJavaScriptException();
      return env.Null();
    }
    (result).Set(i, Napi::New(env, field_name));
  }

  return result;
}

Napi::Value FeatureFields::getFieldAsIntegerList(OGRFeature *feature, int field_index) {
  Napi::EscapableHandleScope scope(env);

  int count_of_values = 0;

  const int *values = feature->GetFieldAsIntegerList(field_index, &count_of_values);

  Napi::Array return_array = Napi::Array::New(env, count_of_values);

  for (int index = 0; index < count_of_values; index++) {
    (return_array).Set(index, Napi::Number::New(env, values[index]));
  }

  return scope.Escape(return_array);
}

Napi::Value FeatureFields::getFieldAsInteger64List(OGRFeature *feature, int field_index) {
  Napi::EscapableHandleScope scope(env);

  int count_of_values = 0;

  const long long *values = feature->GetFieldAsInteger64List(field_index, &count_of_values);

  Napi::Array return_array = Napi::Array::New(env, count_of_values);

  for (int index = 0; index < count_of_values; index++) {
    (return_array).Set(index, Napi::Number::New(env, values[index]));
  }

  return scope.Escape(return_array);
}

Napi::Value FeatureFields::getFieldAsDoubleList(OGRFeature *feature, int field_index) {
  Napi::EscapableHandleScope scope(env);

  int count_of_values = 0;

  const double *values = feature->GetFieldAsDoubleList(field_index, &count_of_values);

  Napi::Array return_array = Napi::Array::New(env, count_of_values);

  for (int index = 0; index < count_of_values; index++) {
    (return_array).Set(index, Napi::Number::New(env, values[index]));
  }

  return scope.Escape(return_array);
}

Napi::Value FeatureFields::getFieldAsStringList(OGRFeature *feature, int field_index) {
  Napi::EscapableHandleScope scope(env);
  char **values = feature->GetFieldAsStringList(field_index);

  int count_of_values = CSLCount(values);

  Napi::Array return_array = Napi::Array::New(env, count_of_values);

  for (int index = 0; index < count_of_values; index++) { (return_array).Set(index, SafeString::New(values[index])); }

  return scope.Escape(return_array);
}

Napi::Value FeatureFields::getFieldAsBinary(OGRFeature *feature, int field_index) {
  Napi::EscapableHandleScope scope(env);

  int count_of_bytes = 0;

  char *data = (char *)feature->GetFieldAsBinary(field_index, &count_of_bytes);

  if (count_of_bytes > 0) {
    // GDAL Feature->GetFieldAsBinary returns a pointer to an internal buffer
    // that should not be freed
    // The lifetime of this internal buffer does not match the lifetime of
    // the returned buffer
    // So we copy
    return scope.Escape(Napi::Buffer::Copy(env, data, count_of_bytes));
  }

  return scope.Escape(env.Undefined());
}

Napi::Value FeatureFields::getFieldAsDateTime(OGRFeature *feature, int field_index) {
  Napi::EscapableHandleScope scope(env);

  int year, month, day, hour, minute, second, timezone;

  year = month = day = hour = minute = second = timezone = 0;

  int result = feature->GetFieldAsDateTime(field_index, &year, &month, &day, &hour, &minute, &second, &timezone);

  if (result == TRUE) {
    Napi::Object hash = Napi::Object::New(env);

    if (year) { (hash).Set(Napi::String::New(env, "year"), Napi::Number::New(env, year)); }
    if (month) { (hash).Set(Napi::String::New(env, "month"), Napi::Number::New(env, month)); }
    if (day) { (hash).Set(Napi::String::New(env, "day"), Napi::Number::New(env, day)); }
    if (hour) { (hash).Set(Napi::String::New(env, "hour"), Napi::Number::New(env, hour)); }
    if (minute) { (hash).Set(Napi::String::New(env, "minute"), Napi::Number::New(env, minute)); }
    if (second) { (hash).Set(Napi::String::New(env, "second"), Napi::Number::New(env, second)); }
    if (timezone) { (hash).Set(Napi::String::New(env, "timezone"), Napi::Number::New(env, timezone)); }

    return scope.Escape(hash);
  } else {
    return scope.Escape(env.Undefined());
  }
}

/**
 * Returns the parent feature.
 *
 * @readonly
 * @kind member
 * @name feature
 * @instance
 * @memberof FeatureFields
 * @type {Feature}
 */
Napi::Value FeatureFields::featureGetter(const Napi::CallbackInfo &info) {
  return Napi::GetPrivate(info.This(), Napi::String::New(env, "parent_"));
}

} // namespace node_gdal
