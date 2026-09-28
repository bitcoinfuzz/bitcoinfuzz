#include "jnibridge.h"

#include "jvmloader.h"
#include <bitcoinfuzz/ffi.h>
#include <bitcoinfuzz/result.h>
#include <cstdio>
#include <cstdlib>
#include <jni.h>
#include <map>

namespace bitcoinfuzz {
namespace {
[[noreturn]] void Die(JNIEnv *env, const std::string &what) {
  std::fprintf(stderr, "jnibridge: %s\n", what.c_str());
  if (env != nullptr && env->ExceptionCheck())
    env->ExceptionDescribe();
  std::abort();
}

JNIEnv *Env() {
  JavaVM *jvm = JvmLoader::get_jvm();
  JNIEnv *env = nullptr;
  if (jvm == nullptr ||
      jvm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_8) != JNI_OK)
    Die(nullptr, "no JVM attached to this thread");
  return env;
}

jclass Class(JNIEnv *env, const std::string &name) {
  static std::map<std::string, jclass> classes;
  auto it = classes.find(name);
  if (it != classes.end())
    return it->second;
  jclass local = env->FindClass(name.c_str());
  if (local == nullptr)
    Die(env, "cannot load " + name);
  auto global = static_cast<jclass>(env->NewGlobalRef(local));
  env->DeleteLocalRef(local);
  classes.emplace(name, global);
  return global;
}

jmethodID Method(JNIEnv *env, jclass cls, const std::string &class_name,
                 const char *method, const char *signature) {
  static std::map<std::string, jmethodID> methods;
  const std::string key = class_name + "." + method + signature;
  auto it = methods.find(key);
  if (it != methods.end())
    return it->second;
  jmethodID id = env->GetStaticMethodID(cls, method, signature);
  if (id == nullptr)
    Die(env, key + " not found");
  methods.emplace(key, id);
  return id;
}

std::optional<std::string> Take(JNIEnv *env, const std::string &call,
                                jobject result) {
  if (env->ExceptionCheck())
    Die(env, call + " threw");
  if (result == nullptr)
    Die(env, call + " returned null");

  // Every wrapper returns the same BfResult class, so its fields resolve once.
  static const auto [status_field, value_field] = [&] {
    jclass cls = env->GetObjectClass(result);
    auto fields = std::pair{env->GetFieldID(cls, "status", "B"),
                            env->GetFieldID(cls, "value", "[B")};
    env->DeleteLocalRef(cls);
    if (fields.first == nullptr || fields.second == nullptr)
      Die(env, call + " did not return a bitcoinfuzz.BfResult");
    return fields;
  }();

  const jbyte status = env->GetByteField(result, status_field);
  auto bytes =
      static_cast<jbyteArray>(env->GetObjectField(result, value_field));
  std::string value(static_cast<size_t>(env->GetArrayLength(bytes)), '\0');
  env->GetByteArrayRegion(bytes, 0, static_cast<jsize>(value.size()),
                          reinterpret_cast<jbyte *>(value.data()));
  env->DeleteLocalRef(bytes);
  env->DeleteLocalRef(result);

  switch (status) {
  case BF_OK:
    return Ok(std::move(value));
  case BF_FAIL:
    return Fail(std::move(value));
  case BF_SKIP:
    return Skip();
  }
  Die(env, call + " returned an unknown status");
}
} // namespace

std::optional<std::string> CallJvm(const char *class_name, const char *method,
                                   std::span<const uint8_t> input) {
  JNIEnv *env = Env();
  jclass cls = Class(env, class_name);
  jmethodID id =
      Method(env, cls, class_name, method, "([B)Lbitcoinfuzz/BfResult;");
  jbyteArray array = env->NewByteArray(static_cast<jsize>(input.size()));
  if (array == nullptr)
    Die(env, "NewByteArray failed");
  env->SetByteArrayRegion(array, 0, static_cast<jsize>(input.size()),
                          reinterpret_cast<const jbyte *>(input.data()));
  jobject result = env->CallStaticObjectMethod(cls, id, array);
  env->DeleteLocalRef(array);
  return Take(env, std::string(class_name) + "." + method, result);
}

std::optional<std::string> CallJvm(const char *class_name, const char *method,
                                   const std::string &input) {
  JNIEnv *env = Env();
  jclass cls = Class(env, class_name);
  jmethodID id = Method(env, cls, class_name, method,
                        "(Ljava/lang/String;)Lbitcoinfuzz/BfResult;");
  jstring str = env->NewStringUTF(input.c_str());
  if (str == nullptr)
    Die(env, "NewStringUTF failed");
  jobject result = env->CallStaticObjectMethod(cls, id, str);
  env->DeleteLocalRef(str);
  return Take(env, std::string(class_name) + "." + method, result);
}
} // namespace bitcoinfuzz
