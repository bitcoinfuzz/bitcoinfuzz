all: module.a

CXXFLAGS += -Wall -Wextra -std=c++20 -fsanitize=address,fuzzer-no-link -O1 -I ../../include

# The checkout and its Lake build (mathlib included) run to several GB; point
# BTC_VERIFIED_DIR elsewhere to keep them out of the module tree. Only lib/ is
# needed at runtime.
BTC_VERIFIED_DIR ?= btc-verified
BTC_VERIFIED_URL ?= https://github.com/ProofOfKeags/btc-verified.git
BITCOINKERNEL_REFERENCE_DIR := ../bitcoinkernel/bitcoin

# Resolve Version Reference
DEFAULT_REF := master
BTC_VERIFIED_REF ?=
ifeq ($(strip $(BTC_VERIFIED_REF)),)
$(warning BTC_VERIFIED_REF not set. Defaulting to $(DEFAULT_REF).)
BTC_VERIFIED_REF := $(DEFAULT_REF)
endif

# Lake writes the coverage build to its own build directory, so sanitizer
# objects never replace ordinary btc-verified artifacts. The upstream header
# is authenticated against Kernel/abi.toml and always lands in .lake/build.
LEAN_KERNEL_LIB := $(BTC_VERIFIED_DIR)/.lake/conformance/verified-build/lib/libbtc_verified_kernel.so
KERNEL_INCLUDE := $(BTC_VERIFIED_DIR)/.lake/build/kernel/include

# The runtime copy: the kernel library beside the Lean runtime it needs, found
# through $ORIGIN, so lib/ works without a Lean toolchain (Docker runner, CI
# test jobs). module.cpp loads it from this absolute path.
KERNEL_LIB := $(abspath lib/libbtc_verified_kernel.so)

LAKE ?= lake
LEAN ?= lean

# The Lean library is loaded at runtime (see module.cpp), not linked, so
# module.a holds only the wrapper object.
module.a: module.o check-core-pin
	rm -f module.a
	ar rcs module.a module.o
	ranlib module.a

# Builds btc-verified's kernel exactly as its conformance harness does:
# generated Lean C, the C boundary, and btc-verified's model get libFuzzer
# coverage; the Lean runtime stays uninstrumented. The library leaves the
# SanitizerCoverage callbacks for the bitcoinfuzz executable to provide.
# The AFL++ image builds with afl-clang-fast, whose instrumentation may call
# its own __afl_ hooks instead, and which needs Lake's link response file
# unquoted (see unquote-rsp-cc.sh).
ifneq ($(findstring afl-,$(notdir $(CC))),)
LAKE_CC := $(abspath unquote-rsp-cc.sh)
else
LAKE_CC := $(CC)
endif

$(LEAN_KERNEL_LIB): btc-verified-sync
	cd $(BTC_VERIFIED_DIR) && $(LAKE) exe cache get
	cd $(BTC_VERIFIED_DIR) && CC=$(LAKE_CC) BTC_VERIFIED_REAL_CC=$(CC) LEAN_CC=$(CC) $(LAKE) -R \
		-K transactionCoverage=true \
		-K transactionCoverageCompiler=$(notdir $(CC)) \
		build kernel
	@nm -u $(LEAN_KERNEL_LIB) | grep -qE '__sanitizer_cov_|__afl_' || \
		{ echo "ERROR: $(LEAN_KERNEL_LIB) lacks coverage instrumentation."; exit 1; }

$(KERNEL_LIB): $(LEAN_KERNEL_LIB)
	mkdir -p lib
	cp $(LEAN_KERNEL_LIB) $@
	cp "$$(cd $(BTC_VERIFIED_DIR) && $(LEAN) --print-prefix)/lib/lean/libleanshared.so" lib/
	patchelf --set-rpath '$$ORIGIN' $@

# btc-verified implements the header at the Core revision pinned in
# Kernel/abi.toml. A different Core revision is comparable only while the
# transaction slice of the header and its implementation are unchanged.
check-core-pin: btc-verified-sync
	@pinned="$$(sed -n 's/^revision = "\(.*\)"/\1/p' $(BTC_VERIFIED_DIR)/Kernel/abi.toml)"; \
	echo "btc-verified implements bitcoinkernel.h at Core $$pinned"; \
	if [ -d "$(BITCOINKERNEL_REFERENCE_DIR)/.git" ]; then \
		core="$$(git -C "$(BITCOINKERNEL_REFERENCE_DIR)" rev-parse HEAD)"; \
		if [ "$$core" != "$$pinned" ]; then \
			echo "WARNING: modules/bitcoinkernel is at $$core, not $$pinned;"; \
			echo "WARNING: check that the transaction slice is unchanged, e.g."; \
			echo "WARNING:   git -C $(BITCOINKERNEL_REFERENCE_DIR) diff $$pinned $$core -- src/kernel/bitcoinkernel.h src/kernel/bitcoinkernel.cpp src/consensus/tx_check.cpp"; \
		fi; \
	fi

btc-verified-sync:
	@if [ ! -d "$(BTC_VERIFIED_DIR)/.git" ]; then \
		echo "Cloning btc-verified repository..."; \
		git clone "$(BTC_VERIFIED_URL)" "$(BTC_VERIFIED_DIR)"; \
	fi
	@current_commit="$$(git -C "$(BTC_VERIFIED_DIR)" rev-parse --verify -q HEAD 2>/dev/null || true)"; \
	if [ "$$current_commit" = "$(BTC_VERIFIED_REF)" ]; then \
		echo "btc-verified already at commit $(BTC_VERIFIED_REF); skipping fetch/checkout."; \
		exit 0; \
	fi; \
	echo "Fetching latest refs from origin..."; \
	git -C "$(BTC_VERIFIED_DIR)" fetch origin; \
	if git -C "$(BTC_VERIFIED_DIR)" rev-parse --verify -q "origin/$(BTC_VERIFIED_REF)^{commit}" >/dev/null; then \
		target="origin/$(BTC_VERIFIED_REF)"; \
	elif git -C "$(BTC_VERIFIED_DIR)" cat-file -e "$(BTC_VERIFIED_REF)^{commit}" 2>/dev/null; then \
		target="$(BTC_VERIFIED_REF)"; \
	else \
		echo "ERROR: commit $(BTC_VERIFIED_REF) not found after fetch."; \
		exit 1; \
	fi; \
	echo "Checking out $$target..."; \
	git -C "$(BTC_VERIFIED_DIR)" checkout --detach "$$target"

module.o: module.cpp module.h $(KERNEL_LIB)
	$(CXX) $(CXXFLAGS) -I $(KERNEL_INCLUDE) \
		-DBTC_VERIFIED_KERNEL_LIB='"$(KERNEL_LIB)"' -fPIC -c $< -o $@

format:
	clang-format -i module.h module.cpp

check-format:
	clang-format -Werror --fail-on-incomplete-format -n module.h module.cpp

clean:
	rm -rf *.o module.a lib $(BTC_VERIFIED_DIR)/.lake/conformance/verified-build

.PHONY: all format check-format clean btc-verified-sync check-core-pin
