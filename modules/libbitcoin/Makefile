all: module.a

CXX := clang++
CC := clang

# Find all source files including .ipp files
CPP_SRCS := $(shell find . -name "*.cpp" ! -name "module.cpp" ! -path "./secp256k1/*")
C_SRCS := $(shell find . -name "*.c" ! -path "./secp256k1/*")
IPP_SRCS := $(shell find . -name "*.ipp" ! -path "./secp256k1/*")

# Create object file lists
CPP_OBJS := $(CPP_SRCS:.cpp=.o)
C_OBJS := $(C_SRCS:.c=.o)
IPP_OBJS := $(IPP_SRCS:.ipp=.o)
ALL_OBJS := $(CPP_OBJS) $(C_OBJS) $(IPP_OBJS)

# Use environment variables for Boost paths with fallbacks
BOOST_INCLUDE_PATH := $(or $(BOOST_INCLUDE_DIR),/opt/homebrew/include)
BOOST_LIB_PATH := $(or $(BOOST_LIB_DIR),/opt/homebrew/lib)

CXXFLAGS += -Wall -Wextra -std=c++20 \
            -fsanitize=fuzzer,address \
            -I . \
            -I $(BOOST_INCLUDE_PATH) \
            -I /opt/homebrew/include \
            -I ./include \
            -I ../../include \
            -I ./secp256k1/include \
            -I ./math/external

CFLAGS += -Wall -Wextra \
          -fsanitize=fuzzer,address \
          -I . \
          -I $(BOOST_INCLUDE_PATH) \
          -I /opt/homebrew/include \
          -I ./include \
          -I ../../include \
          -I ./secp256k1/include \
          -I ./math/external

SECP256K1_LIB = ../secp256k1/.libs/libsecp256k1.a
MAKE = make

# Build libsecp256k1 without sanitizer flags
$(SECP256K1_LIB):
	cd secp256k1 && \
	./autogen.sh && \
	CFLAGS="" ./configure --enable-module-recovery --enable-experimental --enable-module-schnorrsig --disable-shared --with-pic && \
	$(MAKE)

module.a: $(SECP256K1_LIB) module.o $(ALL_OBJS)
	$(AR) rcs $@ module.o $(ALL_OBJS)
	mkdir -p tmp_obj
	# Extract and add secp256k1
	cd tmp_obj && $(AR) x "$(SECP256K1_LIB)"
	$(AR) rcs $@ tmp_obj/*.o
	rm -rf tmp_obj/*

module.o: module.cpp module.h
	$(CXX) $(CXXFLAGS) -I . -fPIC -c module.cpp -o module.o

%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -fPIC -c $< -o $@

%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -fPIC -c $< -o $@

%.o: %.ipp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -fPIC -x c++ -c $< -o $@

clean:
	rm -rf *.o **/*.o module.a tmp_obj/ *_combined.o
	cd secp256k1 && $(MAKE) clean || true

.PHONY: clean
