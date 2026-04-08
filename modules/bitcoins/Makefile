CXXFLAGS += -Wall -Wextra -std=c++20 -fPIC -I ../../include -I ../../helpers

OS_NAME := $(shell uname | tr '[:upper:]' '[:lower:]')

ifeq ($(OS_NAME),darwin)
    JAVA_HOME ?= $(shell /usr/libexec/java_home)
else
    JAVA_HOME ?= $(shell dirname $$(dirname $$(readlink -f $$(which javac))))
endif

CXXFLAGS += -I$(JAVA_HOME)/include -I$(JAVA_HOME)/include/$(OS_NAME)

LIB_DIR = $(shell pwd)/lib
CXXFLAGS += -DLIB_DIR=\"$(LIB_DIR)\"

GJF_VERSION := 1.32.0
GJF := google-java-format-$(GJF_VERSION)-all-deps.jar
GJF_URL := https://github.com/google/google-java-format/releases/download/v$(GJF_VERSION)/$(GJF)

all: module.a

lib:
	mkdir -p lib
	./gradlew :app:copyDeps

lib/BitcoinSWrapper.jar: lib
	javac -cp ".:lib/*" BitcoinSWrapper.java
	jar cf BitcoinSWrapper.jar BitcoinSWrapper.class
	mv BitcoinSWrapper.jar lib/BitcoinSWrapper.jar

module.a: lib module.o lib/BitcoinSWrapper.jar
	$(AR) rcs $@ module.o
	ranlib $@

module.o: module.cpp module.h
	$(CXX) $(CXXFLAGS) -fPIC -c module.cpp -o module.o

$(GJF):
	curl -sSL -o $(GJF) $(GJF_URL)

format: $(GJF)
	clang-format -i module.h module.cpp
	java -jar $(GJF) -i BitcoinSWrapper.java

check-format: $(GJF)
	clang-format -Werror --fail-on-incomplete-format -n module.h module.cpp
	java -jar $(GJF) --dry-run --set-exit-if-changed BitcoinSWrapper.java

clean:
	rm -f *.o *.a
	rm -rf lib
	./gradlew clean 2>/dev/null || true

.PHONY: all lib clean format check-format
