all: bitcoinfuzz

BASE_CXXFLAGS := -fsanitize=address,fuzzer -Wall -Wextra -std=c++20 -I include -I .
UNAME_S := $(shell uname -s)
BITCOINFUZZ_SRC := basemodule modulelogger
BITCOINFUZZ_OBJS := $(addprefix include/bitcoinfuzz/, $(addsuffix .o, $(BITCOINFUZZ_SRC)))
HELPERS_SRC := jvmloader
HELPERS_OBJS := $(addprefix helpers/, $(addsuffix .o, $(HELPERS_SRC)))

BITCOINFUZZ_DIR = $(shell pwd)
CXXFLAGS += -DBITCOINFUZZ_DIR=\"$(BITCOINFUZZ_DIR)\"

# Conditionally include module.a files based on compilation flags
MODULES :=
ifneq ($(findstring -DBITCOIN_CORE,$(BASE_CXXFLAGS) $(CXXFLAGS)),)
	MODULES += modules/bitcoin/module.a
endif

ifneq ($(findstring -DRUST_BITCOIN,$(BASE_CXXFLAGS) $(CXXFLAGS)),)
	MODULES += modules/rustbitcoin/module.a
endif

ifneq ($(findstring -DRUST_MINISCRIPT,$(BASE_CXXFLAGS) $(CXXFLAGS)),)
	MODULES += modules/rustminiscript/module.a
endif

ifneq ($(findstring -DTINY_MINISCRIPT,$(BASE_CXXFLAGS) $(CXXFLAGS)),)
	MODULES += modules/tinyminiscript/module.a
endif

ifneq ($(findstring -DBTCD,$(BASE_CXXFLAGS) $(CXXFLAGS)),)
	MODULES += modules/btcd/module.a
endif

ifneq ($(findstring -DNBITCOIN,$(BASE_CXXFLAGS) $(CXXFLAGS)),)
	MODULES += modules/nbitcoin/module.a
endif

ifneq ($(findstring -DLND,$(BASE_CXXFLAGS) $(CXXFLAGS)),)
	MODULES += modules/lnd/module.a
endif

ifneq ($(findstring -DLDK,$(BASE_CXXFLAGS) $(CXXFLAGS)),)
	MODULES += modules/ldk/module.a
endif

ifneq ($(findstring -DECLAIR,$(BASE_CXXFLAGS) $(CXXFLAGS)),)
	MODULES += modules/eclair/module.a
endif

ifneq ($(findstring -DNLIGHTNING,$(BASE_CXXFLAGS) $(CXXFLAGS)),)
	MODULES += modules/nlightning/module.a
endif

ifneq ($(findstring -DEMBIT,$(BASE_CXXFLAGS) $(CXXFLAGS)),)
	MODULES += modules/embit/module.a
endif

ifneq ($(findstring -DPYBITCOINKERNEL,$(BASE_CXXFLAGS) $(CXXFLAGS)),)
	MODULES += modules/pybitcoinkernel/module.a
endif

ifneq ($(findstring -DRUSTBITCOINKERNEL,$(BASE_CXXFLAGS) $(CXXFLAGS)),)
	MODULES += modules/rustbitcoinkernel/module.a
endif

ifneq ($(findstring -DCLIGHTNING,$(BASE_CXXFLAGS) $(CXXFLAGS)),)
	MODULES += modules/clightning/module.a
endif

ifneq ($(findstring -DLIGHTNING_KMP,$(BASE_CXXFLAGS) $(CXXFLAGS)),)
	MODULES += modules/lightningkmp/module.a
endif

ifneq ($(findstring -DBITCOINJ,$(BASE_CXXFLAGS) $(CXXFLAGS)),)
	MODULES += modules/bitcoinj/module.a
endif

# Add custom mutator module
ifneq (,$(filter -DCUSTOM_MUTATOR%,$(BASE_CXXFLAGS) $(CXXFLAGS)))
	MODULES += custommutator/module.a
endif

ifeq ($(UNAME_S), Darwin)
	LDFLAGS = -framework CoreFoundation -Wl,-ld_classic
endif

ifeq ($(UNAME_S), Darwin)
	LIB_EXT := dylib
else
	LIB_EXT := so
endif

ifneq ($(findstring -DNBITCOIN,$(BASE_CXXFLAGS) $(CXXFLAGS)),)
  NBITCOIN_LIB := ./NBitcoin.CppBridge.$(LIB_EXT)
endif

ifneq ($(findstring -DNLIGHTNING,$(BASE_CXXFLAGS) $(CXXFLAGS)),)
  NLIGHTNING_LIB := ./NLightning.CppBridge.$(LIB_EXT)
endif

ifneq ($(findstring -DTINY_MINISCRIPT,$(BASE_CXXFLAGS) $(CXXFLAGS)),)
  TINY_MINISCRIPT_LIB := ./libtiny_miniscript_lib.$(LIB_EXT)
endif

SODIUM_LDLIBS = $(shell pkg-config --silence-errors --libs libsodium 2>/dev/null)

# Check for EMBIT define and add Python-related flags
ifneq ($(findstring -DEMBIT,$(BASE_CXXFLAGS) $(CXXFLAGS)),)
  PYTHON_LDFLAGS := $(shell python3-config --ldflags --embed)
endif

ifneq ($(findstring -DPYBITCOINKERNEL,$(BASE_CXXFLAGS) $(CXXFLAGS)),)
  PYTHON_LDFLAGS := $(shell python3-config --ldflags --embed)
endif

# Check for LIGHTNING_KMP define and add Java-related flags
ifneq (,$(filter -DECLAIR -DLIGHTNING_KMP -DBITCOINJ,$(BASE_CXXFLAGS) $(CXXFLAGS)))
	ifeq ($(UNAME_S), Darwin)
		JAVA_HOME ?= $(shell /usr/libexec/java_home)
	else
		JAVA_HOME ?= $(shell dirname $$(dirname $$(readlink -f $$(which javac))))
	endif

  JAVA_CXXFLAGS := -I$(JAVA_HOME)/include -I$(JAVA_HOME)/include/$(shell uname -s | tr '[:upper:]' '[:lower:]') -L$(JAVA_HOME)/lib/server -ljvm -Wl,-rpath,$(JAVA_HOME)/lib/server
	JVM_LOADER := helpers/jvmloader.o
endif

CXXFLAGS := $(BASE_CXXFLAGS) $(JAVA_CXXFLAGS) $(CXXFLAGS) $(PYTHON_LDFLAGS)

bitcoinfuzz: main.cpp driver.o $(BITCOINFUZZ_OBJS) $(JVM_LOADER)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) main.cpp $(MODULES) driver.o $(BITCOINFUZZ_OBJS) $(NBITCOIN_LIB) $(NLIGHTNING_LIB) $(TINY_MINISCRIPT_LIB) -o bitcoinfuzz $(PYTHON_LDFLAGS) $(SODIUM_LDLIBS) $(JVM_LOADER)

driver.o: driver.cpp driver.h
	$(CXX) $(CXXFLAGS) -c driver.cpp -o driver.o

include/bitcoinfuzz/%.o: include/bitcoinfuzz/%.cpp include/bitcoinfuzz/%.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

helpers/%.o: helpers/%.cpp helpers/%.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

format:
	clang-format -i include/bitcoinfuzz/*.h include/bitcoinfuzz/*.cpp driver.cpp driver.h main.cpp helpers/*.cpp helpers/*.h

format-all: format
	$(MAKE) -C custommutator format
	@for dir in modules/*/; do $(MAKE) -C $$dir format; done

check-format:
	clang-format -Werror --fail-on-incomplete-format -n include/bitcoinfuzz/*.h include/bitcoinfuzz/*.cpp driver.cpp driver.h main.cpp helpers/*.cpp helpers/*.h

check-format-all:
	@EXIT_CODE=0; \
	$(MAKE) check-format || EXIT_CODE=1; \
	for dir in modules/*/; do \
		$(MAKE) -C $$dir check-format || EXIT_CODE=1; \
	done; \
	exit $$EXIT_CODE

clean:
	rm -rf *.o module.a bitcoinfuzz include/bitcoinfuzz/*.o helpers/*.o $(MODULES)
	rm -rf modules/eclair/eclair.zip modules/eclair/lib modules/eclair/eclair_extracted


.PHONY: all bitcoinfuzz
