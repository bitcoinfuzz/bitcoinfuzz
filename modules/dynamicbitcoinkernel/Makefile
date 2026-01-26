all: module.a

CXXFLAGS += -Wall -Wextra -fsanitize=address,fuzzer -std=c++20 -I ../../include

module.a: module.o
	$(AR) rcs $@ $^

module.o: module.cpp module.h
	$(CXX) $(CXXFLAGS) -I . -fPIC -c $< -o $@

format:
	clang-format -i module.h module.cpp

check-format:
	clang-format -Werror --fail-on-incomplete-format -n module.h module.cpp

clean:
	rm -rf *.o module.a
