CXXFLAGS += -Wall -Wextra -std=c++20 -I. -I ../../include -I./lnd_wrapper

all: module.a

module.a: module.o lnd_wrapper/liblnd_wrapper.a
	bash ../../merge.sh module.a lnd_wrapper/liblnd_wrapper.a module.o
	ranlib module.a

lnd_wrapper/liblnd_wrapper.a:
	cd lnd_wrapper && go build -o liblnd_wrapper.a -buildmode=c-archive -tags=libfuzzer -gcflags=all=-d=libfuzzer wrapper.go

golang: lnd_wrapper/liblnd_wrapper.a

module.o: module.cpp
	$(CXX) $(CXXFLAGS) -lscript -L. -fPIC -c module.cpp -o module.o

format:
	clang-format -i ./module.cpp ./module.h
	cd lnd_wrapper && go fmt

check-format:
	clang-format -Werror --fail-on-incomplete-format -n ./module.cpp ./module.h
	cd lnd_wrapper && files=$$(gofmt -l .); if [ -n "$$files" ]; then echo "Unformatted files:"; echo "$$files"; exit 1; fi

clean:
	rm -f *.o *.a

.PHONY: all golang clean format check-format lnd_wrapper/liblnd_wrapper.a
