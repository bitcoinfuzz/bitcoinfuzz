LIBBITCOIN_DIR := ./libbitcoin-system
SRC_DIR := $(LIBBITCOIN_DIR)/src
INC_DIR := $(LIBBITCOIN_DIR)/include

# check for BOOST_INC_DIR environment variable
ifndef BOOST_INC_DIR
$(warning BOOST_INC_DIR is not set. Using system include paths for Boost.)
BOOST_INCLUDE_FLAG :=
else
BOOST_INCLUDE_FLAG := -I$(BOOST_INC_DIR)
endif

# source files
SRCS := $(shell find $(SRC_DIR) -name "*.cpp")
OBJS := $(SRCS:.cpp=.o)

# module source
MODULE_SRC := ./module.cpp
MODULE_OBJ := $(MODULE_SRC:.cpp=.o)

CXXFLAGS += -I$(INC_DIR) -std=c++20 -Wall -Wextra -fsanitize=fuzzer,address
CXXFLAGS += -I../../include
CXXFLAGS += $(BOOST_INCLUDE_FLAG)
CXXFLAGS += -I../bitcoin/secp256k1/include
CXXFLAGS += -DBOOST_JSON_STANDALONE -DBOOST_NO_JSON

# static lib
LIB := libbitcoin_module.a

all: $(LIB)

$(LIB): $(MODULE_OBJ) $(OBJS)
	ar rcs $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(MODULE_OBJ) $(LIB)

.PHONY: clean