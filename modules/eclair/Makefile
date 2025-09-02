CXX = clang++
CXXFLAGS += -Wall -Wextra -std=c++20 -fPIC  -I ../../include -I ../../helpers

OS_NAME := $(shell uname | tr '[:upper:]' '[:lower:]')

ifeq ($(OS_NAME),darwin)
    JAVA_HOME ?= $(shell /usr/libexec/java_home)
else
    JAVA_HOME ?= $(shell dirname $$(dirname $$(readlink -f $$(which javac))))
endif

CXXFLAGS += -I$(JAVA_HOME)/include -I$(JAVA_HOME)/include/$(OS_NAME)

ROOT_DIR := $(realpath $(dir $(lastword $(MAKEFILE_LIST)))/../..)
ECLAIR_DIR := $(ROOT_DIR)/external/eclair

LIB_DIR = $(shell pwd)/lib
CXXFLAGS += -DLIB_DIR=\"$(LIB_DIR)\"

all: module.a

lib: eclair.zip
	mkdir -p lib
	mkdir -p eclair_extracted
	unzip -o eclair.zip -d eclair_extracted
	cp -r eclair_extracted/*/lib/* lib/

eclair.zip:
	@echo "Building eclair in $(ECLAIR_DIR)..."
	cd $(ECLAIR_DIR) && ./mvnw package -pl eclair-node -am -Dmaven.test.skip=true -DskipTests

	@ECLAIR_ZIP_BUILD=$$(find $(ECLAIR_DIR)/eclair-node/target -name "eclair-node*.zip" | head -n 1); \
	echo "Copying $$ECLAIR_ZIP_BUILD to eclair.zip..."; \
	cp $$ECLAIR_ZIP_BUILD eclair.zip

lib/EclairWrapper.jar: lib
	javac -cp ".:lib/*" EclairWrapper.java
	jar cf EclairWrapper.jar EclairWrapper.class
	cp EclairWrapper.jar lib/EclairWrapper.jar

module.a: lib module.o lib/EclairWrapper.jar
	$(AR) rcs $@ module.o
	ranlib $@

module.o: module.cpp module.h
	$(CXX) $(CXXFLAGS) -fPIC -c module.cpp -o module.o

clean:
	rm -f *.o *.a eclair.zip *.class *.jar
	rm -rf lib eclair_extracted