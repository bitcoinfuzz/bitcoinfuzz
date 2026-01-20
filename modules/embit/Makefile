all: module.a

CXXFLAGS += -Wall -Wextra -fsanitize=address,fuzzer -std=c++20 -I ../../include
PYTHON_CFLAGS := $(shell python3-config --includes)

module.a: module.o
	$(AR) rcs $@ $^
	ranlib $@
	cp embit_lib.py ../../main.py

module.o: module.cpp module.h
	$(CXX) $(CXXFLAGS) $(PYTHON_CFLAGS) -I . -fPIC -c $< -o $@

format:
	clang-format -i ./module.cpp ./module.h
	black ./embit_lib.py

check-format:
	clang-format -Werror --fail-on-incomplete-format -n ./module.cpp ./module.h
	black --check ./embit_lib.py

clean:
	rm -rf *.o *.a ./embit_lib/embit_lib.o ../../main.py

.PHONY: all clean format check-format
