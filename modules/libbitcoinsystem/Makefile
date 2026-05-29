all: module.a

LIB_DIR         = ../../external/libbitcoin-system
SECP_SRC        = ../../external/secp256k1
SECP_INSTALL    = $(abspath $(LIB_DIR)/build_secp_install)
LIBBITCOIN_LIB  = $(LIB_DIR)/build_fuzz/libbitcoin-system.a
SECP_STATIC_LIB = $(SECP_INSTALL)/lib/libsecp256k1.a

BOOST_ROOT ?= /usr
NPROC := $(shell nproc)

# Detect objcopy command (required for symbol remapping to avoid secp256k1
# conflicts when linking with the bitcoin module).
OBJCOPY := $(shell command -v objcopy 2> /dev/null)
ifeq ($(OBJCOPY),)
$(error objcopy not found. Install with: apt-get install binutils)
endif

SYMBOL_PREFIX ?= libbitcoin_system

CXX ?= c++

CLANG_WARNING_FLAGS = -Wno-deprecated-literal-operator -Wno-macro-redefined

CXXFLAGS += $(CLANG_WARNING_FLAGS) \
            -Wall -Wextra -std=c++20 \
            -fsanitize=address,fuzzer-no-link \
            -DNDEBUG \
            -I . \
            -I ../../include \
            -I $(LIB_DIR)/include \
            -I $(SECP_INSTALL)/include \
            -I $(BOOST_ROOT)/include

$(SECP_STATIC_LIB):
	@echo "Building secp256k1 (for libbitcoin-system)..."
	cmake -B $(LIB_DIR)/build_secp \
	  -S $(abspath $(SECP_SRC)) \
	  -DCMAKE_C_COMPILER="$(CC)" \
	  -DCMAKE_CXX_COMPILER="$(CXX)" \
	  -DBUILD_SHARED_LIBS=OFF \
	  -DCMAKE_BUILD_TYPE=Release \
	  -DCMAKE_INSTALL_PREFIX=$(SECP_INSTALL) \
	  -DCMAKE_C_FLAGS="-fsanitize=address,fuzzer-no-link" \
	  -DSECP256K1_BUILD_TESTS=OFF \
	  -DSECP256K1_BUILD_EXAMPLES=OFF \
	  -DSECP256K1_ENABLE_MODULE_RECOVERY=ON \
	  -DSECP256K1_ENABLE_MODULE_EXTRAKEYS=ON \
	  -DSECP256K1_ENABLE_MODULE_SCHNORRSIG=ON \
	  -DSECP256K1_ENABLE_MODULE_ECDH=ON
	cmake --build $(LIB_DIR)/build_secp --target install --parallel $(NPROC)

$(LIBBITCOIN_LIB): $(SECP_STATIC_LIB)
	@echo "Building libbitcoin-system..."
	cmake -B $(LIB_DIR)/build_fuzz \
	  -S $(abspath $(LIB_DIR)/builds/cmake) \
	  -DCMAKE_C_COMPILER="$(CC)" \
	  -DCMAKE_CXX_COMPILER="$(CXX)" \
	  -DCMAKE_BUILD_TYPE=Release \
	  -DBUILD_SHARED_LIBS=OFF \
	  -DCMAKE_CXX_FLAGS="-fsanitize=address,fuzzer-no-link -O1 $(CLANG_WARNING_FLAGS)" \
	  -DCMAKE_C_FLAGS="-fsanitize=address,fuzzer-no-link -O1" \
	  -Dlibsecp256k1_DIR=$(SECP_INSTALL)/lib/cmake/libsecp256k1 \
	  "-DCMAKE_PREFIX_PATH=$(SECP_INSTALL);$(BOOST_ROOT)"
	cmake --build $(LIB_DIR)/build_fuzz --target libbitcoin-system --parallel $(NPROC)
	@test -f $(LIBBITCOIN_LIB) || { echo "ERROR: $(LIBBITCOIN_LIB) not built"; exit 1; }

module.o: module.cpp module.h
	$(CXX) $(CXXFLAGS) -fPIC -c module.cpp -o module.o

# Generate symbol redefine map: prefix every global symbol defined in
# libsecp256k1.a (not just secp256k1_*) so they don't collide with bitcoin
# core's secp256k1 when both modules are linked together. This catches the
# `__odr_asan_gen_*` ODR-detection symbols that ASAN auto-generates, which
# would otherwise produce multiple-definition errors at the final link.
# Same pattern as modules/bitcoinkernelvariant/Makefile.
secp256k1_symbols.map: $(SECP_STATIC_LIB)
	@echo "Generating secp256k1 symbol map for $(SYMBOL_PREFIX)..."
	@{ \
		nm -g --defined-only $(SECP_STATIC_LIB) | awk '{print $$3}'; \
		nm --defined-only $(SECP_STATIC_LIB) | awk '$$2 == "n" {print $$3}'; \
	} | grep -Ev '^$$' | sort -u | while read sym; do \
		echo "$$sym $(SYMBOL_PREFIX)_$$sym"; \
	done > $@
	@sort -u -o $@ $@
	@echo "Generated $$(wc -l < $@) symbol mappings"

module.a: $(LIBBITCOIN_LIB) $(SECP_STATIC_LIB) module.o secp256k1_symbols.map
	rm -f module.a libbitcoin-system_renamed.a libsecp256k1_renamed.a
	# Prefix every secp256k1_* symbol (definitions in secp256k1.a AND
	# references in libbitcoin-system.a) using objcopy --redefine-syms. This
	# avoids symbol collisions when linking with bitcoin core's secp256k1.
	# objcopy processes archive members in place, so we don't need to extract
	# them — important because libbitcoin-system has multiple objects with the
	# same basename (e.g. chain/script.cpp.o and config/script.cpp.o) that
	# would collide on a flat-directory extraction.
	@echo "Renaming secp256k1 symbols in libbitcoin-system and secp256k1 archives..."
	$(OBJCOPY) --redefine-syms=$(abspath secp256k1_symbols.map) $(LIBBITCOIN_LIB) libbitcoin-system_renamed.a
	$(OBJCOPY) --redefine-syms=$(abspath secp256k1_symbols.map) $(SECP_STATIC_LIB) libsecp256k1_renamed.a
	printf 'CREATE module.a\nADDLIB libbitcoin-system_renamed.a\nADDLIB libsecp256k1_renamed.a\nADDMOD module.o\nSAVE\nEND\n' | $(AR) -M
	rm -f libbitcoin-system_renamed.a libsecp256k1_renamed.a
	ranlib module.a

format:
	clang-format -i ./module.cpp ./module.h

check-format:
	clang-format -Werror --fail-on-incomplete-format -n ./module.cpp ./module.h

clean:
	rm -f *.o *.a
	rm -rf $(LIB_DIR)/build_fuzz 2>/dev/null || true

clean-all:
	rm -f *.o *.a secp256k1_symbols.map
	rm -rf $(LIB_DIR)/build_fuzz $(LIB_DIR)/build_secp $(SECP_INSTALL) 2>/dev/null || true

.PHONY: all clean format check-format
