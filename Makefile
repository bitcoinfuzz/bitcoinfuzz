all: bitcoinfuzz

CXX := clang++

# Use environment variables for Boost include path
BOOST_INCLUDE_PATH := $(or $(BOOST_INCLUDE_DIR),/opt/homebrew/include)

CXXFLAGS += -fsanitize=address,fuzzer -Wall -Wextra -std=c++20 \
            -I include -I . \
            -I $(BOOST_INCLUDE_PATH)

MODULES := $(wildcard modules/*/module.a)
UNAME_S := $(shell uname -s)
BITCOINFUZZ_SRC := basemodule modulelogger
BITCOINFUZZ_OBJS := $(addprefix include/bitcoinfuzz/, $(addsuffix .o, $(BITCOINFUZZ_SRC)))

# Include all module dependency files
-include modules/*/deps.mk

# Get Python linking flags if the embit module is being used (DEMBIT is defined)
PYTHON_LDFLAGS = $(shell if echo "$(CXXFLAGS)" | grep -q "\-DEMBIT"; then python3-config --ldflags --embed; fi)

ifeq ($(UNAME_S), Darwin)
	LDFLAGS = -framework CoreFoundation -Wl,-ld_classic
endif

ifneq ($(findstring -DNBITCOIN,$(BASE_CXXFLAGS) $(CXXFLAGS)),)
ifeq ($(UNAME_S), Darwin)
NBITCOIN_LIB_EXT := dylib
else
NBITCOIN_LIB_EXT := so
endif
NBITCOIN_DYLIB := ./NBitcoin.CppBridge.$(NBITCOIN_LIB_EXT)
endif

SODIUM_LDLIBS = $(shell pkg-config --silence-errors --libs libsodium 2>/dev/null)

# Check for EMBIT define and add Python-related flags
ifneq ($(findstring -DEMBIT,$(BASE_CXXFLAGS) $(CXXFLAGS)),)
  PYTHON_LDFLAGS := $(shell python3-config --ldflags --embed)
endif

# Check for LIGHTNING_KMP define and add Java-related flags
ifneq (,$(filter -DECLAIR -DLIGHTNING_KMP,$(BASE_CXXFLAGS) $(CXXFLAGS)))
	ifeq ($(UNAME_S), Darwin)
		JAVA_HOME ?= $(shell /usr/libexec/java_home)
	else
		JAVA_HOME ?= $(shell dirname $$(dirname $$(readlink -f $$(which javac))))
	endif

  JAVA_CXXFLAGS := -I$(JAVA_HOME)/include -I$(JAVA_HOME)/include/$(shell uname -s | tr '[:upper:]' '[:lower:]') -L$(JAVA_HOME)/lib/server -ljvm -Wl,-rpath,$(JAVA_HOME)/lib/server
endif

CXXFLAGS := $(BASE_CXXFLAGS) $(JAVA_CXXFLAGS) $(CXXFLAGS) $(PYTHON_LDFLAGS)

# Check for EMBIT define and add Python-related flags
ifneq ($(findstring -DEMBIT,$(BASE_CXXFLAGS) $(CXXFLAGS)),)
  PYTHON_LDFLAGS := $(shell python3-config --ldflags --embed)
endif

# Check for ECLAIR define and add Java-related flags
ifneq ($(findstring -DECLAIR,$(BASE_CXXFLAGS) $(CXXFLAGS)),)
	ifeq ($(UNAME_S), Darwin)
		JAVA_HOME ?= $(shell /usr/libexec/java_home)
	else
		JAVA_HOME ?= $(shell dirname $$(dirname $$(readlink -f $$(which javac))))
	endif
	
  JAVA_CXXFLAGS := -I$(JAVA_HOME)/include -I$(JAVA_HOME)/include/$(shell uname -s | tr '[:upper:]' '[:lower:]') -L$(JAVA_HOME)/lib/server -ljvm -Wl,-rpath,$(JAVA_HOME)/lib/server
endif

CXXFLAGS := $(BASE_CXXFLAGS) $(JAVA_CXXFLAGS) $(CXXFLAGS) $(PYTHON_LDFLAGS)

# Include module libraries from deps.mk
MODULE_LIBS = $(LIBBITCOIN_ALL_LIBS)

bitcoinfuzz: main.cpp driver.o $(BITCOINFUZZ_OBJS) $(MODULES)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) main.cpp $(MODULES) driver.o $(BITCOINFUZZ_OBJS) $(NBITCOIN_DYLIB) -o bitcoinfuzz $(PYTHON_LDFLAGS) $(SODIUM_LDLIBS) $(MODULE_LIBS)

driver.o: driver.cpp driver.h
	$(CXX) $(CXXFLAGS) -c driver.cpp -o driver.o

include/bitcoinfuzz/%.o: include/bitcoinfuzz/%.cpp include/bitcoinfuzz/%.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf *.o module.a bitcoinfuzz include/bitcoinfuzz/*.o $(MODULES)
	rm -rf modules/eclair/eclair.zip modules/eclair/lib modules/eclair/eclair_extracted

.PHONY: all bitcoinfuzz clean
