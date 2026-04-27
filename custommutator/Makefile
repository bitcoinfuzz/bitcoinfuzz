all: module.a

CXXFLAGS += -Wall -Wextra -std=c++20 \
            -fsanitize=fuzzer,address \
            -I . \
            -I ../../include \
            -I /opt/homebrew/include \
						-I ./utils \
						-I ../

SECP256K1_LIB = ../external/secp256k1/.libs/libsecp256k1.a
MAKE = make
AR := ar

# Automatically find all .cpp files
BITCOIN_SRCS := $(shell find . -name "*.cpp" ! -name "bech32.cpp" ! -name "test_onion_main.cpp" ! -path "./univalue/*" ! -path "./secp256k1/*")
BITCOIN_OBJS := $(BITCOIN_SRCS:.cpp=.o)

# Build libsecp256k1 with sanitizer flags
$(SECP256K1_LIB):
	cd ../external/secp256k1 && \
	./autogen.sh && \
	./configure \
		--enable-module-recovery \
		--enable-experimental \
		--enable-module-schnorrsig \
		--disable-shared \
		--with-pic \
		CFLAGS="-fsanitize=address,fuzzer-no-link" && \
	$(MAKE)

module.a: utils/bech32.o mutators/customcrossover.o $(SECP256K1_LIB) $(BITCOIN_OBJS)
	mkdir -p tmp_obj
	cd tmp_obj && $(AR) x "../$(SECP256K1_LIB)"
	$(AR) rcs $@ utils/bech32.o mutators/customcrossover.o $(BITCOIN_OBJS) tmp_obj/*.o
	rm -rf tmp_obj

utils/bech32.o: utils/bech32.cpp utils/bech32.h
	$(CXX) $(CXXFLAGS) -I . -fPIC -c utils/bech32.cpp -o utils/bech32.o

mutators/customcrossover.o: mutators/customcrossover.cpp mutators/customcrossover.h
	$(CXX) $(CXXFLAGS) -I . -fPIC -c mutators/customcrossover.cpp -o mutators/customcrossover.o

%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -fPIC -c $< -o $@

format:
	clang-format -i mutators/*.h mutators/*.cpp tests/*.cpp

check-format:
	clang-format -Werror --fail-on-incomplete-format -n mutators/*.h mutators/*.cpp tests/*.cpp

clean:
	rm -rf *.o **/*.o module.a tmp_obj/ *_combined.o test_onion
	cd ../external/secp256k1 && $(MAKE) clean || true

# Test for onion custom mutator
INCLUDE_DIRS += -I.
INCLUDE_DIRS += -I../include
INCLUDE_DIRS += -I../external/lightning
INCLUDE_DIRS += -I../external/lightning/ccan
INCLUDE_DIRS += -I../external/lightning/external/libwally-core/src/secp256k1/include
INCLUDE_DIRS += -I../external/lightning/external/libwally-core/include
INCLUDE_DIRS += -I../external/lightning/external/libsodium/src/libsodium/include
INCLUDE_DIRS += -I../external/lightning/hsmd

ROOT_DIR := $(realpath $(dir $(lastword $(MAKEFILE_LIST)))/..)
LIGHTNING_DIR := $(ROOT_DIR)/external/lightning
CLIGHTNING_MAKEFILE := $(ROOT_DIR)/modules/clightning/Makefile-clightning
LIBCLIGHTNING_WRAPPER := ./libcln.a

$(LIGHTNING_DIR)/config.vars:
	@echo "Running ./configure in $(LIGHTNING_DIR)..."
	cd $(LIGHTNING_DIR) && ./configure --enable-address-sanitizer --disable-compat --enable-fuzzing --disable-valgrind --enable-debugbuild --enable-ub-sanitizer CWARNFLAGS="-Wall -Wundef -Wmissing-prototypes -Wmissing-declarations -Wstrict-prototypes -Wold-style-definition"

libcln.a: $(LIGHTNING_DIR)/config.vars
	@marker="$$(head -n 2 $(CLIGHTNING_MAKEFILE) | tail -n 1)"; \
	if ! grep -Fxq "$$marker" $(LIGHTNING_DIR)/Makefile; then \
		echo "Marker not found, appending..."; \
		awk '{ print }' $(CLIGHTNING_MAKEFILE) >> $(LIGHTNING_DIR)/Makefile; \
	else \
		echo "Makefile already includes clightning makefile content."; \
	fi
	cd $(LIGHTNING_DIR) && make libcln.a
	cp $(LIGHTNING_DIR)/libcln.a $(LIBCLIGHTNING_WRAPPER)

CRYPTO_OBJS = ./utils/crypto/chacha20.o ./utils/crypto/hmac_sha256.o \
              ./utils/support/cleanse.o ./utils/support/lockedpool.o \
              ./utils/sha256.o ./utils/util/sha256_sse4.o
test_onion: $(SECP256K1_LIB) $(CRYPTO_OBJS) tests/test_onion_main.cpp libcln.a
	$(CXX) -Wall -Wextra -std=c++20 -fsanitize=address \
		-I . -I ../../include -I ./utils -I ../ $(INCLUDE_DIRS) \
		-o test_onion \
		tests/test_onion_main.cpp \
		$(CRYPTO_OBJS) \
		$(SECP256K1_LIB) \
		libcln.a \
		-lsodium

test: test_onion
	./test_onion

.PHONY: clean
