#include "module.h"
#include <jni.h>
#include <string>
#include <optional>
#include <iostream>
#include <filesystem>
#include <cstring>
#include <sstream>

namespace fs = std::filesystem;

static JavaVM* jvm = nullptr;
static jclass decoderClass = nullptr;
static jmethodID decodeMethodInvoice = nullptr;
static jmethodID decodeMethodOffer = nullptr;

constexpr const char* LIB_DIR_PATH = LIB_DIR;

static std::string build_classpath() {
    std::ostringstream cp;
    cp << "-Djava.class.path=";
    bool first = true;
    
    try {
        for (const auto& entry : fs::directory_iterator(LIB_DIR_PATH)) {
            if (entry.path().extension() == ".jar") {
                if (!first) cp << ":";
                cp << entry.path().string();
                first = false;
            }
        }
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Filesystem error: " << e.what() << std::endl;
    }
    
    return cp.str();
}

static bool init_jvm() {
    if (jvm != nullptr) return true;

    JavaVMInitArgs vm_args;
    JavaVMOption options[7];

    std::string classpathStr = build_classpath();
    options[0].optionString = const_cast<char*>(classpathStr.c_str());
    options[1].optionString = const_cast<char*>("-Xmx512m");
    options[2].optionString = const_cast<char*>("-XX:+UseSignalChaining");
    options[3].optionString = const_cast<char*>("-XX:+ExitOnOutOfMemoryError");
    options[4].optionString = const_cast<char*>("-XX:ErrorFile=./hs_err_pid%p.log");
    options[5].optionString = const_cast<char*>("-Xrs"); 
    options[6].optionString = const_cast<char*>("-Dsun.misc.SignalHandler.handlers=allow");

    vm_args.version = JNI_VERSION_1_8;
    vm_args.nOptions = 7;
    vm_args.options = options;
    vm_args.ignoreUnrecognized = JNI_FALSE;

    JNIEnv* env = nullptr;
    jint res = JNI_CreateJavaVM(&jvm, (void**)&env, &vm_args);
    if (res != JNI_OK) {
        return false;
    }

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