all: module.a

CXXFLAGS += -Wall -Wextra -O2 -std=c++20 -I ../../include
RUST_LIB_PATH := ./rustbitcoinkernel_lib/target/release/librustbitcoinkernel_lib.a

SYMBOL_PREFIX ?= rustbitcoinkernel

# extern "C" functions module.cpp calls directly across the FFI boundary
# (declared in rustbitcoinkernel_lib.h); these must keep their original
# names, everything else in the archive gets prefixed.
FFI_EXPORTS := rustbitcoinkernel_block rustbitcoinkernel_transaction rustbitcoinkernel_block_check

# Detect objcopy command
OBJCOPY := $(shell command -v objcopy 2> /dev/null)
ifeq ($(OBJCOPY),)
$(error objcopy not found. Install with: apt-get install binutils)
endif

$(RUST_LIB_PATH):
	cd rustbitcoinkernel_lib && \
	CFLAGS="-fsanitize=address,fuzzer-no-link -ftrivial-auto-var-init=pattern" \
    CXXFLAGS="-fsanitize=address,fuzzer-no-link -ftrivial-auto-var-init=pattern" \
    LDFLAGS="-fsanitize=address,fuzzer-no-link" \
	RUSTFLAGS="\
		-Z sanitizer=address \
		-C passes=sancov-module \
		-C llvm-args=-sanitizer-coverage-inline-8bit-counters \
		-C llvm-args=-sanitizer-coverage-trace-compares \
		-C llvm-args=-sanitizer-coverage-pc-table \
		-C llvm-args=-sanitizer-coverage-level=4 \
		-C llvm-args=-simplifycfg-branch-fold-threshold=0" \
	cargo +nightly build --release

cargo: $(RUST_LIB_PATH)

# The Rust crate statically bundles a full copy of libbitcoinkernel (secp256k1
# included), which defines plain, non-weak globals like G_TRANSLATION_FUN.
# When this module is linked alongside other modules that embed their own
# copy of the same Bitcoin Core code (bitcoin, secp256k1, bitcoinkernel, ...),
# those definitions collide at link time. Prefix every symbol in the archive
# except the FFI boundary module.cpp calls, same pattern as
# modules/bitcoinkernelvariant/Makefile.
rustbitcoinkernel_symbols.map: $(RUST_LIB_PATH)
	@echo "Generating symbol map for $(SYMBOL_PREFIX)..."
	@{ \
		nm -g --defined-only $(RUST_LIB_PATH) | awk '{print $$3}'; \
		nm --defined-only $(RUST_LIB_PATH) | awk '$$2 == "n" {print $$3}'; \
	} | grep -Ev '^$$' | sort -u | while read sym; do \
		skip=0; \
		for f in $(FFI_EXPORTS); do [ "$$sym" = "$$f" ] && skip=1 && break; done; \
		[ "$$skip" -eq 0 ] && echo "$$sym $(SYMBOL_PREFIX)_$$sym"; \
	done > $@
	@sort -u -o $@ $@
	@echo "Generated $$(wc -l < $@) symbol mappings"

module.a: module.o $(RUST_LIB_PATH) rustbitcoinkernel_symbols.map
	bash ../../scripts/merge.sh module_temp.a $(RUST_LIB_PATH) module.o
	$(OBJCOPY) --redefine-syms=rustbitcoinkernel_symbols.map module_temp.a module.a
	rm -f module_temp.a
	ranlib module.a

module.o: module.cpp module.h
	$(CXX) $(CXXFLAGS) -I . -fPIC -c module.cpp -o module.o

format:
	clang-format -i ./module.cpp ./module.h ./rustbitcoinkernel_lib/rustbitcoinkernel_lib.h
	cd rustbitcoinkernel_lib && cargo fmt

check-format:
	clang-format -Werror --fail-on-incomplete-format -n ./module.cpp ./module.h ./rustbitcoinkernel_lib/rustbitcoinkernel_lib.h
	cd rustbitcoinkernel_lib && cargo fmt --check

clean:
	rm -rf *.o module.a module_temp.a rustbitcoinkernel_symbols.map
	cd rustbitcoinkernel_lib && cargo clean

.PHONY: all cargo clean format check-format $(RUST_LIB_PATH)
