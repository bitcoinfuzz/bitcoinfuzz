all: module.a

SYMBOL_PREFIX ?= bitcoin_core

# Detect objcopy command
OBJCOPY := $(shell command -v objcopy 2> /dev/null)
ifeq ($(OBJCOPY),)
$(error objcopy not found. Install with: apt-get install binutils)
endif

CXXFLAGS += -Wall -Wextra -std=c++20 \
            -fsanitize=fuzzer,address \
            -I . \
            -I ../../include \
            -I ./secp256k1/include \
            -I ./univalue/include \
            -I ./consensus \
            -I ./primitives \
            -I ./script

SECP256K1_LIB = secp256k1/.libs/libsecp256k1.a
UNIVALUE_LIB = univalue/build/libunivalue.a
MAKE = make

# Automatically find all .cpp files
BITCOIN_SRCS := $(shell find . -name "*.cpp" ! -name "module.cpp" ! -name "cleanse.cpp" ! -path "./univalue/*" ! -path "./secp256k1/*")
BITCOIN_OBJS := $(BITCOIN_SRCS:.cpp=.o)

# Build libsecp256k1 with sanitizer flags
$(SECP256K1_LIB):
	cd secp256k1 && \
	./autogen.sh && \
	./configure \
		--enable-module-recovery \
		--enable-experimental \
		--enable-module-schnorrsig \
		--enable-module-musig \
		--disable-shared \
		--with-pic \
		CFLAGS="-fsanitize=address,fuzzer-no-link" && \
	$(MAKE)

# Build univalue with sanitizer flags
$(UNIVALUE_LIB):
	cd univalue && \
	CXXFLAGS="-fsanitize=address" cmake -B build && \
	cmake --build build && \
	cd build && \
	$(MAKE) univalue

# Generate symbol redefine map for ONLY secp256k1 symbols (exclude ASAN/sanitizer symbols)
secp256k1_symbols.map: $(SECP256K1_LIB)
	@echo "Generating comprehensive symbol map..."
	@nm -g $(SECP256K1_LIB) | awk '{print $$3}' | grep '^secp256k1' | sort -u | while read sym; do \
		echo "$$sym $(SYMBOL_PREFIX)_$$sym"; \
	done > $@
	@echo "Generated $(shell wc -l < $@) symbol mappings"

# Compile all object files first (without dependencies to avoid recompilation)
.PHONY: compile_objects
compile_objects: module.o cleanse.o $(BITCOIN_OBJS)

module.a: $(SECP256K1_LIB) $(UNIVALUE_LIB) secp256k1_symbols.map compile_objects
	@echo "========================================"
	@echo "Building module.a with symbol prefix: $(SYMBOL_PREFIX)"
	@echo "========================================"

	# Now rename symbols in all compiled object files
	@echo "Renaming secp256k1 symbols in Bitcoin object files..."
	@for obj in module.o cleanse.o $(BITCOIN_OBJS); do \
		if [ -f "$$obj" ]; then \
			echo "  Renaming symbols in $$obj..."; \
			$(OBJCOPY) --redefine-syms=secp256k1_symbols.map $$obj; \
		fi \
	done

	@echo "Verifying renamed symbols in pubkey.o:"
	@nm pubkey.o | grep secp256k1 | head -3

	# Create archive with renamed Bitcoin objects
	@echo "Creating archive with renamed objects..."
	$(AR) rcs $@ module.o cleanse.o $(BITCOIN_OBJS)

	mkdir -p tmp_obj
	# Extract secp256k1 objects and rename ONLY secp256k1_* symbols
	cd tmp_obj && $(AR) x ../$(SECP256K1_LIB)
	@echo "Renaming only secp256k1_* symbols (preserving ASAN/sanitizer symbols)..."
	cd tmp_obj && for obj in *.o; do \
		for sym in $$(nm $$obj | grep ' T secp256k1' | awk '{print $$3}'); do \
			$(OBJCOPY) --redefine-sym $$sym=$(SYMBOL_PREFIX)_$$sym $$obj 2>/dev/null || true; \
		done; \
		for sym in $$(nm $$obj | grep ' D secp256k1' | awk '{print $$3}'); do \
			$(OBJCOPY) --redefine-sym $$sym=$(SYMBOL_PREFIX)_$$sym $$obj 2>/dev/null || true; \
		done; \
		for sym in $$(nm $$obj | grep ' B secp256k1' | awk '{print $$3}'); do \
			$(OBJCOPY) --redefine-sym $$sym=$(SYMBOL_PREFIX)_$$sym $$obj 2>/dev/null || true; \
		done; \
	done
	$(AR) rcs $@ tmp_obj/*.o
	rm -rf tmp_obj/*

	# Extract and add univalue WITHOUT prefixing (no conflicts)
	cd tmp_obj && $(AR) x ../$(UNIVALUE_LIB)
	@echo "Adding univalue symbols (no renaming needed)..."
	$(AR) rcs $@ tmp_obj/*.o
	rm -rf tmp_obj
	@echo ""
	@echo "✓ module.a built successfully with prefix: $(SYMBOL_PREFIX)"

module.o: module.cpp module.h
	$(CXX) $(CXXFLAGS) -I . -fPIC -c module.cpp -o module.o

cleanse.o: support/cleanse.cpp
	$(CXX) $(CXXFLAGS) -I . -fPIC -c support/cleanse.cpp -o cleanse.o

%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -fPIC -c $< -o $@

clean:
	rm -rf *.o **/*.o module.a tmp_obj/ secp256k1_symbols.map
	cd secp256k1 && $(MAKE) clean || true
	cd univalue && rm -rf build || true

format:
	clang-format -i ./module.cpp ./module.h

check-format:
	clang-format -Werror --fail-on-incomplete-format -n ./module.cpp ./module.h

.PHONY: clean all format check-format compile_objects
