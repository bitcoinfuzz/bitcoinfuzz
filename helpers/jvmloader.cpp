#include "jvmloader.h"

#include <string>
#include <sstream>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

constexpr const char* BITCOINFUZZ_PATH = BITCOINFUZZ_DIR;
static const fs::path BITCOINFUZZ_ROOT{BITCOINFUZZ_PATH};

static JvmLoader& instance() {
    static JvmLoader inst;
    return inst;
}

JavaVM* JvmLoader::get_jvm() {
    return instance().jvm;
}

std::string JvmLoader::build_classpath(const std::vector<std::string>& lib_dirs) {
    std::ostringstream cp;
    cp << "-Djava.class.path=";
    bool first = true;

    for (const auto& lib_dir : lib_dirs) {
        try {
            for (const auto& entry : fs::directory_iterator(lib_dir)) {
                if (entry.is_regular_file() && entry.path().extension() == ".jar") {
                    if (!first) cp << ":";
                    cp << entry.path().string();
                    first = false;
                }
            }
        } catch (const fs::filesystem_error& e) {
            std::cerr << "Filesystem error in " << lib_dir << ": " << e.what() << std::endl;
        }
    }

    return cp.str();
}

JvmLoader::JvmLoader() : jvm(nullptr) {
    init();
}

JvmLoader::~JvmLoader() = default;

void JvmLoader::init() {
    if (jvm != nullptr) return;

    JavaVMInitArgs vm_args;
    JavaVMOption options[7];

    std::vector<std::string> lib_dirs;
    #ifdef LIGHTNING_KMP
        lib_dirs.push_back((BITCOINFUZZ_ROOT / "modules" / "lightningkmp" / "lib").string());
    #endif
    #ifdef ECLAIR
        lib_dirs.push_back((BITCOINFUZZ_ROOT / "modules" / "eclair" / "lib").string());
    #endif
    std::string classpathStr = build_classpath(lib_dirs);

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
        std::cerr << "Failed to create JVM, error code: " << res << std::endl;
        jvm = nullptr;
    }
}
