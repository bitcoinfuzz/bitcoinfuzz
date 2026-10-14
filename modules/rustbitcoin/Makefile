all: module.a

CXXFLAGS += -Wall -Wextra -O2 -std=c++20 -I ../../include
# MacOS: ./rust_bitcoin_lib/target/aarch64-apple-darwin/release/librust_bitcoin_lib.a
# An explicit --target keeps RUSTFLAGS (sanitizer/sancov) off proc-macro crates
# and build scripts, which cannot be built with sanitizers.
RUST_TARGET ?= $(shell rustc +nightly -vV | sed -n 's/^host: //p')
RUST_LIB_PATH := ./rust_bitcoin_lib/target/$(RUST_TARGET)/release/librust_bitcoin_lib.a

# Namespace this module's vendored libsecp256k1 so it can be linked alongside
# other Rust modules pinning the same secp256k1-sys version.
SYMBOL_PREFIX ?= rustbitcoin

$(RUST_LIB_PATH):
	cd rust_bitcoin_lib && \
	RUSTFLAGS="\
		-Z sanitizer=address \
		-C passes=sancov-module \
		-C llvm-args=-sanitizer-coverage-inline-8bit-counters \
		-C llvm-args=-sanitizer-coverage-trace-compares \
		-C llvm-args=-sanitizer-coverage-pc-table \
		-C llvm-args=-sanitizer-coverage-level=4 \
		-C llvm-args=-simplifycfg-branch-fold-threshold=0" \
	cargo +nightly build --release --target $(RUST_TARGET)

cargo: $(RUST_LIB_PATH)

module.a: module.o $(RUST_LIB_PATH)
	bash ../../scripts/merge.sh module.a $(RUST_LIB_PATH) module.o
	bash ../../scripts/rename_rust_secp_symbols.sh module.a $(SYMBOL_PREFIX)
	ranlib module.a

module.o: module.cpp module.h
	$(CXX) $(CXXFLAGS) -I . -fPIC -c module.cpp -o module.o

format:
	clang-format -i ./module.cpp ./module.h ./rust_bitcoin_lib/rust_bitcoin_lib.h
	cd rust_bitcoin_lib && cargo fmt

check-format:
	clang-format -Werror --fail-on-incomplete-format -n ./module.cpp ./module.h ./rust_bitcoin_lib/rust_bitcoin_lib.h
	cd rust_bitcoin_lib && cargo fmt --check

clean:
	rm -rf *.o module.a
	cd rust_bitcoin_lib && cargo clean

.PHONY: all cargo clean format check-format $(RUST_LIB_PATH)
