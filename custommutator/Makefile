all: module.a

CXXFLAGS += -Wall -Wextra -std=c++20 \
            -fsanitize=fuzzer,address \
            -I . \
            -I ../../include \
            -I /opt/homebrew/include \
						-I ./utils \
						-I ../

SECP256K1_LIB = ../../external/secp256k1/.libs/libsecp256k1.a
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
	cd tmp_obj && $(AR) x "$(SECP256K1_LIB)"
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
	clang-format -i mutators/*.h mutators/*.cpp

check-format:
	clang-format -Werror --fail-on-incomplete-format -n mutators/*.h mutators/*.cpp

clean:
	rm -rf *.o **/*.o module.a tmp_obj/ *_combined.o
	cd ../external/secp256k1 && $(MAKE) clean || true

.PHONY: clean
