all: module.a

CXXFLAGS += -Wall -Wextra -std=c++20 \
            -fsanitize=fuzzer,address \
            -I . \
            -I ../../include \
            -I /opt/homebrew/include \
            -I ./secp256k1/include \
            -I ./univalue/include \
            -I ./consensus \
            -I ./primitives \
            -I ./script

SECP256K1_LIB = ../secp256k1/.libs/libsecp256k1.a
UNIVALUE_LIB = ../univalue/build/libunivalue.a
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

module.a: $(SECP256K1_LIB) $(UNIVALUE_LIB) module.o cleanse.o $(BITCOIN_OBJS)
	$(AR) rcs $@ module.o cleanse.o $(BITCOIN_OBJS)
	mkdir -p tmp_obj
	# Extract and add secp256k1
	cd tmp_obj && $(AR) x "$(SECP256K1_LIB)"
	$(AR) rcs $@ tmp_obj/*.o
	rm -rf tmp_obj/*
	# Extract and add univalue
	cd tmp_obj && $(AR) x "$(UNIVALUE_LIB)"
	$(AR) rcs $@ tmp_obj/*.o
	rm -rf tmp_obj

module.o: module.cpp module.h
	$(CXX) $(CXXFLAGS) -I . -fPIC -c module.cpp -o module.o

cleanse.o: support/cleanse.cpp
	$(CXX) $(CXXFLAGS) -I . -fPIC -c support/cleanse.cpp -o cleanse.o

%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -fPIC -c $< -o $@

clean:
	rm -rf *.o **/*.o module.a tmp_obj/ *_combined.o
	cd secp256k1 && $(MAKE) clean || true
	cd univalue && rm -rf build || true

.PHONY: clean
