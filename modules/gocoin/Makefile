CXXFLAGS += -Wall -Wextra -std=c++20 -I. -I ../../include -I./gocoin_wrapper

SYMBOL_PREFIX ?= gocoin

all: module.a

module.a: module.o gocoin_wrapper/libgocoin_wrapper.a
	bash ../../merge.sh module.a gocoin_wrapper/libgocoin_wrapper.a module.o
	ranlib module.a

gocoin_wrapper/libgocoin_wrapper.a:
	cd gocoin_wrapper && go build -o libgocoin_wrapper.a -buildmode=c-archive -tags=libfuzzer -gcflags=all=-d=libfuzzer wrapper.go
	bash ../../rename_cgo_symbols.sh gocoin_wrapper/libgocoin_wrapper.a $(SYMBOL_PREFIX)

module.o: module.cpp gocoin_wrapper/libgocoin_wrapper.a
	$(CXX) $(CXXFLAGS) -fPIC -c module.cpp -o module.o

format:
	clang-format -i ./module.cpp ./module.h
	cd gocoin_wrapper && go fmt

check-format:
	clang-format -Werror --fail-on-incomplete-format -n ./module.cpp ./module.h
	cd gocoin_wrapper && files=$$(gofmt -l .); if [ -n "$$files" ]; then echo "Unformatted files:"; echo "$$files"; exit 1; fi

clean:
	rm -f *.o *.a gocoin_wrapper/*.a gocoin_wrapper/*.h

.PHONY: all clean format check-format
