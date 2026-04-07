#include "module.h"
#include <cstring>
#include <filesystem>
#include <iostream>
#include <jni.h>
#include <jvmloader.h>
#include <optional>
#include <sstream>
#include <string>
#include <thread>

namespace fs = std::filesystem;

static JavaVM *jvm = nullptr;
static bool jvm_initialized = false;
static jclass bitcoinsWrapperClass = nullptr;
static jmethodID createMasterKeyMethod = nullptr;

static bool init_jvm() {
  if (jvm_initialized) {
    return true;
  }

  jvm = JvmLoader::get_jvm();
  if (!jvm) {
    return false;
  }

  JNIEnv *env = nullptr;
  jint ge = jvm->GetEnv((void **)&env, JNI_VERSION_1_8);
  if (ge != JNI_OK || !env) {
    return false;
  }

  bitcoinsWrapperClass = env->FindClass("wrapper/Wrapper");
  if (!bitcoinsWrapperClass) {
    return false;
  }
  bitcoinsWrapperClass =
      static_cast<jclass>(env->NewGlobalRef(bitcoinsWrapperClass));

  createMasterKeyMethod = env->GetStaticMethodID(
      bitcoinsWrapperClass, "createMasterKey", "([B)Ljava/lang/String;");
  if (!createMasterKeyMethod) {
    return false;
  }

  jvm_initialized = true;
  return true;
}

static std::optional<std::string>
call_wrapper_method(jmethodID &methodRef, std::span<const uint8_t> buffer) {
  if (!init_jvm() || !jvm) {
    return "";
  }

  if (!methodRef) {
    return "";
  }

  JNIEnv *env = nullptr;
  jint status = jvm->GetEnv((void **)&env, JNI_VERSION_1_8);
  if (status != JNI_OK || !env) {
    return "";
  }

  jbyteArray jBytes = env->NewByteArray(static_cast<jsize>(buffer.size()));
  if (!jBytes) {
    return "";
  }

  env->SetByteArrayRegion(jBytes, 0, static_cast<jsize>(buffer.size()),
                          reinterpret_cast<const jbyte *>(buffer.data()));

  jstring jResult = static_cast<jstring>(
      env->CallStaticObjectMethod(bitcoinsWrapperClass, methodRef, jBytes));

  env->DeleteLocalRef(jBytes);

  if (!jResult) {
    return "";
  }

  const char *resultChars = env->GetStringUTFChars(jResult, nullptr);
  if (!resultChars) {
    env->DeleteLocalRef(jResult);
    return "";
  }

  std::string result(resultChars);
  env->ReleaseStringUTFChars(jResult, resultChars);
  env->DeleteLocalRef(jResult);

  if (result == "skip error") {
    return std::nullopt;
  }
  return result;
}

namespace bitcoinfuzz {
namespace module {
Bitcoins::Bitcoins(void) : BaseModule("Bitcoins") {}

std::optional<std::string>
Bitcoins::bip32_master_keygen(std::span<const uint8_t> buffer) const {
  return call_wrapper_method(createMasterKeyMethod, buffer);
}
} // namespace module
} // namespace bitcoinfuzz
