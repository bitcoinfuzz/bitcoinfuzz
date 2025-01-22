all: module.a

CXXFLAGS += -Wall -Wextra -std=c++20 -I ../../include
# MacOS: ./rust_bitcoin_lib/target/aarch64-apple-darwin/release/librust_bitcoin_lib.a
RUST_MINISCRIPT_LIB_PATH := ./rust_miniscript_lib/target/$(RUST_TARGET)/release/librust_miniscript_lib.a

module.a: module.o $(RUST_MINISCRIPT_LIB_PATH)
	bash ../../merge.sh module.a $(RUST_MINISCRIPT_LIB_PATH) module.o
	ranlib module.a

module.o: module.cpp module.h
	$(CXX) $(CXXFLAGS) -I . -fPIC -c module.cpp -o module.o

clean:
	rm -rf *.o module.a