all: module.a

CXXFLAGS += -Wall -Wextra -fsanitize=address,fuzzer -std=c++20 -I ../../include

K256_LIB_PATH := ./k256_lib/target/release/libk256_lib.a

$(K256_LIB_PATH):
	cd k256_lib && \
	RUSTFLAGS="\
		-Z sanitizer=address \
		-C passes=sancov-module \
		-C llvm-args=-sanitizer-coverage-inline-8bit-counters \
		-C llvm-args=-sanitizer-coverage-trace-compares \
		-C llvm-args=-sanitizer-coverage-pc-table \
		-C llvm-args=-sanitizer-coverage-level=4 \
		-C llvm-args=-simplifycfg-branch-fold-threshold=0" \
	cargo +nightly build --release

cargo: $(K256_LIB_PATH)

module.a: module.o $(K256_LIB_PATH)
	bash ../../merge.sh module.a $(K256_LIB_PATH) module.o
	ranlib module.a

module.o: module.cpp module.h
	$(CXX) $(CXXFLAGS) -I . -fPIC -c module.cpp -o module.o

format:
	clang-format -i ./module.cpp ./module.h ./k256_lib/k256_lib.h
	cd k256_lib && cargo fmt

check-format:
	clang-format -Werror --fail-on-incomplete-format -n ./module.cpp ./module.h ./k256_lib/k256_lib.h
	cd k256_lib && cargo fmt --check

clean:
	rm -rf *.o module.a
	cd k256_lib && cargo clean

.PHONY: all cargo clean format check-format $(K256_LIB_PATH)
