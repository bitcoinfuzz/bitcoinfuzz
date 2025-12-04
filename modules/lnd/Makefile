CXXFLAGS += -Wall -Wextra -std=c++20 -I. -I ../../include -I./lnd_wrapper

all: module.a

module.a: golang module.o lnd_wrapper/liblnd_wrapper.a
	bash ../../merge.sh module.a lnd_wrapper/liblnd_wrapper.a module.o
	ranlib module.a

golang:
	cd lnd_wrapper && go build -o liblnd_wrapper.a -buildmode=c-archive -tags=libfuzzer -gcflags=all=-d=libfuzzer wrapper.go

module.o: module.cpp
	$(CXX) $(CXXFLAGS) -lscript -L. -fPIC -c module.cpp -o module.o

format:
	clang-format -i ./module.cpp ./module.h
	cd lnd_wrapper && go fmt

clean:
	rm -f *.o *.a
