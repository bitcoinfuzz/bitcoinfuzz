all: module.a

CXXFLAGS += -Wall -Wextra -fsanitize=address,fuzzer -std=c++20 -I ../../include
RUST_LIB_PATH := ./floresta_lib/target/release/libfloresta_lib.a

cargo:
	cd floresta_lib && \
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
	clang-format -i ./module.cpp ./module.h ./floresta_lib/floresta_lib.h
	cd floresta_lib && cargo fmt

check-format:
	clang-format -Werror --fail-on-incomplete-format -n ./module.cpp ./module.h ./floresta_lib/floresta_lib.h
	cd floresta_lib && cargo fmt --check

clean:
	rm -rf module.a module.o
	cd floresta_lib && cargo clean

.PHONY: all cargo format check-format clean
