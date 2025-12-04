CXXFLAGS += -Wall -Wextra -std=c++20 -fPIC

INCLUDE_DIRS += -I.
INCLUDE_DIRS += -I../../include
INCLUDE_DIRS += -I../../external/lightning
INCLUDE_DIRS += -I../../external/lightning/ccan
INCLUDE_DIRS += -I../../external/lightning/external/libwally-core/src/secp256k1/include
INCLUDE_DIRS += -I../../external/lightning/external/libwally-core/include
INCLUDE_DIRS += -I../../external/lightning/external/libsodium/src/libsodium/include

CXXFLAGS += $(INCLUDE_DIRS)

ROOT_DIR := $(realpath $(dir $(lastword $(MAKEFILE_LIST)))/../..)
LIGHTNING_DIR := $(ROOT_DIR)/external/lightning
CLIGHTNING_MAKEFILE := ./Makefile-clightning
LIBCLIGHTNING_WRAPPER := ./libcln.a

all: module.a

$(LIGHTNING_DIR)/config.vars:
	@echo "Running ./configure in $(LIGHTNING_DIR)..."
	cd $(LIGHTNING_DIR) && ./configure --enable-address-sanitizer --disable-compat --enable-fuzzing --disable-valgrind --enable-debugbuild --enable-ub-sanitizer CWARNFLAGS="-Wall -Wundef -Wmissing-prototypes -Wmissing-declarations -Wstrict-prototypes -Wold-style-definition"

libcln.a: $(LIGHTNING_DIR)/config.vars
	@echo "Appending Makefile-clightning to $(LIGHTNING_DIR)/Makefile if not already present..."
	@marker="$$(head -n 2 $(CLIGHTNING_MAKEFILE) | tail -n 1)"; \
	if ! grep -Fxq "$$marker" $(LIGHTNING_DIR)/Makefile; then \
		echo "Marker not found, appending..."; \
		awk '{ print }' $(CLIGHTNING_MAKEFILE) >> $(LIGHTNING_DIR)/Makefile; \
	else \
		echo "Makefile already includes clightning makefile content."; \
	fi

	@echo "Building lightning in $(LIGHTNING_DIR)..."
	cd $(LIGHTNING_DIR) && make libcln.a

	@echo "Copying static library in $(LIGHTNING_DIR)..."
	cp $(LIGHTNING_DIR)/libcln.a $(LIBCLIGHTNING_WRAPPER)

module.a: libcln.a module.o
	bash ../../merge.sh module.a libcln.a module.o

module.o: module.cpp module.h
	$(CXX) $(CXXFLAGS) -fPIC -c module.cpp -o module.o


format: 
	clang-format -i ./module.cpp ./module.h

clean:
	rm -f *.o *.a
