#include <napi.h>

template <class T> class BaseObj : public Napi::ObjectWrap<T> {};

class Obj : public BaseObj<Obj> {
    public:
  Napi::Value ToString(const Napi::CallbackInfo &);
  static void Init();
};

Napi::Value Obj::ToString(const Napi::CallbackInfo &info) {
  return Napi::String::New(info.Env(), "Obj");
}

void Obj::Init() {

  ObjectWrap<Obj>::InstanceMethod("toString", &ToString);

  //Napi::Value (Obj::*method1)(const Napi::CallbackInfo &info) = &ToString;
  //Napi::Value (BaseObj::*method2)(const Napi::CallbackInfo &info) = &ToString;

  InstanceMethod("toString", &ToString);
}
