all: module.a

CXXFLAGS += -Wall -Wextra -std=c++20 \
            -fsanitize=fuzzer,address \
            -I . \
            -I ../../include \
            -I /opt/homebrew/include \
						-I ./utils \
						-I ../

MAKE = make
AR := ar

# Automatically find all .cpp files
BITCOIN_SRCS := $(shell find . -name "*.cpp" ! -name "bech32.cpp" ! -name "cleanse.cpp" ! -path "./univalue/*" ! -path "./secp256k1/*")
BITCOIN_OBJS := $(BITCOIN_SRCS:.cpp=.o)

module.a: utils/bech32.o  mutators/customcrossover.o $(BITCOIN_OBJS)
	$(AR) rcs $@ utils/bech32.o mutators/customcrossover.o $(BITCOIN_OBJS)

utils/bech32.o: utils/bech32.cpp utils/bech32.h
	$(CXX) $(CXXFLAGS) -I . -fPIC -c utils/bech32.cpp -o utils/bech32.o

mutators/customcrossover.o: mutators/customcrossover.cpp mutators/customcrossover.h
	$(CXX) $(CXXFLAGS) -I . -fPIC -c mutators/customcrossover.cpp -o mutators/customcrossover.o

%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -fPIC -c $< -o $@

clean:
	rm -rf *.o **/*.o module.a tmp_obj/ *_combined.o

.PHONY: clean
