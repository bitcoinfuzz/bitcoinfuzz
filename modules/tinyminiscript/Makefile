all: module.a

CXXFLAGS += -Wall -Wextra -fsanitize=address,fuzzer -std=c++20 -I ../../include

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S), Darwin)
  TINY_LIB_EXT := dylib
else
  TINY_LIB_EXT := so
endif

TINY_MINISCRIPT_LIB_PATH := ./tiny_miniscript_lib/target/release/libtiny_miniscript_lib.so

cargo:
	cd tiny_miniscript_lib && \
	RUSTFLAGS="\
		-Z sanitizer=address \
		-C passes=sancov-module \
		-C llvm-args=-sanitizer-coverage-inline-8bit-counters \
		-C llvm-args=-sanitizer-coverage-trace-compares \
		-C llvm-args=-sanitizer-coverage-pc-table \
		-C llvm-args=-sanitizer-coverage-level=4 \
		-C llvm-args=-simplifycfg-branch-fold-threshold=0" \
	cargo +nightly build --release
	cp $(TINY_MINISCRIPT_LIB_PATH) ../../libtiny_miniscript_lib.so

module.a: module.o $(TINY_MINISCRIPT_LIB_PATH)
	ar rcs module.a module.o

module.o: module.cpp module.h
	$(CXX) $(CXXFLAGS) -I . -fPIC -c module.cpp -o module.o

clean:
	rm -rf *.o module.a
