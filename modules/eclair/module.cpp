#include "module.h"
#include <cstring>
#include <filesystem>
#include <iostream>
#include <jni.h>
#include <jvmloader.h>
#include <optional>
#include <sstream>
#include <string>

namespace fs = std::filesystem;

static JavaVM *jvm = nullptr;
static bool jvm_initialized = false;
static jclass decoderClass = nullptr;
static jmethodID decodeBolt11InvoiceMethod = nullptr;
static jmethodID decodeOfferMethod = nullptr;

static std::string cached_classpath;

static void clear_pending_exception(JNIEnv *env) {
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
  }
}

static bool init_jvm() {
  if (jvm_initialized)
    return true;

  jvm = JvmLoader::get_jvm();
  if (!jvm) {
    return false;
  }

  JNIEnv *env = nullptr;
  jint ge = jvm->GetEnv((void **)&env, JNI_VERSION_1_8);
  if (ge != JNI_OK || !env) {
    return false;
  }

  jclass localDecoderClass = env->FindClass("EclairWrapper");
  if (!localDecoderClass) {
    clear_pending_exception(env);
    return false;
  }

  jclass globalDecoderClass =
      static_cast<jclass>(env->NewGlobalRef(localDecoderClass));
  env->DeleteLocalRef(localDecoderClass);
  if (!globalDecoderClass) {
    return false;
  }

  jmethodID decodeBolt11InvoiceMethodRef =
      env->GetStaticMethodID(globalDecoderClass, "decodeBolt11Invoice",
                             "(Ljava/lang/String;)Ljava/lang/String;");
  if (!decodeBolt11InvoiceMethodRef) {
    clear_pending_exception(env);
    env->DeleteGlobalRef(globalDecoderClass);
    return false;
  }

  jmethodID decodeOfferMethodRef = env->GetStaticMethodID(
      globalDecoderClass, "decodeOffer", "(Ljava/lang/String;)Ljava/lang/String;");
  if (!decodeOfferMethodRef) {
    clear_pending_exception(env);
    env->DeleteGlobalRef(globalDecoderClass);
    return false;
  }

  decoderClass = globalDecoderClass;
  decodeBolt11InvoiceMethod = decodeBolt11InvoiceMethodRef;
  decodeOfferMethod = decodeOfferMethodRef;
  jvm_initialized = true;
  return true;
}

static std::optional<std::string>
eclair_decode_invoice(const char *invoiceStr) {
  if (!init_jvm() || !jvm) {
    return "";
  }

  JNIEnv *env = nullptr;
  jint status = jvm->GetEnv((void **)&env, JNI_VERSION_1_8);
  if (status != JNI_OK || !env) {
    return "";
  }

  jstring jInvoiceStr = env->NewStringUTF(invoiceStr);
  if (!jInvoiceStr) {
    return "";
  }

  jstring jResult = static_cast<jstring>(env->CallStaticObjectMethod(
      decoderClass, decodeBolt11InvoiceMethod, jInvoiceStr));
  env->DeleteLocalRef(jInvoiceStr);

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

  // Handle invoices without payment secrets by returning null
  // This is needed because LND don't require payment secrets,
  // and we need to maintain compatibility with that implementation
  if (result == "payment_secret_error") {
    return std::nullopt;
  }

  return result;
}

static std::optional<std::string> eclair_decode_offer(const char *offerStr) {
  if (!init_jvm() || !jvm) {
    return "";
  }

  JNIEnv *env = nullptr;
  jint status = jvm->GetEnv((void **)&env, JNI_VERSION_1_8);
  if (status != JNI_OK || !env) {
    return "";
  }

  jstring jOfferStr = env->NewStringUTF(offerStr);
  if (!jOfferStr) {
    return "";
  }

  jstring jResult = static_cast<jstring>(
      env->CallStaticObjectMethod(decoderClass, decodeOfferMethod, jOfferStr));
  env->DeleteLocalRef(jOfferStr);

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

  return result;
}

namespace bitcoinfuzz {
namespace module {
Eclair::Eclair(void) : BaseModule("Eclair") {}

std::optional<std::string> Eclair::deserialize_invoice(std::string str) const {
  return eclair_decode_invoice(str.c_str());
}

std::optional<std::string> Eclair::deserialize_offer(std::string str) const {
  return eclair_decode_offer(str.c_str());
}
} // namespace module
} // namespace bitcoinfuzz
