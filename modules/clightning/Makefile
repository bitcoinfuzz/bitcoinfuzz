.PHONY: build-lightning


CXX = clang++
CXXFLAGS += -Wall -Wextra -std=c++20 -I. -I ../../include -I./lightningcore_wrapper

ROOT_DIR := $(realpath $(dir $(lastword $(MAKEFILE_LIST)))/../..)
LIGHTNING_DIR := $(ROOT_DIR)/external/lightning
CLIGHTNING_MAKEFILE := $(ROOT_DIR)/modules/clightning/Makefile-clightning
CLIGHTNING_LIB_H := $(ROOT_DIR)/modules/clightning/clightning_lib.h
CLIGHTNING_LIB_C := $(ROOT_DIR)/modules/clightning/clightning_lib.c
LIBCLIGHTNING_WRAPPER := $(ROOT_DIR)/modules/clightning/libclightning_wrapper.a

all: module.a

$(LIGHTNING_DIR)/config.vars:
	@echo "Running ./configure in $(LIGHTNING_DIR)..."
	cd $(LIGHTNING_DIR) && ./configure


libclightning_wrapper.a: $(LIGHTNING_DIR)/config.vars
	@echo "Copying Makefile-clightning to $(LIGHTNING_DIR)/Makefile..."
	cp $(CLIGHTNING_MAKEFILE) $(LIGHTNING_DIR)/Makefile

	@echo "Copying clightning_lib.c to $(LIGHTNING_DIR)/clightning_lib.c..."
	cp $(CLIGHTNING_LIB_C) $(LIGHTNING_DIR)/clightning_lib.c

	@echo "Copying clightning_lib.h to $(LIGHTNING_DIR)/clightning_lib.h..."
	cp $(CLIGHTNING_LIB_H) $(LIGHTNING_DIR)/clightning_lib.h

	@echo "Building lightning in $(LIGHTNING_DIR)..."
	cd $(LIGHTNING_DIR) && make libclightning_wrapper.a

	@echo "Copying static library in $(LIGHTNING_DIR)..."
	cp $(LIGHTNING_DIR)/libclightning_wrapper.a $(LIBCLIGHTNING_WRAPPER)

module.a: module.o libclightning_wrapper.a
	bash ../../merge.sh module.a libclightning_wrapper.a module.o
	ranlib module.a

module.o: module.cpp
	$(CXX) $(CXXFLAGS) -lscript -L. -fPIC -c module.cpp -o module.o

clean:
	rm -f *.o *.a
