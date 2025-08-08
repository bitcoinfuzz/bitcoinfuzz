CXX = clang++
CXXFLAGS += -Wall -Wextra -std=c++20 -I. -I ../../include -I./btcd_wrapper

all: module.a

module.a: golang module.o btcd_wrapper/libbtcd_wrapper.a
	bash ../../merge.sh module.a btcd_wrapper/libbtcd_wrapper.a module.o
	ranlib module.a

golang:
	cd btcd_wrapper && go build -o libbtcd_wrapper.a -buildmode=c-archive -tags=libfuzzer -gcflags=all=-d=libfuzzer wrapper.go

module.o: module.cpp
	$(CXX) $(CXXFLAGS) -lscript -L. -fPIC -c module.cpp -o module.o

clean:
	rm -f *.o *.a btcd_wrapper/*.a
