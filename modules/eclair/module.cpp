#include "module.h"
#include <jni.h>
#include <string>
#include <optional>
#include <iostream>

namespace bitcoinfuzz {
    namespace module {
        Eclair::Eclair(void) : BaseModule("Eclair") {
            // Initialize JVM
            JavaVMInitArgs vm_args;
            JavaVMOption options[1];

            // Path the JAR file
            options[0].optionString = (char*)"-Djava.class.path=../../target/scala-2.13/lightning-invoice-parser.jar";

            vm_args.version = JNI_VERSION_1_8;
            vm_args.nOptions = 1;
            vm_args.options = options;
            vm_args.ignoreUnrecognized = false;

            // Create JVM
            jint rc = JNI_CreateJavaVM(&jvm, (void**)&env, &vm_args);
            if (rc != JNI_OK) {
                std::cerr << "Failed to create JVM" << std::endl;
                exit(1);
            }

            parserClass = env->FindClass("org/bitcoinfuzz/LightningInvoiceParser");
            if (parserClass == nullptr) {
                std::cerr << "Failed to find LightningInvoiceParser class" << std::endl;
                exit(1);
            }

            // Get the parseInvoice method ID
            parseMethod = env->GetStaticMethodID(parserClass, "parseInvoice", "(Ljava/lang/String;)Z");
            if (parseMethod == nullptr) {
                std::cerr << "Failed to find parseInvoice method" << std::endl;
                exit(1);
            }
        }

        Eclair::~Eclair() {
            // Destroy JVM when done
            jvm->DestroyJavaVM();
        }

        std::optional<std::string> Eclair::deserialize_invoice(std::string str)const {
            // Convert C++ string to Java string
            jstring jInvoice = env->NewStringUTF(str.c_str());

            // Call the Java method
            jboolean result = env->CallStaticBooleanMethod(parserClass, parseMethod, jInvoice);

            // Clean up
            env->DeleteLocalRef(jInvoice);

            // Return the result as a string
            if (result) {
                return str; // Return the original invoice if parsing was successful
            } else {
                return std::nullopt; // Return nullopt if parsing failed
            }
        }
    }
}

extern "C" bitcoinfuzz::BaseModule* create_module() {
  return new bitcoinfuzz::module::Eclair();
}
