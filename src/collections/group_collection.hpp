#ifndef __NODE_GDAL_BASEGROUP_COLLECTION_H__
#define __NODE_GDAL_BASEGROUP_COLLECTION_H__

// node
#include <napi.h>
#include <uv.h>
#include <node_object_wrap.h>

// nan
#include <napi.h>

// gdal
#include <gdal_priv.h>

#include "../async.hpp"
#include "../gdal_dataset.hpp"
#include "../gdal_group.hpp"

#if GDAL_VERSION_MAJOR > 3 || (GDAL_VERSION_MAJOR == 3 && GDAL_VERSION_MINOR >= 1)

using namespace Napi;
using namespace Napi;

namespace node_gdal {

template <typename SELF, typename GDALOBJ, typename GDALPARENT, typename NODEOBJ, typename NODEPARENT>
class GroupCollection : public Napi::ObjectWrap<GroupCollection> {
    public:
  static constexpr const char *_className = "GroupCollection<abstract>";
  static void Initialize(Napi::Object target) {
    Napi::HandleScope scope(env);

    Napi::FunctionReference lcons = Napi::Function::New(env, SELF::New);

    lcons->SetClassName(Napi::New(env, SELF::_className));

    InstanceMethod("toString", &toString), Nan__SetPrototypeAsyncableMethod(lcons, "count", count);
    Nan__SetPrototypeAsyncableMethod(lcons, "get", get);

    ATTR_DONT_ENUM(lcons, "ds", dsGetter, READ_ONLY_SETTER);
    ATTR_DONT_ENUM(lcons, "parent", parentGetter, READ_ONLY_SETTER);
    ATTR(lcons, "names", namesGetter, READ_ONLY_SETTER);

    (target).Set(Napi::New(env, SELF::_className), Napi::GetFunction(lcons));

    SELF::constructor.Reset(lcons);
  }

  static Napi::Value toString(const Napi::CallbackInfo &info) {
    return Napi::New(env, SELF::_className);
  }

  static Napi::Value New(const Napi::CallbackInfo &info) {

    if (!info.IsConstructCall()) {
      Napi::Error::New(env, "Cannot call constructor as function, you need to use 'new' keyword")
        .ThrowAsJavaScriptException();
      return env.Null();
    }
    if (info[0].IsExternal()) {
      Napi::External ext = info[0].As<Napi::External>();
      void *ptr = ext->Value();
      SELF *f = static_cast<SELF *>(ptr);
      f->Wrap(info.This());
      return info.This();
      return;
    } else {
      Napi::Error::New(env, "Cannot create GroupCollection directly").ThrowAsJavaScriptException();
      return env.Null();
    }
  }

  static Napi::Value New(Napi::Value parent, Napi::Value parent_ds) {
    Napi::Env env = parent.Env();
    Napi::EscapableHandleScope scope(env);

    SELF *wrapped = new SELF();

    Napi::Value ext = Napi::External::New(env, wrapped);
    Napi::Object obj = Napi::NewInstance(Napi::GetFunction(Napi::New(env, SELF::constructor)), 1, &ext);
    Napi::SetPrivate(obj, Napi::String::New(env, "parent_ds_"), parent_ds);
    Napi::SetPrivate(obj, Napi::String::New(env, "parent_"), parent);

    return scope.Escape(obj);
  }

  static std::shared_ptr<GDALOBJ> __get(std::shared_ptr<GDALPARENT> parent, std::string const &name) {
    return nullptr;
  };
  static std::shared_ptr<GDALOBJ> __get(std::shared_ptr<GDALPARENT> parent, size_t idx) {
    return nullptr;
  };
  static std::vector<std::string> __getNames(std::shared_ptr<GDALPARENT> parent) {
    return {};
  };
  static int __count(std::shared_ptr<GDALPARENT> parent) {
    return 0;
  };

  GDAL_ASYNCABLE_TEMPLATE(get) {

    Napi::Object parent_ds = Napi::GetPrivate(info.This(), Napi::String::New(env, "parent_ds_")).As<Napi::Object>();
    Napi::Object parent_obj = Napi::GetPrivate(info.This(), Napi::String::New(env, "parent_")).As<Napi::Object>();
    NODE_UNWRAP_CHECK(Dataset, parent_ds, ds);
    NODE_UNWRAP_CHECK(NODEPARENT, parent_obj, parent);

    std::shared_ptr<GDALPARENT> raw = parent->get();
    GDALDataset *gdal_ds = ds->get();
    std::string name = "";
    size_t idx = 0;
    NODE_ARG_STR_INT(0, "id", name, idx, isString);

    GDALAsyncableJob<std::shared_ptr<GDALOBJ>> job(ds->uid);
    job.persist(parent_obj);
    job.main = [raw, name, idx, isString](const GDALExecutionProgress &) {
      std::shared_ptr<GDALOBJ> r = nullptr;
      if (!isString)
        r = SELF::__get(raw, idx);
      else
        r = SELF::__get(raw, name);
      if (r == nullptr) throw "Invalid element";
      return r;
    };
    job.rval = [gdal_ds](std::shared_ptr<GDALOBJ> r, const GetFromPersistentFunc &) {
      return NODEOBJ::New(r, gdal_ds);
    };
    job.run(info, async, 1);
  }

  GDAL_ASYNCABLE_TEMPLATE(count) {

    Napi::Object parent_ds = Napi::GetPrivate(info.This(), Napi::String::New(env, "parent_ds_")).As<Napi::Object>();
    Napi::Object parent_obj = Napi::GetPrivate(info.This(), Napi::String::New(env, "parent_")).As<Napi::Object>();
    NODE_UNWRAP_CHECK(Dataset, parent_ds, ds);
    NODE_UNWRAP_CHECK(NODEPARENT, parent_obj, parent);

    std::shared_ptr<GDALPARENT> raw = parent->get();

    GDALAsyncableJob<int> job(ds->uid);
    job.persist(parent_obj);
    job.main = [raw](const GDALExecutionProgress &) {
      int r = SELF::__count(raw);
      return r;
    };
    job.rval = [](int r, const GetFromPersistentFunc &) { return Napi::Number::New(env, r); };
    job.run(info, async, 0);
  }

  Napi::Value namesGetter(const Napi::CallbackInfo &info) {

    Napi::Object parent_ds = Napi::GetPrivate(info.This(), Napi::String::New(env, "parent_ds_")).As<Napi::Object>();
    Dataset *ds = parent_ds.Unwrap<Dataset>();

    Napi::Object parent_obj = Napi::GetPrivate(info.This(), Napi::String::New(env, "parent_")).As<Napi::Object>();
    NODEPARENT *parent = parent_obj.Unwrap<NODEPARENT>();

    if (!ds->isAlive()) {
      Napi::Error::New(env, "Dataset object has already been destroyed").ThrowAsJavaScriptException();
      return env.Null();
    }

    std::vector<std::string> names = SELF::__getNames(parent->get());

    Napi::Array results = Napi::Array::New(env, 0);
    int i = 0;
    for (std::string &n : names) { (results).Set(i++, SafeString::New(n.c_str())); }

    return results;
  }

  Napi::Value parentGetter(const Napi::CallbackInfo &info) {
    return Napi::GetPrivate(info.This(), Napi::String::New(env, "parent_"));
  }

  Napi::Value dsGetter(const Napi::CallbackInfo &info) {
    return Napi::GetPrivate(info.This(), Napi::String::New(env, "parent_ds_"));
  }

  GroupCollection() : Napi::ObjectWrap<GroupCollection>() {
  }

    protected:
  ~GroupCollection() {
  }
};

} // namespace node_gdal
#endif
#endif
