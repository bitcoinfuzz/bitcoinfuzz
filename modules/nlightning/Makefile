all: module.a

CXXFLAGS += -Wall -Wextra -fsanitize=address,fuzzer -std=c++20 -I ../../include

UNAME_S := $(shell uname -s)
RUNTIME_ID := $(shell echo $(UNAME_S)  | tr '[:upper:]' '[:lower:]' | sed 's/darwin/osx/')-$(shell uname -m | sed 's/x86_64/x64/; s/aarch64/arm64/')
ifeq ($(UNAME_S), Darwin)
  NLIGHTNING_LIB_EXT := dylib
else
  NLIGHTNING_LIB_EXT := so
endif

NLIGHTNING_LIB_PATH := ./bin/Release/net9.0/$(RUNTIME_ID)/native/NLightning.CppBridge.$(NLIGHTNING_LIB_EXT)

$(NLIGHTNING_LIB_PATH):
	dotnet publish -c Release -r $(RUNTIME_ID) --self-contained
	# Fix the dylib's install name on macOS: .NET embeds a relative path like
  # "bin/Release/net9.0/osx-arm64/native/NLightning.CppBridge.dylib" inside the library,
  # but we're copying it to the project root, so we need to update the internal
  # reference to match the new location where the runtime loader will find it
	@if [ "$(UNAME_S)" = "Darwin" ]; then \
		llvm-install-name-tool -id ./NLightning.CppBridge.$(NLIGHTNING_LIB_EXT)  $(NLIGHTNING_LIB_PATH); \
	fi
	cp $(NLIGHTNING_LIB_PATH) ../../NLightning.CppBridge.$(NLIGHTNING_LIB_EXT)

module.a: module.o $(NLIGHTNING_LIB_PATH)
	ar rcs module.a module.o

module.o: module.cpp module.h
	$(CXX) $(CXXFLAGS) -I . -fPIC -c module.cpp -o module.o

format:
	clang-format -i module.h module.cpp NLightning/nlightning_lib.h
	dotnet format

check-format:
	clang-format -Werror --fail-on-incomplete-format -n module.h module.cpp NLightning/nlightning_lib.h
	dotnet format --verify-no-changes

clean:
	rm -rf *.o module.a ./bin ./obj

.PHONY: $(NLIGHTNING_LIB_PATH)
