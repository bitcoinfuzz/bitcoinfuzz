#include "module.h"
#include <cstdlib>
#include <optional>
#include <quickjs.h>
#include <string_view>

static JSRuntime *js_runtime = nullptr;
static JSContext *js_context = nullptr;
static bool js_bundle_loaded = false;
static JSValue api_object = JS_UNDEFINED;
static JSValue miniscriptParseMethod = JS_UNDEFINED;

// Defined in ts/dist/bundle.c
extern const uint8_t qjsc_bundle[];
extern const uint32_t qjsc_bundle_size;

static bool init_js() {
  if (js_runtime != nullptr && js_context != nullptr && js_bundle_loaded)
    return true;

  if (js_runtime == nullptr) {
    js_runtime = JS_NewRuntime();
    if (js_runtime == nullptr) {
      return false;
    }
  }

  if (js_context == nullptr) {
    js_context = JS_NewContext(js_runtime);
    if (js_context == nullptr) {
      JS_FreeRuntime(js_runtime);
      js_runtime = nullptr;
      return false;
    }
  }

  if (js_bundle_loaded == false) {
    JSValue obj = JS_ReadObject(js_context, qjsc_bundle, qjsc_bundle_size,
                                JS_READ_OBJ_BYTECODE);
    if (JS_IsException(obj)) {
      return false;
    }

    JSValue eval_result = JS_EvalFunction(js_context, obj);
    if (JS_IsException(eval_result)) {
      JS_FreeValue(js_context, eval_result);
      return false;
    }
    JS_FreeValue(js_context, eval_result);

    JSValue global_obj = JS_GetGlobalObject(js_context);
    api_object = JS_GetPropertyStr(js_context, global_obj, "bitcoinerlab");
    miniscriptParseMethod = JS_GetPropertyStr(
        js_context, api_object, "bitcoinerlabMiniscriptMiniscriptParse");
    JS_FreeValue(js_context, global_obj);
    js_bundle_loaded = true;
  }

  return true;
}

static bool is_stack_overflow_exception() {
  JSValue exception = JS_GetException(js_context);
  JSValue name_val = JS_GetPropertyStr(js_context, exception, "name");
  JSValue message_val = JS_GetPropertyStr(js_context, exception, "message");

  const char *name = JS_ToCString(js_context, name_val);
  const char *message = JS_ToCString(js_context, message_val);

  const bool is_stack_overflow =
      name != nullptr && message != nullptr &&
      std::string_view{name} == "InternalError" &&
      std::string_view{message}.find("stack overflow") !=
          std::string_view::npos;

  JS_FreeCString(js_context, message);
  JS_FreeCString(js_context, name);
  JS_FreeValue(js_context, message_val);
  JS_FreeValue(js_context, name_val);
  JS_FreeValue(js_context, exception);

  return is_stack_overflow;
}

std::optional<bool>
bitcoinerlab_miniscript_miniscript_parse(const uint8_t *input,
                                         size_t input_len) {
  if (!init_js()) {
    std::abort();
  }

  JSValue input_buf = JS_NewArrayBufferCopy(js_context, input, input_len);
  JSValue result_val =
      JS_Call(js_context, miniscriptParseMethod, api_object, 1, &input_buf);

  std::optional<bool> result;
  // If a stack overflow was raised by bitcoinerlab, skip the input.
  // Otherwise, in case of any non-specified exception, should return false
  if (JS_IsException(result_val)) {
    result = is_stack_overflow_exception() ? std::nullopt
                                           : std::optional<bool>{false};
  } else {
    result = JS_ToBool(js_context, result_val);
  }

  JS_FreeValue(js_context, result_val);
  JS_FreeValue(js_context, input_buf);

  return result;
}

namespace bitcoinfuzz {
namespace module {
BitcoinerlabMiniscript::BitcoinerlabMiniscript(void)
    : BaseModule("BitcoinerlabMiniscript") {}

std::optional<bool>
BitcoinerlabMiniscript::miniscript_parse(std::string str) const {
  return bitcoinerlab_miniscript_miniscript_parse(
      reinterpret_cast<const uint8_t *>(str.data()), str.size());
}
} // namespace module
} // namespace bitcoinfuzz
