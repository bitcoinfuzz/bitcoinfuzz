all: module.a

CXXFLAGS += -Wall -Wextra -fsanitize=address,fuzzer -std=c++20 -I ../../include
RUSTREEXO_LIB_PATH := ./rustreexo_lib/target/release/librustreexo_lib.a

$(RUSTREEXO_LIB_PATH):
	cd rustreexo_lib && \
	RUSTFLAGS="\
		-Z sanitizer=address \
		-C passes=sancov-module \
		-C llvm-args=-sanitizer-coverage-inline-8bit-counters \
		-C llvm-args=-sanitizer-coverage-trace-compares \
		-C llvm-args=-sanitizer-coverage-pc-table \
		-C llvm-args=-sanitizer-coverage-level=4 \
		-C llvm-args=-simplifycfg-branch-fold-threshold=0" \
	cargo +nightly build --release

cargo: $(RUSTREEXO_LIB_PATH)

module.a: module.o $(RUSTREEXO_LIB_PATH)
	bash ../../merge.sh module.a $(RUSTREEXO_LIB_PATH) module.o
	ranlib module.a

module.o: module.cpp module.h
	$(CXX) $(CXXFLAGS) -I . -fPIC -c module.cpp -o module.o

format:
	clang-format -i ./module.cpp ./module.h ./rustreexo_lib/rustreexo_lib.h
	cd rustreexo_lib && cargo fmt

check-format:
	clang-format -Werror --fail-on-incomplete-format -n ./module.cpp ./module.h ./rustreexo_lib/rustreexo_lib.h
	cd rustreexo_lib && cargo fmt --check

clean:
	rm -rf *.o module.a
	cd rustreexo_lib && cargo clean

.PHONY: all cargo clean format check-format $(RUSTREEXO_LIB_PATH)
