
#include "gdal_feature.hpp"
#include "collections/feature_fields.hpp"
#include "gdal_common.hpp"
#include "gdal_feature_defn.hpp"
#include "gdal_field_defn.hpp"
#include "geometry/gdal_geometry.hpp"
#include "gdal_layer.hpp"

namespace node_gdal {

Napi::FunctionReference Feature::constructor;

void Feature::Initialize(Napi::Object target) {
  Napi::HandleScope scope(env);

  Napi::FunctionReference lcons = Napi::Function::New(env, Feature::New);

  lcons->SetClassName(Napi::String::New(env, "Feature"));

  InstanceMethod("toString", &toString),
  InstanceMethod("getGeometry", &getGeometry),
  // InstanceMethod("setGeometryDirectly", &setGeometryDirectly),
  InstanceMethod("setGeometry", &setGeometry),
  // InstanceMethod("stealGeometry", &stealGeometry),
  InstanceMethod("clone", &clone),
  // InstanceMethod("equals", &equals),
  // InstanceMethod("getFieldDefn", &getFieldDefn), (use
  // defn.fields.get() instead)
  InstanceMethod("setFrom", &setFrom),

  // Note: This is used mainly for testing
  // TODO: Give node more info on the amount of memory a feature is using
  //      Napi::AdjustExternalMemory()
  InstanceMethod("destroy", &destroy),

  ATTR(lcons, "fields", fieldsGetter, READ_ONLY_SETTER);
  ATTR(lcons, "defn", defnGetter, READ_ONLY_SETTER);
  ATTR(lcons, "fid", fidGetter, fidSetter);

  (target).Set(Napi::String::New(env, "Feature"), Napi::GetFunction(lcons));

  constructor.Reset(lcons);
}

Feature::Feature(OGRFeature *feature) : Napi::ObjectWrap<Feature>(), this_(feature), owned_(true) {
  LOG("Created Feature[%p]", feature);
}

Feature::Feature() : Napi::ObjectWrap<Feature>(), this_(0), owned_(true) {
}

Feature::~Feature() {
  dispose();
}

void Feature::dispose() {
  if (this_) {
    LOG("Disposing Feature [%p] (%s)", this_, owned_ ? "owned" : "unowned");
    if (owned_) OGRFeature::DestroyFeature(this_);
    LOG("Disposed Feature [%p]", this_);
    this_ = NULL;
  }
}

/**
 * A simple feature, including geometry and attributes. Its fields and geometry
 * type is defined by the given definition.
 *
 * @example
 * //create layer and specify geometry type
 * var layer = dataset.layers.create('mylayer', null, gdal.Point);
 *
 * //setup fields for the given layer
 * layer.fields.add(new gdal.FieldDefn('elevation', gdal.OFTInteger));
 * layer.fields.add(new gdal.FieldDefn('name', gdal.OFTString));
 *
 * //create feature using layer definition and then add it to the layer
 * var feature = new gdal.Feature(layer);
 * feature.fields.set('elevation', 13775);
 * feature.fields.set('name', 'Grand Teton');
 * feature.setGeometry(new gdal.Point(43.741208, -110.802414));
 * layer.features.add(feature);
 *
 * @constructor
 * @class Feature
 * @param {Layer|FeatureDefn} definition
 */
Napi::Value Feature::New(const Napi::CallbackInfo& info) {
  Feature *f;

  if (!info.IsConstructCall()) {
    Napi::Error::New(env, "Cannot call constructor as function, you need to use 'new' keyword").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (info[0].IsExternal()) {
    Napi::External ext = info[0].As<Napi::External>();
    void *ptr = ext->Value();
    f = static_cast<Feature *>(ptr);

  } else {

    if (info.Length() < 1) {
      Napi::Error::New(env, "Constructor expects Layer or FeatureDefn object").ThrowAsJavaScriptException();
      return env.Null();
    }

    OGRFeatureDefn *def;

    if (IS_WRAPPED(info[0], Layer)) {
      Layer *layer = info[0].As<Napi::Object>().Unwrap<Layer>();
      if (!layer->isAlive()) {
        Napi::Error::New(env, "Layer object already destroyed").ThrowAsJavaScriptException();
        return env.Null();
      }
      def = layer->get()->GetLayerDefn();
    } else if (IS_WRAPPED(info[0], FeatureDefn)) {
      FeatureDefn *feature_def = info[0].As<Napi::Object>().Unwrap<FeatureDefn>();
      if (!feature_def->isAlive()) {
        Napi::Error::New(env, "FeatureDefn object already destroyed").ThrowAsJavaScriptException();
        return env.Null();
      }
      def = feature_def->get();
    } else {
      Napi::Error::New(env, "Constructor expects Layer or FeatureDefn object").ThrowAsJavaScriptException();
      return env.Null();
    }

    OGRFeature *ogr_f = new OGRFeature(def);
    f = new Feature(ogr_f);
  }

  Napi::Value fields = FeatureFields::New(info.This());
  Napi::SetPrivate(info.This(), Napi::String::New(env, "fields_"), fields);

  f->Wrap(info.This());
  return info.This();
}

Napi::Value Feature::New(OGRFeature *feature) {
  Napi::EscapableHandleScope scope(env);
  return scope.Escape(Feature::New(feature, true));
}

Napi::Value Feature::New(OGRFeature *feature, bool owned) {
  Napi::EscapableHandleScope scope(env);

  if (!feature) { return scope.Escape(env.Null()); }

  Feature *wrapped = new Feature(feature);
  wrapped->owned_ = owned;
  Napi::Value ext = Napi::External::New(env, wrapped);
  Napi::Object obj =
    Napi::NewInstance(Napi::GetFunction(Napi::New(env, Feature::constructor)), 1, &ext);
  return scope.Escape(obj);
}

Napi::Value Feature::toString(const Napi::CallbackInfo& info) {
  return Napi::String::New(env, "Feature");
}

/**
 * Returns the geometry of the feature.
 *
 * @method getGeometry
 * @instance
 * @memberof Feature
 * @return {Geometry}
 */
Napi::Value Feature::getGeometry(const Napi::CallbackInfo& info) {

  Feature *feature = info.This().Unwrap<Feature>();
  if (!feature->isAlive()) {
    Napi::Error::New(env, "Feature object already destroyed").ThrowAsJavaScriptException();
    return env.Null();
  }

  OGRGeometry *geom = feature->this_->GetGeometryRef();
  if (!geom) {
    return env.Null();
    return;
  }

  return Geometry::New(geom, false);
}

#if 0
/*
 * Returns the definition of a particular field at an index.
 *
 * _method getFieldDefn
 * _param {number} index Field index (0-based)
 * _return {FieldDefn}
 */
Napi::Value Feature::getFieldDefn(const Napi::CallbackInfo& info) {
  int field_index;
  NODE_ARG_INT(0, "field index", field_index);

  Feature *feature = info.This().Unwrap<Feature>();
  if (!feature->isAlive()) {
    Napi::Error::New(env, "Feature object already destroyed").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (field_index < 0 || field_index >= feature->this_->GetFieldCount()) {
    Napi::RangeError::New(env, "Invalid field index").ThrowAsJavaScriptException();
    return env.Null();
  }

  return FieldDefn::New(feature->this_->GetFieldDefnRef(field_index), false);
}
#endif

// NODE_WRAPPED_METHOD_WITH_RESULT(Feature, stealGeometry, Geometry,
// StealGeometry);

/**
 * Sets the feature's geometry.
 *
 * @throws {Error}
 * @method setGeometry
 * @instance
 * @memberof Feature
 * @param {Geometry|null} geometry new geometry or null to clear the field
 */
Napi::Value Feature::setGeometry(const Napi::CallbackInfo& info) {

  Geometry *geom = NULL;
  NODE_ARG_WRAPPED_OPT(0, "geometry", Geometry, geom);

  Feature *feature = info.This().Unwrap<Feature>();
  if (!feature->isAlive()) {
    Napi::Error::New(env, "Feature object already destroyed").ThrowAsJavaScriptException();
    return env.Null();
  }

  OGRErr err = feature->this_->SetGeometry(geom ? geom->get() : NULL);
  if (err) { NODE_THROW_OGRERR(err); }

  return;
}

/**
 * Determines if the features are the same.
 *
 * @method equals
 * @instance
 * @memberof Feature
 * @param {Feature} feature
 * @return {boolean} `true` if the features are the same, `false` if different
 */
NODE_WRAPPED_METHOD_WITH_RESULT_1_WRAPPED_PARAM(Feature, equals, Boolean, Equal, Feature, "feature");

/**
 * Clones the feature.
 *
 * @method clone
 * @instance
 * @memberof Feature
 * @return {Feature}
 */
Napi::Value Feature::clone(const Napi::CallbackInfo& info) {
  Feature *feature = info.This().Unwrap<Feature>();
  if (!feature->isAlive()) {
    Napi::Error::New(env, "Feature object already destroyed").ThrowAsJavaScriptException();
    return env.Null();
  }
  return Feature::New(feature->this_->Clone());
}

/**
 * Releases the feature from memory.
 *
 * @method destroy
 * @instance
 * @memberof Feature
 */
Napi::Value Feature::destroy(const Napi::CallbackInfo& info) {
  Feature *feature = info.This().Unwrap<Feature>();
  if (!feature->isAlive()) {
    Napi::Error::New(env, "Feature object already destroyed").ThrowAsJavaScriptException();
    return env.Null();
  }
  feature->dispose();
  return;
}

/**
 * Set one feature from another. Overwrites the contents of this feature
 * from the geometry and attributes of another.
 *
 * @example
 *
 * var feature1 = new gdal.Feature(defn);
 * var feature2 = new gdal.Feature(defn);
 * feature1.setGeometry(new gdal.Point(5, 10));
 * feature1.fields.set([5, 'test', 3.14]);
 * feature2.setFrom(feature1);
 *
 * @throws {Error}
 * @method setFrom
 * @instance
 * @memberof Feature
 * @param {Feature} feature
 * @param {number[]} [index_map] Array mapping each field from the source feature
 * to the given index in the destination feature. -1 ignores the source field.
 * The field types must still match otherwise the behavior is undefined.
 * @param {boolean} [forgiving=true] `true` if the operation should continue
 * despite lacking output fields matching some of the source fields.
 */
Napi::Value Feature::setFrom(const Napi::CallbackInfo& info) {
  Feature *other_feature;
  int forgiving = 1;
  Napi::Array index_map;
  OGRErr err = 0;

  NODE_ARG_WRAPPED(0, "feature", Feature, other_feature);

  Feature *feature = info.This().Unwrap<Feature>();
  if (!feature->isAlive()) {
    Napi::Error::New(env, "Feature object already destroyed").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (!info[1].IsArray()) {
    NODE_ARG_BOOL_OPT(1, "forgiving", forgiving);

    err = feature->this_->SetFrom(other_feature->this_, forgiving ? TRUE : FALSE);
  } else {
    NODE_ARG_ARRAY(1, "index map", index_map);
    NODE_ARG_BOOL_OPT(2, "forgiving", forgiving);

    if (index_map->Length() < 1) {
      Napi::Error::New(env, "index map must contain at least 1 index").ThrowAsJavaScriptException();
      return env.Null();
    }

    int *index_map_ptr = new int[index_map->Length()];

    for (unsigned index = 0; index < index_map->Length(); index++) {
      Napi::Value field_index((index_map).Get(Napi::Number::New(env, index)));

      if (!field_index.IsNumber()) {
        delete[] index_map_ptr;
        Napi::Error::New(env, "index map must contain only integer values").ThrowAsJavaScriptException();
        return env.Null();
      }

      int val = (int)field_index.As<Napi::Number>().Int32Value().ToChecked(); // todo: validate index? perhaps ogr already
                                                                // does this and throws an error

      index_map_ptr[index] = val;
    }

    err = feature->this_->SetFrom(other_feature->this_, index_map_ptr, forgiving ? TRUE : FALSE);

    delete[] index_map_ptr;
  }

  if (err) {
    NODE_THROW_OGRERR(err);
    return;
  }
  return;
}

/**
 * @readonly
 * @kind member
 * @name fields
 * @instance
 * @memberof Feature
 * @type {FeatureFields}
 */
Napi::Value Feature::fieldsGetter(const Napi::CallbackInfo& info) {
  return Napi::GetPrivate(info.This(), Napi::String::New(env, "fields_"));
}

/**
 * @kind member
 * @name fid
 * @instance
 * @memberof Feature
 * @type {number}
 */
Napi::Value Feature::fidGetter(const Napi::CallbackInfo& info) {
  Feature *feature = info.This().Unwrap<Feature>();
  if (!feature->isAlive()) {
    Napi::Error::New(env, "Feature object already destroyed").ThrowAsJavaScriptException();
    return env.Null();
  }
  return Napi::Number::New(env, feature->this_->GetFID());
}

/**
 * @readonly
 * @kind member
 * @name defn
 * @instance
 * @memberof Feature
 * @type {FeatureDefn}
 */
Napi::Value Feature::defnGetter(const Napi::CallbackInfo& info) {
  Feature *feature = info.This().Unwrap<Feature>();
  if (!feature->isAlive()) {
    Napi::Error::New(env, "Feature object already destroyed").ThrowAsJavaScriptException();
    return env.Null();
  }
  return FeatureDefn::New(feature->this_->GetDefnRef(), false);
}

void Feature::fidSetter(const Napi::CallbackInfo& info, const Napi::Value& value) {
  Feature *feature = info.This().Unwrap<Feature>();
  if (!feature->isAlive()) {
    Napi::Error::New(env, "Feature object already destroyed").ThrowAsJavaScriptException();
    return env.Null();
  }
  if (!value.IsNumber()) {
    Napi::Error::New(env, "fid must be an integer").ThrowAsJavaScriptException();
    return env.Null();
  }
  feature->this_->SetFID(value.As<Napi::Number>().Int64Value().ToChecked());
}

} // namespace node_gdal
