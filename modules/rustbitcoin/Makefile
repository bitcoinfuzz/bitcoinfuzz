all: module.a

CXXFLAGS += -Wall -Wextra -fsanitize=address,fuzzer -std=c++20 -I ../../include
# MacOS: ./rust_bitcoin_lib/target/aarch64-apple-darwin/release/librust_bitcoin_lib.a
RUST_LIB_PATH := ./rust_bitcoin_lib/target/release/librust_bitcoin_lib.a

cargo:
	cd rust_bitcoin_lib && \
	RUSTFLAGS="\
		-Z sanitizer=address \
		-C passes=sancov-module \
		-C llvm-args=-sanitizer-coverage-inline-8bit-counters \
		-C llvm-args=-sanitizer-coverage-trace-compares \
		-C llvm-args=-sanitizer-coverage-pc-table \
		-C llvm-args=-sanitizer-coverage-level=4 \
		-C llvm-args=-simplifycfg-branch-fold-threshold=0" \
	cargo +nightly build --release

module.a: module.o $(RUST_LIB_PATH)
	bash ../../merge.sh module.a $(RUST_LIB_PATH) module.o
	ranlib module.a

module.o: module.cpp module.h
	$(CXX) $(CXXFLAGS) -I . -fPIC -c module.cpp -o module.o

format:
	clang-format -i ./module.cpp ./module.h ./rust_bitcoin_lib/rust_bitcoin_lib.h
	cd rust_bitcoin_lib && cargo fmt

clean:
	rm -rf *.o module.a
