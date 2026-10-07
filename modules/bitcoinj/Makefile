CXXFLAGS += -Wall -Wextra -std=c++20 -O2 -fPIC  -I ../../include -I ../../helpers

BUILD_DIR = app/build/distributions
all: module.a

lib: $(BUILD_DIR)/app.zip
	mkdir -p lib
	mkdir -p bitcoinj_extracted
	unzip -o $(BUILD_DIR)/app.zip -d bitcoinj_extracted
	cp -r bitcoinj_extracted/*/lib/* lib/

$(BUILD_DIR)/app.zip:
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
