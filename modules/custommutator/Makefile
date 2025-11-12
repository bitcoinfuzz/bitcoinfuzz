all: module.a

CXXFLAGS += -Wall -Wextra -std=c++20 \
            -fsanitize=fuzzer,address \
            -I . \
            -I ../../include \
            -I /opt/homebrew/include

MAKE = make
AR := ar

# Automatically find all .cpp files
BITCOIN_SRCS := $(shell find . -name "*.cpp" ! -name "bech32.cpp" ! -path "./univalue/*" ! -path "./secp256k1/*")
BITCOIN_OBJS := $(BITCOIN_SRCS:.cpp=.o)

SECP256K1_LIB = ./secp256k1/.libs/libsecp256k1.a

# Build libsecp256k1 with sanitizer flags
$(SECP256K1_LIB):
	cd secp256k1 && \
	./autogen.sh && \
	./configure \
		--enable-module-recovery \
		--enable-experimental \
		--enable-module-schnorrsig \
		--disable-shared \
		--with-pic \
		CFLAGS="-fsanitize=address,fuzzer-no-link" && \
	$(MAKE)

module.a: bech32.o customcrossover.o $(BITCOIN_OBJS) $(SECP256K1_LIB)
	$(AR) rcs $@ bech32.o customcrossover.o $(BITCOIN_OBJS)
	bash ../../merge.sh module.a $(SECP256K1_LIB) bech32.o customcrossover.o $(BITCOIN_OBJS)
	ranlib module.a


bech32.o: bech32.cpp bech32.h
	$(CXX) $(CXXFLAGS) -I . -fPIC -c bech32.cpp -o bech32.o

customcrossover.o: customcrossover.cpp customcrossover.h
	$(CXX) $(CXXFLAGS) -I . -fPIC -c customcrossover.cpp -o customcrossover.o

%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -fPIC -c $< -o $@

clean:
	rm -rf *.o **/*.o module.a tmp_obj/ *_combined.o

.PHONY: clean
