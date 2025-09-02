#include "module.h"
#include <jni.h>
#include <string>
#include <optional>
#include <iostream>
#include <filesystem>
#include <cstring>
#include <sstream>
#include <thread>
#include <jvmloader.h>

namespace fs = std::filesystem;

static JavaVM* jvm = nullptr;
static jclass decoderClass = nullptr;
static jmethodID decodeMethodInvoice = nullptr;
static jmethodID decodeMethodOffer = nullptr;

static bool init_jvm() {
    if (jvm != nullptr) {
        return true;
    }
    
    jvm = JvmLoader::get_jvm();
    JNIEnv* env = nullptr;
    jint ge = jvm->GetEnv((void**)&env, JNI_VERSION_1_8);

    decoderClass = env->FindClass("invoice/decode/InvoiceDecoder");
    if (!decoderClass) {
        return false;
    }
    
    decoderClass = static_cast<jclass>(env->NewGlobalRef(decoderClass));
    
    decodeMethodInvoice = env->GetStaticMethodID(decoderClass, "decodeBolt11Invoice", "(Ljava/lang/String;)Ljava/lang/String;");
    if (!decodeMethodInvoice) {
        return false;
    }

    decodeMethodOffer = env->GetStaticMethodID(decoderClass, "decodeBolt12Offer", "(Ljava/lang/String;)Ljava/lang/String;");
    if (!decodeMethodOffer) {
        return false;
    }

    return true;
}

static std::optional<std::string> lightning_kmp_decode_invoice(const char* invoiceStr) {
    if (!init_jvm() || !jvm) {
        return "";
    }

    JNIEnv* env = nullptr;
    jint status = jvm->GetEnv((void**)&env, JNI_VERSION_1_8);

    jstring jInvoiceStr = env->NewStringUTF(invoiceStr);
    if (!jInvoiceStr) {
        return "";
    }

    jstring jResult = static_cast<jstring>(
        env->CallStaticObjectMethod(decoderClass, decodeMethodInvoice, jInvoiceStr)
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

    if (result == "skip error") {
        return std::nullopt;
    }
    
    return result;
}

static std::optional<std::string> lightning_kmp_decode_offer(const char* offerStr) {
    if (!init_jvm() || !jvm) {
        return "";
    }

    JNIEnv* env = nullptr;
    jint status = jvm->GetEnv((void**)&env, JNI_VERSION_1_8);

    jstring jInvoiceStr = env->NewStringUTF(offerStr);
    if (!jInvoiceStr) {
        return "";
    }

    jstring jResult = static_cast<jstring>(
        env->CallStaticObjectMethod(decoderClass, decodeMethodOffer, jInvoiceStr)
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

    if (result == "skip error") {
        return std::nullopt;
    }
    
    return result;
}

namespace bitcoinfuzz {
    namespace module {
        LightningKmp::LightningKmp(void) : BaseModule("LightningKmp") {}

        std::optional<std::string> LightningKmp::deserialize_invoice(std::string str) const {
            return lightning_kmp_decode_invoice(str.c_str());
        }

        std::optional<std::string> LightningKmp::deserialize_offer(std::string str) const {
            return lightning_kmp_decode_offer(str.c_str());
        }
    }
}