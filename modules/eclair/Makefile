CXXFLAGS += -Wall -Wextra -std=c++20 -O2 -fPIC  -I ../../include -I ../../helpers

ROOT_DIR := $(realpath $(dir $(lastword $(MAKEFILE_LIST)))/../..)
ECLAIR_DIR := $(ROOT_DIR)/external/eclair

all: module.a

lib: eclair.zip
	mkdir -p lib
	mkdir -p eclair_extracted
	unzip -o eclair.zip -d eclair_extracted
	cp -r eclair_extracted/*/lib/* lib/
	rm -rf eclair_extracted

eclair.zip:
	@echo "Building eclair in $(ECLAIR_DIR)..."
	cd $(ECLAIR_DIR) && ./mvnw package -pl eclair-node -am -Dmaven.test.skip=true -DskipTests

	@# Old builds linger in target/, so pick the newest zip rather than an arbitrary one.
	@ECLAIR_ZIP_BUILD=$$(find $(ECLAIR_DIR)/eclair-node/target -name "eclair-node*.zip" -printf '%T@ %p\n' | sort -rn | head -n 1 | cut -d' ' -f2-); \
	echo "Copying $$ECLAIR_ZIP_BUILD to eclair.zip..."; \
	cp $$ECLAIR_ZIP_BUILD eclair.zip

lib/EclairWrapper.jar: lib
	rm -rf classes
	javac -d classes -cp "lib/*" EclairWrapper.java ../../include/bitcoinfuzz/BfResult.java
	jar cf $@ -C classes .

module.a: lib module.o lib/EclairWrapper.jar
	$(AR) rcs $@ module.o
	ranlib $@

module.o: module.cpp module.h
	$(CXX) $(CXXFLAGS) -fPIC -c module.cpp -o module.o

format:
	clang-format -i ./module.cpp ./module.h
	java -jar google-java-format.jar -i ./EclairWrapper.java

check-format:
	clang-format -Werror --fail-on-incomplete-format -n ./module.cpp ./module.h
	java -jar google-java-format.jar --dry-run --set-exit-if-changed ./EclairWrapper.java

clean:
	rm -f *.o *.a eclair.zip
	rm -rf lib eclair_extracted classes
	cd $(ECLAIR_DIR) && ./mvnw -q -pl eclair-node -am clean

.PHONY: all lib clean format check-format lib/EclairWrapper.jar eclair.zip
