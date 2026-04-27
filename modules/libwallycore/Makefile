all: module.a

CXXFLAGS += -Wall -Wextra -std=c++20 \
            -fsanitize=fuzzer,address \
            -I . \
            -I ../../include \
            -I ../../external/libwally-core/include \
						-I ../../external/libwally-core/src/ccan \
						-I ../../external/libwally-core/_cmake \
						-I ../../external/libwally-core/src

LIBWALLY_CORE_LIB = ../../external/libwally-core/src/.libs/libwallycore.a
LIBWALLY_CORE_SECP_LIB = ../../external/libwally-core/src/secp256k1/.libs/libsecp256k1.a
MAKE = make
REDEFINE_SYMS = /tmp/libwallycore_redefine.syms

# Build libwally_core with sanitizer flags
$(LIBWALLY_CORE_LIB):
	cd ../../external/libwally-core && \
	./tools/autogen.sh && \
	./configure \
		--disable-shared \
		--enable-export-all \
		--disable-elements \
		--disable-elements-abi \
		--disable-tests \
		CFLAGS="-fsanitize=address,fuzzer-no-link" && \
	$(MAKE)

module.a: $(LIBWALLY_CORE_LIB) $(LIBWALLY_CORE_SECP_LIB) module.o
	bash ../../merge.sh module.a $(LIBWALLY_CORE_LIB) module.o
	rm -rf _secp_tmp && mkdir _secp_tmp
	cd _secp_tmp && ar x ../$(LIBWALLY_CORE_SECP_LIB)
	ar rcs module.a _secp_tmp/*.o
	ranlib module.a
	rm -rf _secp_tmp
	@nm module.a | grep '__odr_asan_gen_' | awk '{print $$3}' | sort -u | \
		awk '{print $$1, "libwallycore_" $$1}' > $(REDEFINE_SYMS)
	@if [ -s $(REDEFINE_SYMS) ]; then \
		objcopy --redefine-syms=$(REDEFINE_SYMS) module.a; \
	fi
	@rm -f $(REDEFINE_SYMS)

module.o: module.cpp module.h
	$(CXX) $(CXXFLAGS) -I . -fPIC -c module.cpp -o module.o

format: 
	clang-format -i ./module.cpp ./module.h

check-format:
	clang-format -Werror --fail-on-incomplete-format -n ./module.cpp ./module.h

clean:
	rm -f *.o *.a
	cd ../../external/libwally-core && $(MAKE) clean || true

.PHONY: all clean format check-format
