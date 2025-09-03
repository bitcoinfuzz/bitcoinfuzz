#include "module.h"
#include <jni.h>
#include <string>
#include <optional>
#include <iostream>
#include <filesystem>
#include <cstring>
#include <sstream>
#include <jvmloader.h>

namespace fs = std::filesystem;

static JavaVM* jvm = nullptr;
static jclass decoderClass = nullptr;
static jmethodID decodeBolt11InvoiceMethod = nullptr;
static jmethodID decodeOfferMethod = nullptr;

static std::string cached_classpath;

static bool init_jvm() {
    if (jvm != nullptr) return true;

    jvm = JvmLoader::get_jvm();
    JNIEnv* env = nullptr;
    jint ge = jvm->GetEnv((void**)&env, JNI_VERSION_1_8);

    decoderClass = env->FindClass("EclairWrapper");
    if (!decoderClass) {
        return false;
    }
    
    decoderClass = static_cast<jclass>(env->NewGlobalRef(decoderClass));
    
    decodeBolt11InvoiceMethod = env->GetStaticMethodID(decoderClass, "decodeBolt11Invoice", "(Ljava/lang/String;)Ljava/lang/String;");
    if (!decodeBolt11InvoiceMethod) {
        return false;
    }

    decodeOfferMethod = env->GetStaticMethodID(decoderClass, "decodeOffer", "(Ljava/lang/String;)Ljava/lang/String;");
    if (!decodeOfferMethod) {
        return false;
    }

    return true;
}

static std::optional<std::string> eclair_decode_invoice(const char* invoiceStr) {
    if (!init_jvm() || !jvm) {
        std::abort();
    }

    JNIEnv* env = nullptr;
    jint status = jvm->GetEnv((void**)&env, JNI_VERSION_1_8);

    jstring jInvoiceStr = env->NewStringUTF(invoiceStr);
    if (!jInvoiceStr) {
        return "";
    }

    jstring jResult = static_cast<jstring>(
        env->CallStaticObjectMethod(decoderClass, decodeBolt11InvoiceMethod, jInvoiceStr)
    );
    env->DeleteLocalRef(jInvoiceStr);

    if (!jResult) {
        return "";
    }
    
    const char* resultChars = env->GetStringUTFChars(jResult, nullptr);
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
    if(result == "payment_secret_error") {
        return std::nullopt;
    }
    
    return result;
}

static std::optional<std::string> eclair_decode_offer(const char* offerStr) {
    if (!init_jvm() || !jvm) {
        std::abort();
    }

    JNIEnv* env = nullptr;
    jint status = jvm->GetEnv((void**)&env, JNI_VERSION_1_8);

    
    jstring jOfferStr = env->NewStringUTF(offerStr);
    if (!jOfferStr) {
        return "";
    }

    jstring jResult = static_cast<jstring>(
        env->CallStaticObjectMethod(decoderClass, decodeOfferMethod, jOfferStr)
    );
    env->DeleteLocalRef(jOfferStr);
    
    if (!jResult) {
        return "";
    }
    
    const char* resultChars = env->GetStringUTFChars(jResult, nullptr);
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
    }
}