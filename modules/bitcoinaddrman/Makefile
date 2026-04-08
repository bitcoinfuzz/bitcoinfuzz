all: module.a

CXXFLAGS += -Wall -Wextra -fsanitize=address,fuzzer -std=c++20 -I ../../include
BITCOIN_ADDRMAN_LIB_PATH := ./bitcoin_addrman_lib/target/release/libbitcoin_addrman_lib.a

$(BITCOIN_ADDRMAN_LIB_PATH):
	cd bitcoin_addrman_lib && \
	RUSTFLAGS="\
		-Z sanitizer=address \
		-C passes=sancov-module \
		-C llvm-args=-sanitizer-coverage-inline-8bit-counters \
		-C llvm-args=-sanitizer-coverage-trace-compares \
		-C llvm-args=-sanitizer-coverage-pc-table \
		-C llvm-args=-sanitizer-coverage-level=4 \
		-C llvm-args=-simplifycfg-branch-fold-threshold=0" \
	cargo +nightly build --release

cargo: $(BITCOIN_ADDRMAN_LIB_PATH)

module.a: module.o $(BITCOIN_ADDRMAN_LIB_PATH)
	bash ../../merge.sh module.a $(BITCOIN_ADDRMAN_LIB_PATH) module.o
	ranlib module.a

module.o: module.cpp module.h
	$(CXX) $(CXXFLAGS) -I . -fPIC -c module.cpp -o module.o

format:
	clang-format -i ./module.cpp ./module.h ./bitcoin_addrman_lib/bitcoin_addrman_lib.h
	cd bitcoin_addrman_lib && cargo fmt

check-format:
	clang-format -Werror --fail-on-incomplete-format -n ./module.cpp ./module.h ./bitcoin_addrman_lib/bitcoin_addrman_lib.h
	cd bitcoin_addrman_lib && cargo fmt --check

clean:
	rm -rf *.o module.a
	cd bitcoin_addrman_lib && cargo clean

.PHONY: all cargo clean format check-format $(BITCOIN_ADDRMAN_LIB_PATH)
