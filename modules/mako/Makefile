all: module.a

CXX = clang++
CXXFLAGS += -Wall -Wextra -std=c++20 -I ../../include
# MAKO_LIB_PATH:= /Users/user/projects/mako/build/libmako.a

module.a: module.o $(MAKO_LIB_PATH)
	bash ../../merge.sh module.a $(MAKO_LIB_PATH) module.o
	ranlib module.a

module.o: module.cpp module.h
	$(CXX) $(CXXFLAGS) -I . -fPIC -c module.cpp -o module.o

clean:
	rm -rf *.o module.a
