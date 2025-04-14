all: module.a

CXX=clang
CXXFLAGS += -Wall -Wextra -fsanitize=address,fuzzer -std=c++20 -I ../../include
PYTHON_CFLAGS := $(shell python3-config --includes)

# Build the static library for the module
module.a: module.o embit_lib/embit_lib.o
	$(AR) rcs $@ $^
	ranlib $@
	cp embit_lib/embit_lib.py ../../main.py

# Compile the module
module.o: module.cpp module.h
	$(CXX) $(CXXFLAGS) -I . -fPIC -c $< -o $@

# Compile the Python lib
embit_lib/embit_lib.o: embit_lib/embit_lib.cpp
	$(CXX) $(CXXFLAGS) $(PYTHON_CFLAGS) -fPIC -c $< -o $@

# Clean up
clean:
	rm -rf *.o *.a ./embit_lib/embit_lib.o ../../main.py