all: module.a

CXXFLAGS += -Wall -Wextra -std=c++20 -I ../../include
# MacOS: ./ldk_lib/target/aarch64-apple-darwin/release/libldk_lib.a
LDK_LIB_PATH := ./ldk_lib/target/$(RUST_TARGET)/release/libldk_lib.a
LDK_LIB_NAME := libldk_lib.a

module.a: module.o $(LDK_LIB_PATH)
	bash ../../merge.sh module.a $(LDK_LIB_PATH) module.o
	ranlib module.a

module.o: module.cpp module.h
	$(CXX) $(CXXFLAGS) -I . -fPIC -c module.cpp -o module.o

clean:
	rm -rf *.o module.a