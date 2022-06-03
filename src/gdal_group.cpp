#include "gdal_group.hpp"
#include "gdal_common.hpp"
#include "gdal_driver.hpp"
#include "collections/group_groups.hpp"
#include "collections/group_arrays.hpp"
#include "collections/group_dimensions.hpp"
#include "collections/group_attributes.hpp"
#include "geometry/gdal_geometry.hpp"
#include "gdal_layer.hpp"
#include "gdal_majorobject.hpp"
#include "gdal_rasterband.hpp"
#include "gdal_spatial_reference.hpp"

namespace node_gdal {

#if GDAL_VERSION_MAJOR > 3 || (GDAL_VERSION_MAJOR == 3 && GDAL_VERSION_MINOR >= 1)

Napi::FunctionReference Group::constructor;

void Group::Initialize(Napi::Object target) {
  Napi::HandleScope scope(env);

  Napi::FunctionReference lcons = Napi::Function::New(env, Group::New);

  lcons->SetClassName(Napi::String::New(env, "Group"));

  InstanceMethod("toString", &toString),

  ATTR_DONT_ENUM(lcons, "_uid", uidGetter, READ_ONLY_SETTER);
  ATTR(lcons, "description", descriptionGetter, READ_ONLY_SETTER);
  ATTR(lcons, "groups", groupsGetter, READ_ONLY_SETTER);
  ATTR(lcons, "arrays", arraysGetter, READ_ONLY_SETTER);
  ATTR(lcons, "dimensions", dimensionsGetter, READ_ONLY_SETTER);
  ATTR(lcons, "attributes", attributesGetter, READ_ONLY_SETTER);

  (target).Set(Napi::String::New(env, "Group"), Napi::GetFunction(lcons));

  constructor.Reset(lcons);
}

Group::Group(std::shared_ptr<GDALGroup> group) : Napi::ObjectWrap<Group>(), uid(0), this_(group), parent_ds(0) {
  LOG("Created group [%p]", group.get());
}

Group::Group() : Napi::ObjectWrap<Group>(), uid(0), this_(0), parent_ds(0) {
}

Group::~Group() {
  dispose();
}

void Group::dispose() {
  if (this_) {

    LOG("Disposing group [%p]", this_.get());

    object_store.dispose(uid);

    LOG("Disposed group [%p]", this_.get());
  }
};

/**
 * A representation of a group with access methods.
 *
 * @class Group
 */
Napi::Value Group::New(const Napi::CallbackInfo& info) {

  if (!info.IsConstructCall()) {
    Napi::Error::New(env, "Cannot call constructor as function, you need to use 'new' keyword").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (info.Length() > 1 && info[0].IsExternal() && info[1].IsObject()) {
    Napi::External ext = info[0].As<Napi::External>();
    void *ptr = ext->Value();
    Group *f = static_cast<Group *>(ptr);
    f->Wrap(info.This());

    Napi::Value groups = GroupGroups::New(info.This(), info[1]);
    Napi::SetPrivate(info.This(), Napi::String::New(env, "groups_"), groups);
    Napi::Value arrays = GroupArrays::New(info.This(), info[1]);
    Napi::SetPrivate(info.This(), Napi::String::New(env, "arrays_"), arrays);
    Napi::Value dims = GroupDimensions::New(info.This(), info[1]);
    Napi::SetPrivate(info.This(), Napi::String::New(env, "dims_"), dims);
    Napi::Value attrs = GroupAttributes::New(info.This(), info[1]);
    Napi::SetPrivate(info.This(), Napi::String::New(env, "attrs_"), attrs);

    return info.This();
    return;
  } else {
    Napi::Error::New(env, "Cannot create group directly. Create with dataset instead.").ThrowAsJavaScriptException();
    return env.Null();
  }

  return info.This();
}

Napi::Value Group::New(std::shared_ptr<GDALGroup> raw, GDALDataset *parent_ds) {
  Napi::EscapableHandleScope scope(env);

  if (object_store.has(parent_ds)) {
    Napi::Object ds = object_store.get(parent_ds);
    return Group::New(raw, ds);
  } else {
    LOG("Group's parent dataset disappeared from cache (group = %p, dataset = %p)", raw.get(), parent_ds);
    Napi::Error::New(env, "Group's parent dataset disappeared from cache").ThrowAsJavaScriptException();

    return scope.Escape(env.Undefined());
  }
}

Napi::Value Group::New(std::shared_ptr<GDALGroup> raw, Napi::Object parent_ds) {
  Napi::EscapableHandleScope scope(env);

  if (!raw) { return scope.Escape(env.Null()); }
  if (object_store.has(raw)) { return scope.Escape(object_store.get(raw)); }

  Group *wrapped = new Group(raw);

  long parent_group_uid = 0;
  Napi::Object parent;

  Napi::Value ext = Napi::External::New(env, wrapped);
  Napi::Value argv[] = {ext, parent_ds};
  Napi::Object obj =
    Napi::NewInstance(Napi::GetFunction(Napi::New(env, Group::constructor)), 2, argv);

  Dataset *unwrapped_ds = parent_ds.Unwrap<Dataset>();
  long parent_uid = unwrapped_ds->uid;

  wrapped->uid = object_store.add(raw, wrapped->persistent(), parent_uid);
  wrapped->parent_ds = unwrapped_ds->get();
  wrapped->parent_uid = parent_uid;
  Napi::SetPrivate(obj, Napi::String::New(env, "ds_"), parent_ds);
  if (parent_group_uid != 0) Napi::SetPrivate(obj, Napi::String::New(env, "parent_"), parent);

  return scope.Escape(obj);
}

Napi::Value Group::toString(const Napi::CallbackInfo& info) {
  return Napi::String::New(env, "Group");
}

/**
 * @readonly
 * @kind member
 * @name description
 * @instance
 * @memberof Group
 * @type {string}
 */
NODE_WRAPPED_GETTER_WITH_STRING_LOCKED(Group, descriptionGetter, GetFullName);

/**
 * @readonly
 * @kind member
 * @name groups
 * @instance
 * @memberof Group
 * @type {GroupGroups}
 */
Napi::Value Group::groupsGetter(const Napi::CallbackInfo& info) {
  return Napi::GetPrivate(info.This(), Napi::String::New(env, "groups_"));
}

/**
 * @readonly
 * @kind member
 * @name arrays
 * @instance
 * @memberof Group
 * @type {GroupArrays}
 */
Napi::Value Group::arraysGetter(const Napi::CallbackInfo& info) {
  return Napi::GetPrivate(info.This(), Napi::String::New(env, "arrays_"));
}

/**
 * @readonly
 * @kind member
 * @name dimensions
 * @instance
 * @memberof Group
 * @type {GroupDimensions}
 */
Napi::Value Group::dimensionsGetter(const Napi::CallbackInfo& info) {
  return Napi::GetPrivate(info.This(), Napi::String::New(env, "dims_"));
}

/**
 * @readonly
 * @kind member
 * @name attributes
 * @instance
 * @memberof Group
 * @type {GroupAttributes}
 */
Napi::Value Group::attributesGetter(const Napi::CallbackInfo& info) {
  return Napi::GetPrivate(info.This(), Napi::String::New(env, "attrs_"));
}

Napi::Value Group::uidGetter(const Napi::CallbackInfo& info) {
  Group *group = info.This().Unwrap<Group>();
  return Napi::New(env, (int)group->uid);
}

#endif

} // namespace node_gdal
