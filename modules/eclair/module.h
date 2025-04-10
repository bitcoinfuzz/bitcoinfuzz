#pragma once

#include <bitcoinfuzz/basemodule.h>
#include <jni.h>
#include <optional>
#include <string>

namespace bitcoinfuzz {
    namespace module {
        class Eclair : public BaseModule {
        public:
            Eclair(void);
            ~Eclair();
            std::optional<std::string> deserialize_invoice(std::string str) const override;

        private:
            JavaVM *jvm;
            JNIEnv *env;
            jclass parserClass;
            jmethodID parseMethod;
        };
    } 
}
