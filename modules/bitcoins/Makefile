CXXFLAGS += -Wall -Wextra -std=c++20 -O2 -fPIC -I ../../include -I ../../helpers

GJF_VERSION := 1.32.0
GJF := google-java-format-$(GJF_VERSION)-all-deps.jar
GJF_URL := https://github.com/google/google-java-format/releases/download/v$(GJF_VERSION)/$(GJF)

all: module.a

lib:
	mkdir -p lib
	./gradlew :app:copyDeps

lib/BitcoinSWrapper.jar: lib
	rm -rf classes
	javac -d classes -cp "lib/*" BitcoinSWrapper.java ../../include/bitcoinfuzz/BfResult.java
	jar cf $@ -C classes .

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
	rm -rf lib classes
	./gradlew clean 2>/dev/null || true

.PHONY: all lib clean format check-format
