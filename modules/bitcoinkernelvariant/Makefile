all: module.a

CXXFLAGS += -Wall -Wextra -std=c++20 -I ../../include

SYMBOL_PREFIX ?= variant
BITCOIN_DIR := bitcoin
BITCOIN_URL := https://github.com/bitcoin/bitcoin.git
BITCOINKERNEL_REFERENCE_DIR := ../bitcoinkernel/bitcoin
BITCOINKERNEL_WAIT_TIMEOUT_SECS ?= 60
BITCOINKERNEL_WAIT_INTERVAL_SECS ?= 2
KERNEL_BUILD_DIR := $(BITCOIN_DIR)/build-kernel
KERNEL_LIB := $(KERNEL_BUILD_DIR)/lib/libbitcoinkernel.a

# Resolve Version Reference
DEFAULT_REF:= master
BITCOINKERNEL_VARIANT_REF ?=
ifeq ($(strip $(BITCOINKERNEL_VARIANT_REF)),)
$(warning BITCOINKERNEL_VARIANT_REF not set. Defaulting to $(DEFAULT_REF).)
BITCOINKERNEL_VARIANT_REF := $(DEFAULT_REF)
endif

# Detect objcopy command
OBJCOPY := $(shell command -v objcopy 2> /dev/null)
ifeq ($(OBJCOPY),)
$(error objcopy not found. Install with: apt-get install binutils)
endif

# Map global symbols and COMDAT group signatures in libbitcoinkernel.a
variant_symbols.map: $(KERNEL_LIB)
	@echo "Generating deterministic symbol map..."
	@{ \
		nm -g --defined-only $(KERNEL_LIB) | awk '{print $$3}'; \
		nm --defined-only $(KERNEL_LIB) | awk '$$2 == "n" {print $$3}' ; \
	} | grep -Ev '^$$' | sort -u | while read sym; do \
		echo "$$sym $(SYMBOL_PREFIX)_$$sym"; \
	done > $@
	@sort -u -o $@ $@
	@echo "Generated $$(wc -l < $@) symbol mappings"

module.a: $(KERNEL_LIB) variant_symbols.map module.o
	@echo "Building module.a with symbol prefix: $(SYMBOL_PREFIX)"

	# Merge wrapper object + kernel library first.
	bash ../../merge.sh module_temp.a $(KERNEL_LIB) module.o

	# Rewrite symbols directly in the archive to preserve duplicate member names.
	@echo "Renaming symbols with $(SYMBOL_PREFIX)_ prefix..."
	$(OBJCOPY) --redefine-syms=variant_symbols.map module_temp.a module.a
	ranlib module.a

	@echo "Verifying renamed symbols (first 10):"
	@nm module.a | grep ' variant_btck_' | head -10
	# Cleanup temporary build artifacts.
	rm -rf module_temp.a

$(KERNEL_LIB): bitcoin-sync
	cd $(BITCOIN_DIR) && \
	cmake -S . -B build-kernel -G "Unix Makefiles" \
		-DCMAKE_C_COMPILER=$(CC) \
		-DCMAKE_CXX_COMPILER=$(CXX) \
		-DBUILD_KERNEL_LIB=ON \
		-DBUILD_SHARED_LIBS=OFF \
		-DENABLE_IPC=OFF \
		-DBUILD_BITCOIN_BIN=OFF \
		-DBUILD_DAEMON=OFF \
		-DBUILD_CLI=OFF \
		-DBUILD_TESTS=OFF \
		-DBUILD_TX=OFF \
		-DBUILD_UTIL=OFF \
		-DBUILD_KERNEL_TEST=OFF \
		-DENABLE_WALLET=OFF \
		-DBUILD_WALLET_TOOL=OFF \
		-DENABLE_EXTERNAL_SIGNER=OFF \
		-DSANITIZERS=fuzzer \
	cd $(BITCOIN_DIR) && \
	cmake --build build-kernel -j"$$(nproc)" --target bitcoinkernel

check-bitcoinkernel-variant-ref:
	@if [ -z "$(BITCOINKERNEL_VARIANT_REF)" ]; then \
		echo "ERROR: BITCOINKERNEL_VARIANT_REF must be set."; \
		exit 1; \
	fi

wait-bitcoin-reference-repo: check-bitcoinkernel-variant-ref
	@elapsed=0; \
	while [ ! -d "$(BITCOINKERNEL_REFERENCE_DIR)/.git" ]; do \
		if [ $$elapsed -ge "$(BITCOINKERNEL_WAIT_TIMEOUT_SECS)" ]; then \
			echo "ERROR: timed out waiting for reference repository at $(BITCOINKERNEL_REFERENCE_DIR) after $(BITCOINKERNEL_WAIT_TIMEOUT_SECS)s."; \
			exit 1; \
		fi; \
		echo "Waiting for reference repository at $(BITCOINKERNEL_REFERENCE_DIR) ($$elapsed/$(BITCOINKERNEL_WAIT_TIMEOUT_SECS)s)..."; \
		sleep "$(BITCOINKERNEL_WAIT_INTERVAL_SECS)"; \
		elapsed=$$((elapsed + $(BITCOINKERNEL_WAIT_INTERVAL_SECS))); \
	done

bitcoin-sync: wait-bitcoin-reference-repo
	@if [ ! -d "$(BITCOIN_DIR)/.git" ]; then \
		echo "Cloning Bitcoin Core repository with reference $(BITCOINKERNEL_REFERENCE_DIR)..."; \
		git clone --reference "$(BITCOINKERNEL_REFERENCE_DIR)" "$(BITCOIN_URL)" "$(BITCOIN_DIR)"; \
	fi
	@current_commit="$$(git -C "$(BITCOIN_DIR)" rev-parse --verify -q HEAD 2>/dev/null || true)"; \
	if [ "$$current_commit" = "$(BITCOINKERNEL_VARIANT_REF)" ]; then \
		echo "Bitcoin Core already at commit $(BITCOINKERNEL_VARIANT_REF); skipping fetch/checkout."; \
		exit 0; \
	fi; \
	if ! git -C "$(BITCOIN_DIR)" cat-file -e "$(BITCOINKERNEL_VARIANT_REF)^{commit}" 2>/dev/null; then \
		echo "Fetching latest refs from origin..."; \
		git -C "$(BITCOIN_DIR)" fetch origin; \
	fi; \
	if ! git -C "$(BITCOIN_DIR)" cat-file -e "$(BITCOINKERNEL_VARIANT_REF)^{commit}" 2>/dev/null; then \
		echo "ERROR: commit $(BITCOINKERNEL_VARIANT_REF) not found after fetch."; \
		exit 1; \
	fi; \
	echo "Checking out commit $(BITCOINKERNEL_VARIANT_REF)..."; \
	git -C "$(BITCOIN_DIR)" checkout --detach "$(BITCOINKERNEL_VARIANT_REF)"

module.o: module.cpp module.h \
	bitcoinkernel_variant_symbol_prefix.h
	$(CXX) $(CXXFLAGS) -I $(BITCOIN_DIR)/src -fPIC -c module.cpp -o module.o

format:
	clang-format -i module.h module.cpp

check-format:
	clang-format -Werror --fail-on-incomplete-format -n module.h module.cpp

clean:
	rm -rf *.o module.a module_temp.a tmp_obj variant_symbols.map bitcoin/build-kernel

.PHONY: all format check-format clean bitcoin-sync check-bitcoinkernel-variant-ref wait-bitcoinkernel-base-module
