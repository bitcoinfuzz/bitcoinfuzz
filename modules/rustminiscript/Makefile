all: module.a

CXXFLAGS += -Wall -Wextra -fsanitize=address,fuzzer -std=c++20 -I ../../include
# MacOS: ./rust_miniscript_lib/target/aarch64-apple-darwin/release/librust_bitcoin_lib.a
RUST_MINISCRIPT_LIB_PATH := ./rust_miniscript_lib/target/release/librust_miniscript_lib.a

cargo:
	cd rust_miniscript_lib && RUSTFLAGS="-Z sanitizer=address" cargo build --release

module.a: module.o $(RUST_MINISCRIPT_LIB_PATH)
	bash ../../merge.sh module.a $(RUST_MINISCRIPT_LIB_PATH) module.o
	ranlib module.a

module.o: module.cpp module.h
	$(CXX) $(CXXFLAGS) -I . -fPIC -c module.cpp -o module.o

clean:
	rm -rf *.o module.a
