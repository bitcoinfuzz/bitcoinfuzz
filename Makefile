all: bitcoinfuzz

CXX := clang++
CXXFLAGS += -fsanitize=address,fuzzer -Wall -Wextra -std=c++20 -I include -I . -Lmodules/ldk/ldk_lib -lldk_lib
MODULES := $(wildcard modules/*/module.a)
UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),Darwin)
LDFLAGS = -framework CoreFoundation -Wl,-ld_classic
endif

bitcoinfuzz: modules/ldk/ldk_lib/libldk_lib.a main.cpp driver.o include/bitcoinfuzz/basemodule.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) main.cpp $(MODULES) driver.o include/bitcoinfuzz/basemodule.o -o bitcoinfuzz

driver.o: driver.cpp driver.h
	$(CXX) $(CXXFLAGS) -c driver.cpp -o driver.o

include/bitcoinfuzz/basemodule.o: include/bitcoinfuzz/basemodule.cpp include/bitcoinfuzz/basemodule.h
	$(CXX) $(CXXFLAGS) -c include/bitcoinfuzz/basemodule.cpp -o include/bitcoinfuzz/basemodule.o

modules/ldk/ldk_lib/libldk_lib.a:
	cd modules/ldk/ldk_lib && cargo build --release
	cp modules/ldk/ldk_lib/target/release/libldk_lib.a modules/ldk/ldk_lib/

clean:
	rm -rf *.o module.a bitcoinfuzz include/bitcoinfuzz/*.o $(MODULES) modules/ldk/ldk_lib/libldk_lib.a