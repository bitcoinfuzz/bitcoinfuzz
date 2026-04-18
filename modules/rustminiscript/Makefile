all: module.a

CXXFLAGS += -Wall -Wextra -fsanitize=address,fuzzer -std=c++20 -I ../../include
# MacOS: ./rust_miniscript_lib/target/aarch64-apple-darwin/release/librust_bitcoin_lib.a
RUST_BUILD_DIR := ./build
ifneq ($(strip $(RUST_MINISCRIPT_REF)),)
RUST_MINISCRIPT_RESOLVED_REF := $(strip $(RUST_MINISCRIPT_REF))
else ifneq ($(strip $(RUST_MINISCRIPT_PR)),)
RUST_MINISCRIPT_RESOLVED_REF := refs/pull/$(strip $(RUST_MINISCRIPT_PR))/head
else ifneq ($(strip $(COMMIT)),)
RUST_MINISCRIPT_RESOLVED_REF := $(strip $(COMMIT))
else ifneq ($(strip $(PR)),)
RUST_MINISCRIPT_RESOLVED_REF := refs/pull/$(strip $(PR))/head
else
RUST_MINISCRIPT_RESOLVED_REF :=
endif
RUST_REF_KEY := $(if $(RUST_MINISCRIPT_RESOLVED_REF),$(subst /,_,$(RUST_MINISCRIPT_RESOLVED_REF)),default)
RUST_BUILD_INSTANCE ?= $(shell printf '%s' "$${CI_JOB_ID:-shared}" | tr -c '[:alnum:]_.-' '_')
RUST_TARGET_SUBDIR := target/$(RUST_BUILD_INSTANCE)/$(RUST_REF_KEY)
RUST_MINISCRIPT_LIB_PATH := $(RUST_BUILD_DIR)/$(RUST_TARGET_SUBDIR)/release/librust_miniscript_lib.a
RUST_WORKSPACE_MANIFEST := $(RUST_BUILD_DIR)/Cargo.toml
RUST_CARGO_CONFIG := $(RUST_BUILD_DIR)/.cargo/config.toml
RUST_WORKSPACE_CRATE_DIR := $(RUST_BUILD_DIR)/rust_miniscript_lib
RUST_MANIFEST_PATH := rust_miniscript_lib/Cargo.toml
RUST_MANIFEST_GENERATOR := ../../scripts/generate_rust_workspace_manifest.py

$(RUST_MINISCRIPT_LIB_PATH):
	@set -eu; \
	resolved_ref="$(RUST_MINISCRIPT_RESOLVED_REF)"; \
	echo "Building rustminiscript module (override ref: $${resolved_ref:-<default pinned refs>})"; \
	rm -rf "$(RUST_WORKSPACE_CRATE_DIR)"; \
	mkdir -p "$(RUST_BUILD_DIR)"; \
	cp -a rust_miniscript_lib "$(RUST_WORKSPACE_CRATE_DIR)"; \
	mkdir -p "$(RUST_BUILD_DIR)/.cargo"; \
	config_path="$(RUST_CARGO_CONFIG)"; \
	config_tmp=$$(mktemp); \
	printf '[net]\n' > "$$config_tmp"; \
	printf 'git-fetch-with-cli = true\n' >> "$$config_tmp"; \
	if [ ! -f "$$config_path" ] || ! cmp -s "$$config_tmp" "$$config_path"; then \
		mv "$$config_tmp" "$$config_path"; \
	else \
		rm -f "$$config_tmp"; \
	fi; \
	manifest_args=""; \
	if [ -n "$$resolved_ref" ]; then \
		manifest_args="--resolved-ref $$resolved_ref"; \
	fi; \
	python3 "$(RUST_MANIFEST_GENERATOR)" \
		--source-manifest "$(RUST_MANIFEST_PATH)" \
		--workspace-member rust_miniscript_lib \
		--output "$(RUST_WORKSPACE_MANIFEST)" \
		--patch-root miniscript \
		$$manifest_args; \
	if ! (cd "$(RUST_BUILD_DIR)" && \
	RUSTFLAGS="\
		-Z sanitizer=address \
		-C passes=sancov-module \
		-C llvm-args=-sanitizer-coverage-inline-8bit-counters \
		-C llvm-args=-sanitizer-coverage-trace-compares \
		-C llvm-args=-sanitizer-coverage-pc-table \
		-C llvm-args=-sanitizer-coverage-level=4 \
		-C llvm-args=-simplifycfg-branch-fold-threshold=0" \
	CARGO_TARGET_DIR="./$(RUST_TARGET_SUBDIR)" cargo +nightly build --release); then \
		echo "ERROR: rustminiscript build failed for ref '$${resolved_ref:-<default pinned refs>}'"; \
		exit 1; \
	fi; \
	if command -v nm >/dev/null 2>&1; then \
		if ! nm -g --defined-only "$(RUST_MINISCRIPT_LIB_PATH)" 2>/dev/null | grep -Eq '(^|[[:space:]])_?rust_miniscript_descriptor_parse$$'; then \
			echo "ERROR: rustminiscript staticlib is missing expected symbol rust_miniscript_descriptor_parse"; \
			exit 1; \
		fi; \
	else \
		echo "WARNING: nm not found; skipping rustminiscript symbol validation"; \
	fi

cargo: $(RUST_MINISCRIPT_LIB_PATH)

module.a: FORCE module.o $(RUST_MINISCRIPT_LIB_PATH)
	bash ../../merge.sh module.a $(RUST_MINISCRIPT_LIB_PATH) module.o
	ranlib module.a

FORCE:

module.o: module.cpp module.h
	$(CXX) $(CXXFLAGS) -I . -fPIC -c module.cpp -o module.o

format:
	clang-format -i ./module.cpp ./module.h ./rust_miniscript_lib/rust_miniscript_lib.h
	cd rust_miniscript_lib && cargo fmt

check-format:
	clang-format -Werror --fail-on-incomplete-format -n ./module.cpp ./module.h ./rust_miniscript_lib/rust_miniscript_lib.h
	cd rust_miniscript_lib && cargo fmt --check

clean:
	# The temporary build directory is intentionally kept after failed builds for debugging.
	# Run `make clean` to remove all temporary module build state.
	rm -rf *.o module.a $(RUST_BUILD_DIR)

.PHONY: all cargo clean format check-format FORCE $(RUST_MINISCRIPT_LIB_PATH)
