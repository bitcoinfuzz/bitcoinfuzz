CXXFLAGS += -Wall -Wextra -std=c++20 -I. -I ../../include -I./btcd_wrapper

all: module.a

module.a: module.o btcd_wrapper/libbtcd_wrapper.a
	bash ../../merge.sh module.a btcd_wrapper/libbtcd_wrapper.a module.o
	ranlib module.a

btcd_wrapper/libbtcd_wrapper.a:
	cd btcd_wrapper && go build -o libbtcd_wrapper.a -buildmode=c-archive -tags=libfuzzer -gcflags=all=-d=libfuzzer wrapper.go

module.o: module.cpp
	$(CXX) $(CXXFLAGS) -fPIC -c module.cpp -o module.o

format:
	clang-format -i ./module.cpp ./module.h
	cd btcd_wrapper && go fmt

check-format:
	clang-format -Werror --fail-on-incomplete-format -n ./module.cpp ./module.h
	cd btcd_wrapper && files=$$(gofmt -l .); if [ -n "$$files" ]; then echo "Unformatted files:"; echo "$$files"; exit 1; fi

clean:
	rm -f *.o *.a btcd_wrapper/*.a

.PHONY: all clean
