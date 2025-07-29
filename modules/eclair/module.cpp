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
static jmethodID decodeMethod = nullptr;

static std::string cached_classpath;

static const std::string build_classpath() {
    if (!cached_classpath.empty()) return cached_classpath;
    
    std::ostringstream cp;
    cp << "-Djava.class.path=";
    bool first = true;
    
    try {
        for (const auto& entry : fs::directory_iterator("./modules/eclair/lib")) {
            auto ext = entry.path().extension();
            if (ext == ".jar" || ext == ".class") {
                if (!first) cp << ":";
                cp << entry.path().string();
                first = false;
            }
        }
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Filesystem error: " << e.what() << std::endl;
    }
    
    cached_classpath = cp.str();
    return cached_classpath;
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

    decoderClass = env->FindClass("Bolt11InvoiceWrapper");
    if (!decoderClass) {
        return false;
    }
    
    decoderClass = static_cast<jclass>(env->NewGlobalRef(decoderClass));
    
    decodeMethod = env->GetStaticMethodID(decoderClass, "decodeBolt11Invoice", "(Ljava/lang/String;)Ljava/lang/String;");
    if (!decodeMethod) {
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
        env->CallStaticObjectMethod(decoderClass, decodeMethod, jInvoiceStr)
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

namespace bitcoinfuzz {
    namespace module {
        Eclair::Eclair(void) : BaseModule("Eclair") {}

        std::optional<std::string> Eclair::deserialize_invoice(std::string str) const {
            return eclair_decode_invoice(str.c_str());
        }
    }
}