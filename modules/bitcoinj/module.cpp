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
static jclass bitcoinJWrapperClass = nullptr;
static jmethodID createMasterKeyMethod = nullptr;

static bool init_jvm() {
  if (jvm != nullptr) {
    return true;
  }

  jvm = JvmLoader::get_jvm();
  JNIEnv *env = nullptr;
  jint ge = jvm->GetEnv((void **)&env, JNI_VERSION_1_8);

  bitcoinJWrapperClass = env->FindClass("wrapper/Wrapper");
  if (!bitcoinJWrapperClass) {
    return false;
  }

  bitcoinJWrapperClass =
      static_cast<jclass>(env->NewGlobalRef(bitcoinJWrapperClass));

  createMasterKeyMethod = env->GetStaticMethodID(
      bitcoinJWrapperClass, "createMasterKey", "([B)Ljava/lang/String;");
  if (!createMasterKeyMethod) {
    return false;
  }
  return true;
}

static std::optional<std::string>
bitcoinj_bip32_master_keygen(std::span<const uint8_t> buffer) {
  if (!init_jvm() || !jvm) {
    return "";
  }
  JNIEnv *env = nullptr;
  jint status = jvm->GetEnv((void **)&env, JNI_VERSION_1_8);

  jbyteArray jSeed = env->NewByteArray(buffer.size());
  env->SetByteArrayRegion(jSeed, 0, buffer.size(),
                          reinterpret_cast<const jbyte *>(buffer.data()));
  if (!jSeed) {
    return "";
  }
  jstring jResult = static_cast<jstring>(env->CallStaticObjectMethod(
      bitcoinJWrapperClass, createMasterKeyMethod, jSeed));
  env->DeleteLocalRef(jSeed);

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
  return bitcoinj_bip32_master_keygen(buffer);
}
} // namespace module
} // namespace bitcoinfuzz