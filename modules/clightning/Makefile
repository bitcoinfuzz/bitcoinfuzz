CXXFLAGS += -Wall -Wextra -std=c++20 -fPIC
INCLUDE_DIRS += -I.
INCLUDE_DIRS += -I../../include
INCLUDE_DIRS += -I../../external/lightning
INCLUDE_DIRS += -I../../external/lightning/ccan
INCLUDE_DIRS += -I../../external/lightning/external/libwally-core/src/secp256k1/include
INCLUDE_DIRS += -I../../external/lightning/external/libwally-core/include
INCLUDE_DIRS += -I../../external/lightning/external/libsodium/src/libsodium/include
INCLUDE_DIRS += -I../../external/lightning/hsmd

CXXFLAGS += $(INCLUDE_DIRS)

# Set default prefix, but allow user to override
SYMBOL_PREFIX ?= clightning

# Detect objcopy command
OBJCOPY := $(shell command -v objcopy 2> /dev/null)
ifeq ($(OBJCOPY),)
$(error objcopy not found. Install with: apt-get install binutils)
endif

ROOT_DIR := $(realpath $(dir $(lastword $(MAKEFILE_LIST)))/../..)
LIGHTNING_DIR := $(ROOT_DIR)/external/lightning
CLIGHTNING_MAKEFILE := ./Makefile-clightning
LIBCLIGHTNING_WRAPPER := ./libcln.a

all: module.a

$(LIGHTNING_DIR)/config.vars:
	@echo "Running ./configure in $(LIGHTNING_DIR)..."
	cd $(LIGHTNING_DIR) && ./configure --enable-address-sanitizer --disable-compat --enable-fuzzing --disable-valgrind --enable-debugbuild --enable-ub-sanitizer CWARNFLAGS="-Wall -Wundef -Wmissing-prototypes -Wmissing-declarations -Wstrict-prototypes -Wold-style-definition"

libcln.a: $(LIGHTNING_DIR)/config.vars
	@echo "Appending Makefile-clightning to $(LIGHTNING_DIR)/Makefile if not already present..."
	@marker="$$(head -n 2 $(CLIGHTNING_MAKEFILE) | tail -n 1)"; \
	if ! grep -Fxq "$$marker" $(LIGHTNING_DIR)/Makefile; then \
		echo "Marker not found, appending..."; \
		awk '{ print }' $(CLIGHTNING_MAKEFILE) >> $(LIGHTNING_DIR)/Makefile; \
	else \
		echo "Makefile already includes clightning makefile content."; \
	fi
	@echo "Building lightning in $(LIGHTNING_DIR)..."
	cd $(LIGHTNING_DIR) && make libcln.a
	@echo "Copying static library in $(LIGHTNING_DIR)..."
	cp $(LIGHTNING_DIR)/libcln.a $(LIBCLIGHTNING_WRAPPER)

# Generate symbol redefine map for secp256k1 symbols only
secp256k1_symbols.map: libcln.a
	@echo "Generating symbol map for ALL secp256k1 symbols..."
	@nm libcln.a | awk '/ [TDBR] / && /secp256k1/ {print $$3}' | sort -u | while read sym; do \
		echo "$$sym $(SYMBOL_PREFIX)_$$sym"; \
	done > $@
	@echo "Generated $$(wc -l < $@) symbol mappings"

module.a: libcln.a secp256k1_symbols.map module.o
	@echo "========================================"
	@echo "Building module.a with symbol prefix: $(SYMBOL_PREFIX)"
	@echo "========================================"

	# First merge using the existing script
	bash ../../merge.sh module_temp.a libcln.a module.o

	# Apply selective symbol renaming (only secp256k1 symbols)
	@echo "Extracting objects..."
	mkdir -p tmp_obj
	cd tmp_obj && ar x ../module_temp.a

	@echo "Renaming secp256k1 symbols with $(SYMBOL_PREFIX)_ prefix..."
	cd tmp_obj && for obj in *.o; do \
		$(OBJCOPY) --redefine-syms=../secp256k1_symbols.map $$obj 2>/dev/null || true; \
	done

	# Create final archive
	@echo "Creating final archive..."
	ar rcs module.a tmp_obj/*.o
	ranlib module.a

	# Cleanup
	rm -rf tmp_obj module_temp.a
	@echo ""
	@echo "✓ module.a built successfully with prefix: $(SYMBOL_PREFIX)"
	@echo "Verifying renamed symbols (first 5):"
	@nm module.a | grep secp256k1 | head -5

module.o: module.cpp module.h
	$(CXX) $(CXXFLAGS) -fPIC -c module.cpp -o module.o

format:
	clang-format -i ./module.cpp ./module.h

check-format:
	clang-format -Werror --fail-on-incomplete-format -n ./module.cpp ./module.h

clean:
	rm -f *.o *.a module_temp.a secp256k1_symbols.map
	rm -rf tmp_obj
	$(MAKE) -C $(LIGHTNING_DIR) clean || true

.PHONY: all format check-format clean
