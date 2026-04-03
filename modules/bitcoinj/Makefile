CXXFLAGS += -Wall -Wextra -std=c++20 -fPIC  -I ../../include -I ../../helpers

OS_NAME := $(shell uname | tr '[:upper:]' '[:lower:]')

ifeq ($(OS_NAME),darwin)
    JAVA_HOME ?= $(shell /usr/libexec/java_home)
else
    JAVA_HOME ?= $(shell dirname $$(dirname $$(readlink -f $$(which javac))))
endif

CXXFLAGS += -I$(JAVA_HOME)/include -I$(JAVA_HOME)/include/$(OS_NAME)
BUILD_DIR = app/build/distributions
LIB_DIR = $(shell pwd)/lib
CXXFLAGS += -DLIB_DIR=\"$(LIB_DIR)\"
all: module.a

lib: $(BUILD_DIR)/app.zip
	mkdir -p lib
	mkdir -p bitcoinj_extracted
	unzip -o $(BUILD_DIR)/app.zip -d bitcoinj_extracted
	cp -r bitcoinj_extracted/*/lib/* lib/

$(BUILD_DIR)/app.zip:
	chmod +x ./gradlew
	./gradlew build

module.a: lib module.o
	$(AR) rcs $@ module.o
	ranlib $@

module.o: module.cpp module.h
	$(CXX) $(CXXFLAGS) -fPIC -c module.cpp -o module.o

format:
	clang-format -i module.h module.cpp
	./gradlew ktlintFormat

check-format:
	clang-format -Werror --fail-on-incomplete-format -n module.h module.cpp
	./gradlew ktlintCheck

clean:
	rm -f *.o *.a $(BUILD_DIR)/app.zip
	rm -rf lib bitcoinj_extracted

.PHONY: all lib clean format check-format $(BUILD_DIR)/app.zip
