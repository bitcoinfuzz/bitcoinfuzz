all: module.a

CXXFLAGS += -Wall -Wextra -std=c++20 \
            -fsanitize=fuzzer,address \
            -I . \
            -I ../../include \
            -I /opt/homebrew/include \
            -I ../../external/secp256k1/include

SECP256K1_LIB = ../../external/secp256k1/.libs/libsecp256k1.a
MAKE = make

# Build libsecp256k1 with sanitizer flags
$(SECP256K1_LIB):
	cd ../../external/secp256k1 && \
	./autogen.sh && \
	./configure \
		--enable-module-recovery \
		--enable-experimental \
		--enable-module-schnorrsig \
		--disable-shared \
		--with-pic \
		CFLAGS="-fsanitize=address,fuzzer-no-link" && \
	$(MAKE)

module.a: $(SECP256K1_LIB) module.o
	bash ../../merge.sh module.a $(SECP256K1_LIB) module.o

module.o: module.cpp module.h
	$(CXX) $(CXXFLAGS) -I . -fPIC -c module.cpp -o module.o

format: 
	clang-format -i ./module.cpp ./module.h

check-format:
	clang-format -Werror --fail-on-incomplete-format -n ./module.cpp ./module.h

clean:
	rm -f *.o *.a
	cd ../../external/secp256k1 && $(MAKE) clean || true

.PHONY: all clean format check-format
