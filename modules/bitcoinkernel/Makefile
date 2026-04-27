all: module.a

CXXFLAGS += -Wall -Wextra -std=c++20 -I ../../include

BITCOIN_DIR := bitcoin
BITCOIN_URL := https://github.com/bitcoin/bitcoin.git
KERNEL_BUILD_DIR := $(BITCOIN_DIR)/build-kernel
KERNEL_LIB := $(KERNEL_BUILD_DIR)/lib/libbitcoinkernel.a

# Resolve Version Reference
DEFAULT_REF:= master
BITCOINKERNEL_REF ?=
ifeq ($(strip $(BITCOINKERNEL_REF)),)
$(warning BITCOINKERNEL_REF not set. Defaulting to $(DEFAULT_REF).)
BITCOINKERNEL_REF := $(DEFAULT_REF)
endif

module.a: $(KERNEL_LIB) module.o
	bash ../../merge.sh module.a $(KERNEL_LIB) module.o

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

bitcoin-sync:
	@if [ ! -d "$(BITCOIN_DIR)/.git" ]; then \
		echo "Cloning Bitcoin Core repository..."; \
		git clone "$(BITCOIN_URL)" "$(BITCOIN_DIR)"; \
	fi
	@current_commit="$$(git -C "$(BITCOIN_DIR)" rev-parse --verify -q HEAD 2>/dev/null || true)"; \
	if [ "$$current_commit" = "$(BITCOINKERNEL_REF)" ]; then \
		echo "Bitcoin Core already at commit $(BITCOINKERNEL_REF); skipping fetch/checkout."; \
		exit 0; \
	fi; \
	if ! git -C "$(BITCOIN_DIR)" cat-file -e "$(BITCOINKERNEL_REF)^{commit}" 2>/dev/null; then \
		echo "Fetching latest refs from origin..."; \
		git -C "$(BITCOIN_DIR)" fetch origin; \
	fi; \
	if ! git -C "$(BITCOIN_DIR)" cat-file -e "$(BITCOINKERNEL_REF)^{commit}" 2>/dev/null; then \
		echo "ERROR: commit $(BITCOINKERNEL_REF) not found after fetch."; \
		exit 1; \
	fi; \
	echo "Checking out commit $(BITCOINKERNEL_REF)..."; \
	git -C "$(BITCOIN_DIR)" checkout --detach "$(BITCOINKERNEL_REF)"

module.o: module.cpp module.h bitcoin-sync
	$(CXX) $(CXXFLAGS) -I $(BITCOIN_DIR)/src -fPIC -c $< -o $@

format:
	clang-format -i module.h module.cpp

check-format:
	clang-format -Werror --fail-on-incomplete-format -n module.h module.cpp

clean:
	rm -rf *.o module.a bitcoin/build-kernel

.PHONY: all format check-format clean bitcoin-sync
