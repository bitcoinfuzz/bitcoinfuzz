CXXFLAGS += -Wall -Wextra -std=c++20 -I. -I ../../include -I./utreexo_wrapper

all: module.a

module.a: module.o utreexo_wrapper/libutreexo_wrapper.a
	bash ../../merge.sh module.a utreexo_wrapper/libutreexo_wrapper.a module.o
	ranlib module.a

utreexo_wrapper/libutreexo_wrapper.a:
	cd utreexo_wrapper && go build -o libutreexo_wrapper.a -buildmode=c-archive -tags=libfuzzer -gcflags=all=-d=libfuzzer wrapper.go

golang: utreexo_wrapper/libutreexo_wrapper.a

module.o: module.cpp utreexo_wrapper/libutreexo_wrapper.a
	$(CXX) $(CXXFLAGS) -lscript -L. -fPIC -c module.cpp -o module.o

format:
	clang-format -i ./module.cpp ./module.h
	cd utreexo_wrapper && go fmt

check-format:
	clang-format -Werror --fail-on-incomplete-format -n ./module.cpp ./module.h
	cd utreexo_wrapper && files=$$(gofmt -l .); if [ -n "$$files" ]; then echo "Unformatted files:"; echo "$$files"; exit 1; fi

clean:
	rm -rf *.o *.a utreexo_wrapper/*.so utreexo_wrapper/*.a utreexo_wrapper/*.h

.PHONY: all golang clean format check-format utreexo_wrapper/libutreexo_wrapper.a
