all: module.a

CXXFLAGS += -Wall -Wextra -fsanitize=address,fuzzer -std=c++20 -I ../../include
PYTHON_CFLAGS := $(shell python3-config --includes)

module.a: module.o
	$(AR) rcs $@ $^
	ranlib $@
	cp pywallet_lib.py ../../pywallet_main.py

module.o: module.cpp module.h
	$(CXX) $(CXXFLAGS) $(PYTHON_CFLAGS) -I . -fPIC -c $< -o $@

format:
	clang-format -i ./module.cpp ./module.h
	black ./pywallet_lib.py

check-format:
	clang-format -Werror --fail-on-incomplete-format -n ./module.cpp ./module.h
	black --check ./pywallet_lib.py

clean:
	rm -rf *.o *.a ../../pywallet_main.py

.PHONY: all clean format check-format
