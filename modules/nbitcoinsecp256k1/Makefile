all: module.a

CXXFLAGS += -Wall -Wextra -O2 -std=c++20 -I ../../include

RUNTIME_ID := linux-$(shell uname -m | sed 's/x86_64/x64/; s/aarch64/arm64/')
NBITCOIN_SECP256K1_LIB_EXT := so

NBITCOIN_SECP256K1_LIB_PATH := ./bin/Release/net9.0/$(RUNTIME_ID)/native/NBitcoinSecp256k1.CppBridge.$(NBITCOIN_SECP256K1_LIB_EXT)

$(NBITCOIN_SECP256K1_LIB_PATH):
	dotnet publish -c Release -r $(RUNTIME_ID) --self-contained
	cp $(NBITCOIN_SECP256K1_LIB_PATH) ../../NBitcoinSecp256k1.CppBridge.$(NBITCOIN_SECP256K1_LIB_EXT)

module.a: module.o $(NBITCOIN_SECP256K1_LIB_PATH)
	ar rcs module.a module.o

module.o: module.cpp module.h
	$(CXX) $(CXXFLAGS) -I . -fPIC -c module.cpp -o module.o

format:
	clang-format -i module.h module.cpp NBitcoinSecp256k1/nbitcoinsecp256k1_lib.h
	dotnet format

check-format:
	clang-format -Werror --fail-on-incomplete-format -n module.h module.cpp NBitcoinSecp256k1/nbitcoinsecp256k1_lib.h
	dotnet format --verify-no-changes

clean:
	rm -rf *.o module.a ./bin ./obj

.PHONY: all clean format check-format $(NBITCOIN_SECP256K1_LIB_PATH)
