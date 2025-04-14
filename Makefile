all: bitcoinfuzz

CXX := clang++
CXXFLAGS += -fsanitize=address,fuzzer -Wall -Wextra -std=c++20 -I include -I .
MODULES := $(wildcard modules/*/module.a)
UNAME_S := $(shell uname -s)

# Get Python linking flags if the embit module is being used (DEMBIT is defined)
PYTHON_LDFLAGS = $(shell if echo "$(CXXFLAGS)" | grep -q "\-DEMBIT"; then python3-config --ldflags --embed; fi)

ifeq ($(UNAME_S), Darwin)
LDFLAGS = -framework CoreFoundation -Wl,-ld_classic
endif

bitcoinfuzz: main.cpp driver.o include/bitcoinfuzz/basemodule.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) main.cpp $(MODULES) driver.o include/bitcoinfuzz/basemodule.o -o bitcoinfuzz $(PYTHON_LDFLAGS)

driver.o: driver.cpp driver.h
	$(CXX) $(CXXFLAGS) -c driver.cpp -o driver.o

include/bitcoinfuzz/basemodule.o: include/bitcoinfuzz/basemodule.cpp include/bitcoinfuzz/basemodule.h
	$(CXX) $(CXXFLAGS) -c include/bitcoinfuzz/basemodule.cpp -o include/bitcoinfuzz/basemodule.o

clean:
	rm -rf *.o module.a bitcoinfuzz include/bitcoinfuzz/*.o $(MODULES)

.PHONY: all bitcoinfuzz