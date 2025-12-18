// Betti-RDL Node.js Bindings
// Native addon for JavaScript/TypeScript

#include "../src/cpp_kernel/betti_rdl_c_api.h"

#include <napi.h>

class BettiKernelWrapper : public Napi::ObjectWrap<BettiKernelWrapper> {
public:
  static Napi::Object Init(Napi::Env env, Napi::Object exports);
  BettiKernelWrapper(const Napi::CallbackInfo &info);
  ~BettiKernelWrapper() override;

private:
  BettiRDLCompute *kernel_ = nullptr;

  Napi::Value SpawnProcess(const Napi::CallbackInfo &info);
  Napi::Value InjectEvent(const Napi::CallbackInfo &info);
  Napi::Value Run(const Napi::CallbackInfo &info);
  Napi::Value GetEventsProcessed(const Napi::CallbackInfo &info);
  Napi::Value GetCurrentTime(const Napi::CallbackInfo &info);
  Napi::Value GetProcessCount(const Napi::CallbackInfo &info);
  Napi::Value GetTelemetry(const Napi::CallbackInfo &info);
  Napi::Value GetProcessState(const Napi::CallbackInfo &info);
};

BettiKernelWrapper::BettiKernelWrapper(const Napi::CallbackInfo &info)
    : Napi::ObjectWrap<BettiKernelWrapper>(info) {
  kernel_ = betti_rdl_create();
  if (!kernel_) {
    Napi::Error::New(info.Env(), "Failed to create Betti-RDL kernel")
        .ThrowAsJavaScriptException();
  }
}

BettiKernelWrapper::~BettiKernelWrapper() {
  if (kernel_) {
    betti_rdl_destroy(kernel_);
    kernel_ = nullptr;
  }
}

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
          InstanceMethod("getProcessCount", &BettiKernelWrapper::GetProcessCount),
          InstanceMethod("getTelemetry", &BettiKernelWrapper::GetTelemetry),
          InstanceMethod("getProcessState", &BettiKernelWrapper::GetProcessState),
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

  const int x = info[0].As<Napi::Number>().Int32Value();
  const int y = info[1].As<Napi::Number>().Int32Value();
  const int z = info[2].As<Napi::Number>().Int32Value();

  betti_rdl_spawn_process(kernel_, x, y, z);
  return env.Undefined();
}

Napi::Value BettiKernelWrapper::InjectEvent(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();

  if (info.Length() < 4) {
    Napi::TypeError::New(env, "Expected 4 arguments")
        .ThrowAsJavaScriptException();
    return env.Null();
  }

  const int x = info[0].As<Napi::Number>().Int32Value();
  const int y = info[1].As<Napi::Number>().Int32Value();
  const int z = info[2].As<Napi::Number>().Int32Value();
  const int value = info[3].As<Napi::Number>().Int32Value();

  betti_rdl_inject_event(kernel_, x, y, z, value);
  return env.Undefined();
}

Napi::Value BettiKernelWrapper::Run(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();

  if (info.Length() < 1) {
    Napi::TypeError::New(env, "Expected 1 argument")
        .ThrowAsJavaScriptException();
    return env.Null();
  }

  const int maxEvents = info[0].As<Napi::Number>().Int32Value();
  const int result = betti_rdl_run(kernel_, maxEvents);
  return Napi::Number::New(env, result);
}

Napi::Value
BettiKernelWrapper::GetEventsProcessed(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();
  return Napi::Number::New(env,
                           static_cast<double>(
                               betti_rdl_get_events_processed(kernel_)));
}

Napi::Value BettiKernelWrapper::GetCurrentTime(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();
  return Napi::Number::New(
      env, static_cast<double>(betti_rdl_get_current_time(kernel_)));
}

Napi::Value BettiKernelWrapper::GetProcessCount(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();
  return Napi::Number::New(
      env, static_cast<double>(betti_rdl_get_process_count(kernel_)));
}

Napi::Value BettiKernelWrapper::GetTelemetry(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();

  const BettiRDLTelemetry telemetry = betti_rdl_get_telemetry(kernel_);

  Napi::Object obj = Napi::Object::New(env);
  obj.Set("events_processed",
          Napi::Number::New(env, static_cast<double>(telemetry.events_processed)));
  obj.Set("current_time",
          Napi::Number::New(env, static_cast<double>(telemetry.current_time)));
  obj.Set("process_count",
          Napi::Number::New(env, static_cast<double>(telemetry.process_count)));
  obj.Set("memory_used",
          Napi::Number::New(env, static_cast<double>(telemetry.memory_used)));

  return obj;
}

Napi::Value BettiKernelWrapper::GetProcessState(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();

  if (info.Length() < 1) {
    Napi::TypeError::New(env, "Expected 1 argument")
        .ThrowAsJavaScriptException();
    return env.Null();
  }

  const int pid = info[0].As<Napi::Number>().Int32Value();
  return Napi::Number::New(env, betti_rdl_get_process_state(kernel_, pid));
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
  return BettiKernelWrapper::Init(env, exports);
}

NODE_API_MODULE(betti_rdl, Init)
