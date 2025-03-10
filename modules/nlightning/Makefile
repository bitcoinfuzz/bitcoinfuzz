# Detect operating system
ifeq ($(OS),Windows_NT)
    DETECTED_OS := windows
    # Use appropriate extensions for Windows
    EXE_EXT := .exe
    LIB_EXT := .lib
    DLL_EXT := .dll
    # Windows commands
    RM := del /Q
    MKDIR := mkdir
    MV := move
    AR := lib
    ARFLAGS := /OUT:
else
    UNAME_S := $(shell uname -s)
    ifeq ($(UNAME_S),Linux)
        DETECTED_OS := linux
    endif
    ifeq ($(UNAME_S),Darwin)
        DETECTED_OS := macos
        # Check for Apple Silicon
        UNAME_M := $(shell uname -m)
        ifeq ($(UNAME_M),arm64)
            DETECTED_ARCH := arm64
        else
            DETECTED_ARCH := x64
        endif
    endif
    # Unix commands
    RM := rm -f
    MKDIR := mkdir -p
    MV := mv
    AR := ar
    ARFLAGS := rcs
    RANLIB := ranlib
    # Standard extensions
    EXE_EXT :=
    LIB_EXT := .a
    DLL_EXT := .so
    ifeq ($(DETECTED_OS),macos)
        DLL_EXT := .dylib
    endif
endif

# Auto-detect compiler if not specified
ifndef CXX
    ifeq ($(DETECTED_OS),windows)
        CXX := cl
    else
        CXX := g++
        # Check if clang++ is available
        ifneq ($(shell which clang++ 2>/dev/null),)
            CXX := clang++
        endif
        # On macOS with Homebrew, check for LLVM clang
        ifeq ($(DETECTED_OS),macos)
            ifneq ($(shell test -d /opt/homebrew/opt/llvm/bin && echo exists),)
                CXX := /opt/homebrew/opt/llvm/bin/clang++
            endif
        endif
    endif
endif

# Set compiler flags based on platform
ifeq ($(DETECTED_OS),windows)
    CXXFLAGS := /W4 /std:c++20 /EHsc /I. /I..\..\include
    PIC_FLAG :=
else
    CXXFLAGS := -Wall -Wextra -std=c++20 -I. -I ../../include
    PIC_FLAG := -fPIC
endif

# Auto-detect runtime ID if not specified
ifndef RUNTIME_ID
    ifeq ($(DETECTED_OS),windows)
        RUNTIME_ID := win-x64
    else ifeq ($(DETECTED_OS),linux)
        RUNTIME_ID := linux-x64
    else ifeq ($(DETECTED_OS),macos)
        ifeq ($(DETECTED_ARCH),arm64)
            RUNTIME_ID := osx-arm64
        else
            RUNTIME_ID := osx-x64
        endif
    else
        # Default fallback
        RUNTIME_ID := linux-x64
    endif
endif

# Output directory for "dotnet publish"
DOTNET_OUT_DIR := src/bin

# Output library name
MODULE_LIB := module$(LIB_EXT)
SODIUM_LIB := libsodium$(DLL_EXT)
ifeq ($(DETECTED_OS),windows)
    SODIUM_LIB := sodium$(DLL_EXT)
endif

all: $(MODULE_LIB)

$(MODULE_LIB): dotnet module.o
	$(AR) $(ARFLAGS) $(MODULE_LIB) module.o
ifeq ($(DETECTED_OS),windows)
	# No ranlib equivalent needed on Windows
else
	$(RANLIB) $(MODULE_LIB)
endif

dotnet:
	dotnet publish src \
		-c Release \
		-r $(RUNTIME_ID) \
		-o $(DOTNET_OUT_DIR)
	$(MV) $(DOTNET_OUT_DIR)/$(SODIUM_LIB) ../../$(SODIUM_LIB)

module.o: module.cpp
ifeq ($(DETECTED_OS),windows)
	$(CXX) $(CXXFLAGS) /c module.cpp /Fo:module.o
else
	$(CXX) $(CXXFLAGS) $(PIC_FLAG) -c module.cpp -o module.o
endif

clean:
ifeq ($(DETECTED_OS),windows)
	$(RM) module.o $(MODULE_LIB) 
	$(RM) $(DOTNET_OUT_DIR)\*.*
else
	$(RM) module.o $(MODULE_LIB)
	$(RM) -r $(DOTNET_OUT_DIR)
endif

.PHONY: all dotnet clean