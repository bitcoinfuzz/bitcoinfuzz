all: check-quickjs module.a

quickjs_prefix_is_complete = $(and \
	$(wildcard $(1)/bin/qjsc), \
	$(wildcard $(1)/include/quickjs/quickjs.h), \
	$(wildcard $(1)/lib/quickjs/libquickjs.a))

# QuickJs should be installed at /usr or /usr/local
ifneq ($(call quickjs_prefix_is_complete,/usr/local),)
	QUICKJS_PREFIX := /usr/local
else ifneq ($(call quickjs_prefix_is_complete,/usr),)
	QUICKJS_PREFIX := /usr
endif

QJSC := $(QUICKJS_PREFIX)/bin/qjsc
QUICKJS_INCLUDE_DIR := $(QUICKJS_PREFIX)/include/quickjs
QUICKJS_LIB := $(QUICKJS_PREFIX)/lib/quickjs/libquickjs.a

CXXFLAGS += -Wall -Wextra -fsanitize=fuzzer -std=c++20 -I ../../include -isystem $(QUICKJS_INCLUDE_DIR)

TS_DIR := ./ts
NODE_MODULES_DIR := $(TS_DIR)/node_modules
ESBUILD := $(NODE_MODULES_DIR)/.bin/esbuild
PRETTIER := $(NODE_MODULES_DIR)/.bin/prettier
DIST_DIR := $(TS_DIR)/dist
BUNDLE_JS := $(DIST_DIR)/bundle.js
BUNDLE_C := $(DIST_DIR)/bundle.c
BUNDLE_O := $(DIST_DIR)/bundle.o

check-quickjs:
	@test -x "$(QJSC)" || { echo "Error: qjsc not found at $(QJSC). Install QuickJS in /usr/local or /usr"; exit 1; }
	@test -f "$(QUICKJS_INCLUDE_DIR)/quickjs.h" || { echo "Error: quickjs.h not found in $(QUICKJS_INCLUDE_DIR). Install QuickJS headers"; exit 1; }
	@test -f "$(QUICKJS_LIB)" || { echo "Error: libquickjs.a not found at $(QUICKJS_LIB). Install QuickJS static library"; exit 1; }

module.a: module.o $(BUNDLE_O) $(QUICKJS_LIB)
	bash ../../merge.sh module.a $(QUICKJS_LIB) module.o $(BUNDLE_O)
	ranlib module.a

$(NODE_MODULES_DIR): $(TS_DIR)/package.json $(TS_DIR)/package-lock.json
	cd $(TS_DIR) && npm ci

$(BUNDLE_JS): $(NODE_MODULES_DIR) $(TS_DIR)/src/index.ts
	mkdir -p $(dir $@)
	$(ESBUILD) $(TS_DIR)/src/index.ts \
		--bundle \
		--platform=node \
		--format=iife \
		--outfile=$@

$(BUNDLE_C): $(BUNDLE_JS) $(QJSC)
	$(QJSC) -c -o $@ $(BUNDLE_JS)

$(BUNDLE_O): $(BUNDLE_C)
	$(CC) -Wall -Wextra -fsanitize=fuzzer -c $(BUNDLE_C) -o $@

module.o: module.cpp module.h
	$(CXX) $(CXXFLAGS) -fPIC -c module.cpp -o module.o

format: $(NODE_MODULES_DIR)
	clang-format -i ./module.cpp ./module.h
	$(PRETTIER) $(TS_DIR)/src/index.ts --write --config $(TS_DIR)/.prettier.json

check-format: $(NODE_MODULES_DIR)
	clang-format -Werror --fail-on-incomplete-format -n ./module.cpp ./module.h
	$(PRETTIER) $(TS_DIR)/src/index.ts --check --config $(TS_DIR)/.prettier.json

clean:
	rm -rf *.o module.a $(DIST_DIR)

clean-all: clean
	# Also removes the installed JS dependencies in ts/node_modules.
	rm -rf $(NODE_MODULES_DIR)

.PHONY: all check-quickjs clean clean-all format check-format
