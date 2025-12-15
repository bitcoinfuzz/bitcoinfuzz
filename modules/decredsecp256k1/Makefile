CXXFLAGS += -Wall -Wextra -std=c++20 -I. -I ../../include -I./decredsecp256k1_wrapper

all: module.a

module.a: module.o decredsecp256k1_wrapper/libdecredsecp256k1_wrapper.a
	bash ../../merge.sh module.a decredsecp256k1_wrapper/libdecredsecp256k1_wrapper.a module.o
	ranlib module.a

decredsecp256k1_wrapper/libdecredsecp256k1_wrapper.a:
	cd decredsecp256k1_wrapper && go build -o libdecredsecp256k1_wrapper.a -buildmode=c-archive -tags=libfuzzer -gcflags=all=-d=libfuzzer wrapper.go

module.o: module.cpp
	$(CXX) $(CXXFLAGS) -fPIC -c module.cpp -o module.o

format:
	clang-format -i ./module.cpp ./module.h
	cd decredsecp256k1_wrapper && go fmt

check-format:
	clang-format -Werror --fail-on-incomplete-format -n ./module.cpp ./module.h
	cd decredsecp256k1_wrapper && files=$$(gofmt -l .); if [ -n "$$files" ]; then echo "Unformatted files:"; echo "$$files"; exit 1; fi

clean:
	rm -f *.o *.a decredsecp256k1_wrapper/*.a

.PHONY: all clean
