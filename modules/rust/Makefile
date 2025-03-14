all: module.a

CXX := clang++
CXXFLAGS += -Wall -Wextra -fsanitize=address,fuzzer -std=c++20 -I ../../include

# Determine platform and set Rust target path accordingly
UNAME_S := $(shell uname -s)
UNAME_M := $(shell uname -m)

ifeq ($(UNAME_S),Darwin)
    ifeq ($(UNAME_M),arm64)
        RUST_TARGET := aarch64-apple-darwin
    else
        RUST_TARGET := x86_64-apple-darwin
    endif
    RUST_LIB_PATH := ./target/$(RUST_TARGET)/release/librust_modules_lib.a
else
    RUST_LIB_PATH := ./target/release/librust_modules_lib.a
endif

cargo:
	RUSTFLAGS="-Z sanitizer=address" cargo build --release

module.a: module.o $(RUST_LIB_PATH)
	bash ../../merge.sh module.a $(RUST_LIB_PATH) module.o
	ranlib module.a

module.o: module.cpp module.h
	$(CXX) $(CXXFLAGS) -I . -fPIC -c module.cpp -o module.o

# Ensure Rust library is built before trying to link
$(RUST_LIB_PATH): cargo

clean:
	rm -rf *.o module.a
	cargo clean