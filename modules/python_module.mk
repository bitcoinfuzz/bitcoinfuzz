all: module.a

CXXFLAGS += -Wall -Wextra -O2 -std=c++20 -I ../../include -I ../../helpers
PYTHON_CFLAGS := $(shell python3-config --includes)

module.a: module.o
	$(AR) rcs $@ $^
	ranlib $@

module.o: module.cpp module.h
	$(CXX) $(CXXFLAGS) $(PYTHON_CFLAGS) -I . -fPIC -c $< -o $@

format:
	clang-format -i ./module.cpp ./module.h
	black ./$(MODULE_NAME)_lib.py

check-format:
	clang-format -Werror --fail-on-incomplete-format -n ./module.cpp ./module.h
	black --check ./$(MODULE_NAME)_lib.py

clean:
	rm -rf *.o *.a ./$(MODULE_NAME)_lib/$(MODULE_NAME)_lib.o

.PHONY: all clean format check-format
