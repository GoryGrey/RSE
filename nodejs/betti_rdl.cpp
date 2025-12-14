// Betti-RDL Node.js Bindings
// Native addon for JavaScript/TypeScript

#include "../src/cpp_kernel/demos/BettiRDLCompute.h"
#include <napi.h>


class BettiKernelWrapper : public Napi::ObjectWrap<BettiKernelWrapper> {
public:
  static Napi::Object Init(Napi::Env env, Napi::Object exports);
  BettiKernelWrapper(const Napi::CallbackInfo &info);

private:
  BettiRDLCompute kernel;

  Napi::Value SpawnProcess(const Napi::CallbackInfo &info);
  Napi::Value InjectEvent(const Napi::CallbackInfo &info);
  Napi::Value Run(const Napi::CallbackInfo &info);
  Napi::Value GetEventsProcessed(const Napi::CallbackInfo &info);
  Napi::Value GetCurrentTime(const Napi::CallbackInfo &info);
  Napi::Value GetProcessCount(const Napi::CallbackInfo &info);
  Napi::Value GetProcessState(const Napi::CallbackInfo &info);
};

BettiKernelWrapper::BettiKernelWrapper(const Napi::CallbackInfo &info)
    : Napi::ObjectWrap<BettiKernelWrapper>(info) {}

Napi::Object BettiKernelWrapper::Init(Napi::Env env, Napi::Object exports) {
  Napi::Function func = DefineClass(
      env, "Kernel",
      {
          InstanceMethod("spawnProcess", &BettiKernelWrapper::SpawnProcess),
          InstanceMethod("injectEvent", &BettiKernelWrapper::InjectEvent),
          InstanceMethod("run", &BettiKernelWrapper::Run),
          InstanceMethod("getEventsProcessed",
                         &BettiKernelWrapper::GetEventsProcessed),
          InstanceMethod("getCurrentTime", &BettiKernelWrapper::GetCurrentTime),
          InstanceMethod("getProcessCount",
                         &BettiKernelWrapper::GetProcessCount),
          InstanceMethod("getProcessState",
                         &BettiKernelWrapper::GetProcessState),
      });

  Napi::FunctionReference *constructor = new Napi::FunctionReference();
  *constructor = Napi::Persistent(func);
  env.SetInstanceData(constructor);

  exports.Set("Kernel", func);
  return exports;
}

Napi::Value BettiKernelWrapper::SpawnProcess(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();

  if (info.Length() < 3) {
    Napi::TypeError::New(env, "Expected 3 arguments")
        .ThrowAsJavaScriptException();
    return env.Null();
  }

  int x = info[0].As<Napi::Number>().Int32Value();
  int y = info[1].As<Napi::Number>().Int32Value();
  int z = info[2].As<Napi::Number>().Int32Value();

  kernel.spawnProcess(x, y, z);
  return env.Undefined();
}

Napi::Value BettiKernelWrapper::InjectEvent(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();

  if (info.Length() < 4) {
    Napi::TypeError::New(env, "Expected 4 arguments")
        .ThrowAsJavaScriptException();
    return env.Null();
  }

  int x = info[0].As<Napi::Number>().Int32Value();
  int y = info[1].As<Napi::Number>().Int32Value();
  int z = info[2].As<Napi::Number>().Int32Value();
  int value = info[3].As<Napi::Number>().Int32Value();

  kernel.injectEvent(x, y, z, value);
  return env.Undefined();
}

Napi::Value BettiKernelWrapper::Run(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();

  if (info.Length() < 1) {
    Napi::TypeError::New(env, "Expected 1 argument")
        .ThrowAsJavaScriptException();
    return env.Null();
  }

  int maxEvents = info[0].As<Napi::Number>().Int32Value();
  kernel.run(maxEvents);
  return env.Undefined();
}

Napi::Value
BettiKernelWrapper::GetEventsProcessed(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();
  return Napi::Number::New(env, kernel.getEventsProcessed());
}

Napi::Value BettiKernelWrapper::GetCurrentTime(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();
  return Napi::Number::New(env, kernel.getCurrentTime());
}

Napi::Value
BettiKernelWrapper::GetProcessCount(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();
  return Napi::Number::New(env, kernel.getProcessCount());
}

Napi::Value
BettiKernelWrapper::GetProcessState(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();

  if (info.Length() < 1) {
    Napi::TypeError::New(env, "Expected 1 argument")
        .ThrowAsJavaScriptException();
    return env.Null();
  }

  int pid = info[0].As<Napi::Number>().Int32Value();
  return Napi::Number::New(env, kernel.getProcessState(pid));
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
  return BettiKernelWrapper::Init(env, exports);
}

NODE_API_MODULE(betti_rdl, Init)
