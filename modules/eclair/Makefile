CXX := clang++
CXXFLAGS += -fsanitize=address,fuzzer -Wall -Wextra -std=c++20 -I ../../include -I ../.. -fPIC -shared
LDFLAGS += -ljvm

# Path to JDK
JDK_PATH := $(shell dirname $$(dirname $$(readlink -f $$(which javac))))
CXXFLAGS += -I$(JDK_PATH)/include -I$(JDK_PATH)/include/linux -I$(JDK_PATH)/include/win32

all: module.a

module.a: module.o
$(AR) rcs $@ $^

module.o: module.cpp module.h
$(CXX) $(CXXFLAGS) -c module.cpp -o module.o

clean:
rm -f *.o *.a

