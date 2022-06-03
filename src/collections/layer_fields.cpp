#include "layer_fields.hpp"
#include "../gdal_common.hpp"
#include "../gdal_field_defn.hpp"
#include "../gdal_layer.hpp"

namespace node_gdal {

Napi::FunctionReference LayerFields::constructor;

void LayerFields::Initialize(Napi::Object target) {
  Napi::HandleScope scope(env);

  Napi::FunctionReference lcons = Napi::Function::New(env, LayerFields::New);

  lcons->SetClassName(Napi::String::New(env, "LayerFields"));

  InstanceMethod("toString", &toString),
  InstanceMethod("count", &count),
  InstanceMethod("get", &get),
  InstanceMethod("remove", &remove),
  InstanceMethod("getNames", &getNames),
  InstanceMethod("indexOf", &indexOf),
  InstanceMethod("reorder", &reorder),
  InstanceMethod("add", &add),
  // InstanceMethod("alter", &alter),

  ATTR_DONT_ENUM(lcons, "layer", layerGetter, READ_ONLY_SETTER);

  (target).Set(Napi::String::New(env, "LayerFields"), Napi::GetFunction(lcons));

  constructor.Reset(lcons);
}

LayerFields::LayerFields() : Napi::ObjectWrap<LayerFields>(){
}

LayerFields::~LayerFields() {
}

/**
 * @class LayerFields
 */
Napi::Value LayerFields::New(const Napi::CallbackInfo& info) {

  if (!info.IsConstructCall()) {
    Napi::Error::New(env, "Cannot call constructor as function, you need to use 'new' keyword").ThrowAsJavaScriptException();
    return env.Null();
  }
  if (info[0].IsExternal()) {
    Napi::External ext = info[0].As<Napi::External>();
    void *ptr = ext->Value();
    LayerFields *layer = static_cast<LayerFields *>(ptr);
    layer->Wrap(info.This());
    return info.This();
    return;
  } else {
    Napi::Error::New(env, "Cannot create LayerFields directly").ThrowAsJavaScriptException();
    return env.Null();
  }
}

Napi::Value LayerFields::New(Napi::Value layer_obj) {
  Napi::Env env = layer_obj.Env();
  Napi::EscapableHandleScope scope(env);

  LayerFields *wrapped = new LayerFields();

  Napi::Value ext = Napi::External::New(env, wrapped);
  Napi::Object obj =
    Napi::NewInstance(Napi::GetFunction(Napi::New(env, LayerFields::constructor)), 1, &ext);
  Napi::SetPrivate(obj, Napi::String::New(env, "parent_"), layer_obj);

  return scope.Escape(obj);
}

Napi::Value LayerFields::toString(const Napi::CallbackInfo& info) {
  return Napi::String::New(env, "LayerFields");
}

/**
 * Returns the number of fields.
 *
 * @method count
 * @instance
 * @memberof LayerFields
 * @return {number}
 */
Napi::Value LayerFields::count(const Napi::CallbackInfo& info) {

  Napi::Object parent =
    Napi::GetPrivate(info.This(), Napi::String::New(env, "parent_")).As<Napi::Object>();
  Layer *layer = parent.Unwrap<Layer>();
  if (!layer->isAlive()) {
    Napi::Error::New(env, "Layer object already destroyed").ThrowAsJavaScriptException();
    return env.Null();
  }

  OGRFeatureDefn *def = layer->get()->GetLayerDefn();
  if (!def) {
    Napi::Error::New(env, "Layer has no layer definition set").ThrowAsJavaScriptException();
    return env.Null();
  }

  return Napi::Number::New(env, def->GetFieldCount());
}

/**
 * Find the index of field in the layer.
 *
 * @method indexOf
 * @instance
 * @memberof LayerFields
 * @param {string} field
 * @return {number} Field index, or -1 if the field doesn't exist
 */
Napi::Value LayerFields::indexOf(const Napi::CallbackInfo& info) {

  Napi::Object parent =
    Napi::GetPrivate(info.This(), Napi::String::New(env, "parent_")).As<Napi::Object>();
  Layer *layer = parent.Unwrap<Layer>();
  if (!layer->isAlive()) {
    Napi::Error::New(env, "Layer object already destroyed").ThrowAsJavaScriptException();
    return env.Null();
  }

  OGRFeatureDefn *def = layer->get()->GetLayerDefn();
  if (!def) {
    Napi::Error::New(env, "Layer has no layer definition set").ThrowAsJavaScriptException();
    return env.Null();
  }

  std::string name("");
  NODE_ARG_STR(0, "field name", name);

  return Napi::Number::New(env, def->GetFieldIndex(name.c_str()));
}

/**
 * Returns a field definition.
 *
 * @throws {Error}
 * @method get
 * @instance
 * @memberof LayerFields
 * @param {string|number} field Field name or index (0-based)
 * @return {FieldDefn}
 */
Napi::Value LayerFields::get(const Napi::CallbackInfo& info) {

  Napi::Object parent =
    Napi::GetPrivate(info.This(), Napi::String::New(env, "parent_")).As<Napi::Object>();
  Layer *layer = parent.Unwrap<Layer>();
  if (!layer->isAlive()) {
    Napi::Error::New(env, "Layer object already destroyed").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (info.Length() < 1) {
    Napi::Error::New(env, "Field index or name must be given").ThrowAsJavaScriptException();
    return env.Null();
  }

  OGRFeatureDefn *def = layer->get()->GetLayerDefn();
  if (!def) {
    Napi::Error::New(env, "Layer has no layer definition set").ThrowAsJavaScriptException();
    return env.Null();
  }

  int field_index;
  ARG_FIELD_ID(0, def, field_index);

  auto r = def->GetFieldDefn(field_index);
  if (r == nullptr) {
    NODE_THROW_LAST_CPLERR;
    return;
  }
  return FieldDefn::New(r);
}

/**
 * Returns a list of field names.
 *
 * @throws {Error}
 * @method getNames
 * @instance
 * @memberof LayerFields
 * @return {string[]} List of strings.
 */
Napi::Value LayerFields::getNames(const Napi::CallbackInfo& info) {

  Napi::Object parent =
    Napi::GetPrivate(info.This(), Napi::String::New(env, "parent_")).As<Napi::Object>();
  Layer *layer = parent.Unwrap<Layer>();
  if (!layer->isAlive()) {
    Napi::Error::New(env, "Layer object already destroyed").ThrowAsJavaScriptException();
    return env.Null();
  }

  OGRFeatureDefn *def = layer->get()->GetLayerDefn();
  if (!def) {
    Napi::Error::New(env, "Layer has no layer definition set").ThrowAsJavaScriptException();
    return env.Null();
  }

  int n = def->GetFieldCount();
  Napi::Array result = Napi::Array::New(env, n);

  for (int i = 0; i < n; i++) {
    OGRFieldDefn *field_def = def->GetFieldDefn(i);
    (result).Set(i, SafeString::New(field_def->GetNameRef()));
  }

  return result;
}

/**
 * Removes a field.
 *
 * @throws {Error}
 * @method remove
 * @instance
 * @memberof LayerFields
 * @param {string|number} field Field name or index (0-based)
 */
Napi::Value LayerFields::remove(const Napi::CallbackInfo& info) {

  Napi::Object parent =
    Napi::GetPrivate(info.This(), Napi::String::New(env, "parent_")).As<Napi::Object>();
  Layer *layer = parent.Unwrap<Layer>();
  if (!layer->isAlive()) {
    Napi::Error::New(env, "Layer object already destroyed").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (info.Length() < 1) {
    Napi::Error::New(env, "Field index or name must be given").ThrowAsJavaScriptException();
    return env.Null();
  }

  OGRFeatureDefn *def = layer->get()->GetLayerDefn();
  if (!def) {
    Napi::Error::New(env, "Layer has no layer definition set").ThrowAsJavaScriptException();
    return env.Null();
  }

  int field_index;
  ARG_FIELD_ID(0, def, field_index);

  int err = layer->get()->DeleteField(field_index);
  if (err) {
    NODE_THROW_OGRERR(err);
    return;
  }

  return;
}

/**
 * Adds field(s).
 *
 * @throws {Error}
 * @method add
 * @instance
 * @memberof LayerFields
 * @param {FieldDefn|FieldDefn[]} defs A field definition, or array of field
 * definitions.
 * @param {boolean} [approx=true]
 */
Napi::Value LayerFields::add(const Napi::CallbackInfo& info) {

  Napi::Object parent =
    Napi::GetPrivate(info.This(), Napi::String::New(env, "parent_")).As<Napi::Object>();
  Layer *layer = parent.Unwrap<Layer>();
  if (!layer->isAlive()) {
    Napi::Error::New(env, "Layer object already destroyed").ThrowAsJavaScriptException();
    return env.Null();
  }
  if (info.Length() < 1) {
    Napi::Error::New(env, "field definition(s) must be given").ThrowAsJavaScriptException();
    return env.Null();
  }

  FieldDefn *field_def;
  int err;
  int approx = 1;
  NODE_ARG_BOOL_OPT(1, "approx", approx);

  if (info[0].IsArray()) {
    Napi::Array array = info[0].As<Napi::Array>();
    int n = array->Length();
    for (int i = 0; i < n; i++) {
      Napi::Value element = (array).Get(i);
      if (IS_WRAPPED(element, FieldDefn)) {
        field_def = element.As<Napi::Object>().Unwrap<FieldDefn>();
        err = layer->get()->CreateField(field_def->get(), approx);
        if (err) {
          NODE_THROW_OGRERR(err);
          return;
        }
      } else {
        Napi::Error::New(env, "All array elements must be FieldDefn objects").ThrowAsJavaScriptException();
        return env.Null();
      }
    }
  } else if (IS_WRAPPED(info[0], FieldDefn)) {
    field_def = info[0].As<Napi::Object>().Unwrap<FieldDefn>();
    err = layer->get()->CreateField(field_def->get(), approx);
    if (err) {
      NODE_THROW_OGRERR(err);
      return;
    }
  } else {
    Napi::Error::New(env, "field definition(s) must be a FieldDefn object or array of FieldDefn objects").ThrowAsJavaScriptException();
    return env.Null();
  }

  return;
}

/**
 * Reorders fields.
 *
 * @example
 *
 * // reverse field order
 * layer.fields.reorder([2,1,0]);
 *
 * @throws {Error}
 * @method reorder
 * @instance
 * @memberof LayerFields
 * @param {number[]} map An array of new indexes (integers)
 */
Napi::Value LayerFields::reorder(const Napi::CallbackInfo& info) {

  Napi::Object parent =
    Napi::GetPrivate(info.This(), Napi::String::New(env, "parent_")).As<Napi::Object>();
  Layer *layer = parent.Unwrap<Layer>();
  if (!layer->isAlive()) {
    Napi::Error::New(env, "Layer object already destroyed").ThrowAsJavaScriptException();
    return env.Null();
  }

  OGRFeatureDefn *def = layer->get()->GetLayerDefn();
  if (!def) {
    Napi::Error::New(env, "Layer has no layer definition set").ThrowAsJavaScriptException();
    return env.Null();
  }

  Napi::Array field_map = Napi::Array::New(env, 0);
  NODE_ARG_ARRAY(0, "field map", field_map);

  int n = def->GetFieldCount();
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

  err = layer->get()->ReorderFields(field_map_array);

  delete[] field_map_array;

  if (err) {
    NODE_THROW_OGRERR(err);
    return;
  }
  return;
}

/**
 * Returns the parent layer.
 *
 * @readonly
 * @kind member
 * @name layer
 * @instance
 * @memberof LayerFields
 * @type {Layer}
 */
Napi::Value LayerFields::layerGetter(const Napi::CallbackInfo& info) {
  return Napi::GetPrivate(info.This(), Napi::String::New(env, "parent_"));
}

} // namespace node_gdal
