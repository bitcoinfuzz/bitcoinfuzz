all: module.a

CXXFLAGS += -Wall -Wextra -fsanitize=address,fuzzer -std=c++20 -I ../../include

TARGET := $(shell rustc --print host-tuple)

LDK_LIB_PATH := ./ldk_lib/target/$(TARGET)/release/libldk_lib.a

cargo:
	cd ldk_lib && cargo +nightly build --release --target $(TARGET)

module.a: module.o $(LDK_LIB_PATH)
	bash ../../merge.sh module.a $(LDK_LIB_PATH) module.o
	ranlib module.a

module.o: module.cpp module.h
	$(CXX) $(CXXFLAGS) -I . -fPIC -c module.cpp -o module.o

clean:
	rm -rf *.o module.a
