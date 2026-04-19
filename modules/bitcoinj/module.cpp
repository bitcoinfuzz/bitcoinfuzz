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
static jclass bitcoinJWrapperClass = nullptr;
static jmethodID createMasterKeyMethod = nullptr;
static jmethodID deserializeExtendedKeyMethod = nullptr;

static void clear_pending_exception(JNIEnv *env) {
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
  }
}

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

  jclass localWrapperClass = env->FindClass("wrapper/Wrapper");
  if (!localWrapperClass) {
    clear_pending_exception(env);
    return false;
  }

  jclass globalWrapperClass =
      static_cast<jclass>(env->NewGlobalRef(localWrapperClass));
  env->DeleteLocalRef(localWrapperClass);
  if (!globalWrapperClass) {
    return false;
  }

  jmethodID createMasterKeyMethodRef = env->GetStaticMethodID(
      globalWrapperClass, "createMasterKey", "([B)Ljava/lang/String;");
  if (!createMasterKeyMethodRef) {
    clear_pending_exception(env);
    env->DeleteGlobalRef(globalWrapperClass);
    return false;
  }

  jmethodID deserializeExtendedKeyMethodRef = env->GetStaticMethodID(
      globalWrapperClass, "deserializeExtendedKey", "([B)Ljava/lang/String;");
  if (!deserializeExtendedKeyMethodRef) {
    clear_pending_exception(env);
    env->DeleteGlobalRef(globalWrapperClass);
    return false;
  }

  bitcoinJWrapperClass = globalWrapperClass;
  createMasterKeyMethod = createMasterKeyMethodRef;
  deserializeExtendedKeyMethod = deserializeExtendedKeyMethodRef;
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
      env->CallStaticObjectMethod(bitcoinJWrapperClass, methodRef, jBytes));

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
BitcoinJ::BitcoinJ(void) : BaseModule("BitcoinJ") {}

std::optional<std::string>
BitcoinJ::bip32_master_keygen(std::span<const uint8_t> buffer) const {
  return call_wrapper_method(createMasterKeyMethod, buffer);
}

std::optional<std::string> BitcoinJ::bip32_deserialize_extended_key(
    std::span<const uint8_t> buffer) const {
  return call_wrapper_method(deserializeExtendedKeyMethod, buffer);
}
} // namespace module
} // namespace bitcoinfuzz
