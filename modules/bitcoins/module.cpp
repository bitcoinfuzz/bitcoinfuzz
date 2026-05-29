#include "module.h"
#include <iostream>
#include <jni.h>
#include <jvmloader.h>
#include <optional>
#include <string>

static JavaVM *jvm = nullptr;
static jclass wrapperClass = nullptr;
static jmethodID createMasterKeyMethod = nullptr;
static jmethodID deserializeExtendedKeyMethod = nullptr;
static jmethodID parsePSBTMethod = nullptr;
static bool init_attempted = false;
static bool init_ok = false;

static bool init_jvm() {
  if (init_attempted) {
    return init_ok;
  }
  init_attempted = true;

  jvm = JvmLoader::get_jvm();
  if (!jvm) {
    std::cerr << "[BitcoinS] JvmLoader::get_jvm() returned null" << std::endl;
    return false;
  }

  JNIEnv *env = nullptr;
  jvm->GetEnv((void **)&env, JNI_VERSION_1_8);
  if (!env) {
    std::cerr << "[BitcoinS] GetEnv failed" << std::endl;
    return false;
  }

  jclass rawClass = env->FindClass("BitcoinSWrapper");
  if (!rawClass) {
    if (env->ExceptionCheck()) {
      std::cerr << "[BitcoinS] FindClass(BitcoinSWrapper) threw:" << std::endl;
      env->ExceptionDescribe();
      env->ExceptionClear();
    } else {
      std::cerr << "[BitcoinS] FindClass(BitcoinSWrapper) returned null"
                << std::endl;
    }
    return false;
  }
  wrapperClass = static_cast<jclass>(env->NewGlobalRef(rawClass));
  env->DeleteLocalRef(rawClass);

  createMasterKeyMethod = env->GetStaticMethodID(
      wrapperClass, "createMasterKey", "([B)Ljava/lang/String;");
  if (!createMasterKeyMethod) {
    std::cerr << "[BitcoinS] GetStaticMethodID(createMasterKey) failed"
              << std::endl;
    if (env->ExceptionCheck()) {
      env->ExceptionDescribe();
      env->ExceptionClear();
    }
    return false;
  }

  deserializeExtendedKeyMethod = env->GetStaticMethodID(
      wrapperClass, "deserializeExtendedKey", "([B)Ljava/lang/String;");
  if (!deserializeExtendedKeyMethod) {
    std::cerr << "[BitcoinS] GetStaticMethodID(deserializeExtendedKey) failed"
              << std::endl;
    if (env->ExceptionCheck()) {
      env->ExceptionDescribe();
      env->ExceptionClear();
    }
    return false;
  }

  parsePSBTMethod = env->GetStaticMethodID(wrapperClass, "parsePSBT",
                                           "([B)Ljava/lang/String;");
  if (!parsePSBTMethod) {
    std::cerr << "[BitcoinS] GetStaticMethodID(parsePSBT) failed" << std::endl;
    if (env->ExceptionCheck()) {
      env->ExceptionDescribe();
      env->ExceptionClear();
    }
    return false;
  }

  init_ok = true;
  return true;
}

static std::optional<std::string>
call_static_bytes_method(jmethodID *pMethodID,
                         std::span<const uint8_t> buffer) {
  if (!init_jvm() || !jvm || !wrapperClass || !*pMethodID) {
    std::abort();
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
  if (!buffer.empty()) {
    env->SetByteArrayRegion(jBytes, 0, static_cast<jsize>(buffer.size()),
                            reinterpret_cast<const jbyte *>(buffer.data()));
  }

  jstring jResult = static_cast<jstring>(
      env->CallStaticObjectMethod(wrapperClass, *pMethodID, jBytes));
  env->DeleteLocalRef(jBytes);

  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    return "";
  }

  if (!jResult) {
    return "";
  }

  const char *chars = env->GetStringUTFChars(jResult, nullptr);
  if (!chars) {
    env->DeleteLocalRef(jResult);
    return "";
  }

  std::string result(chars);
  env->ReleaseStringUTFChars(jResult, chars);
  env->DeleteLocalRef(jResult);

  if (result == "skip error") {
    return std::nullopt;
  }

  return result;
}

namespace bitcoinfuzz {
namespace module {

BitcoinS::BitcoinS(void) : BaseModule("BitcoinS") {}

std::optional<std::string>
BitcoinS::bip32_master_keygen(std::span<const uint8_t> buffer) const {
  return call_static_bytes_method(&createMasterKeyMethod, buffer);
}

std::optional<std::string> BitcoinS::bip32_deserialize_extended_key(
    std::span<const uint8_t> buffer) const {
  return call_static_bytes_method(&deserializeExtendedKeyMethod, buffer);
}

std::optional<std::string>
BitcoinS::psbt_parse(std::span<const uint8_t> buffer) const {
  auto result = call_static_bytes_method(&parsePSBTMethod, buffer);
  if (result.has_value() && result->empty()) {
    return std::nullopt;
  }
  return result;
}

} // namespace module
} // namespace bitcoinfuzz
