all: bitcoinfuzz

CXX := clang++
BASE_CXXFLAGS := -fsanitize=address,fuzzer -Wall -Wextra -std=c++20 -I include -I .
MODULES := $(wildcard modules/*/module.a)
UNAME_S := $(shell uname -s)

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

bitcoinfuzz: main.cpp driver.o include/bitcoinfuzz/basemodule.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) main.cpp $(MODULES) driver.o include/bitcoinfuzz/basemodule.o $(NBITCOIN_DYLIB) -o bitcoinfuzz $(PYTHON_LDFLAGS) $(SODIUM_LDLIBS)

driver.o: driver.cpp driver.h
	$(CXX) $(CXXFLAGS) -c driver.cpp -o driver.o

include/bitcoinfuzz/basemodule.o: include/bitcoinfuzz/basemodule.cpp include/bitcoinfuzz/basemodule.h
	$(CXX) $(CXXFLAGS) -c include/bitcoinfuzz/basemodule.cpp -o include/bitcoinfuzz/basemodule.o

clean:
	rm -rf *.o module.a bitcoinfuzz include/bitcoinfuzz/*.o $(MODULES)
	rm -rf modules/eclair/eclair.zip modules/eclair/lib modules/eclair/eclair_extracted

.PHONY: all bitcoinfuzz