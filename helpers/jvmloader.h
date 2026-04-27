#pragma once
#include <jni.h>
#include <string>
#include <vector>

class JvmLoader {
public:
  JvmLoader();
  ~JvmLoader();
  static JavaVM *get_jvm();

private:
  void init();
  static std::string build_classpath(const std::vector<std::string> &lib_dirs);

  JavaVM *jvm;

  JvmLoader(const JvmLoader &) = delete;
  JvmLoader &operator=(const JvmLoader &) = delete;
  JvmLoader(JvmLoader &&) = delete;
  JvmLoader &operator=(JvmLoader &&) = delete;
};