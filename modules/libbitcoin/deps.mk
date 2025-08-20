# Library dependencies for libbitcoin module

UNAME_S := $(shell uname -s)

# Use environment variables for Boost paths with platform-specific fallbacks
ifeq ($(UNAME_S), Darwin)
# macOS-specific paths and libraries
BOOST_LIB_PATH := $(or $(BOOST_LIB_DIR),/opt/homebrew/opt/boost/lib)
LIBBITCOIN_LDFLAGS = -L/opt/homebrew/opt/openssl@3/lib \
                     -L$(BOOST_LIB_PATH) \
                     -L/opt/homebrew/lib
LIBBITCOIN_LIBS = -lssl -lcrypto \
                  -lboost_system -lboost_thread-mt -lboost_program_options
else
# Linux-specific libraries
BOOST_LIB_PATH := $(or $(BOOST_LIB_DIR),/usr/lib/x86_64-linux-gnu)
LIBBITCOIN_LDFLAGS = -L$(BOOST_LIB_PATH)
LIBBITCOIN_LIBS = -lssl -lcrypto \
                  -lboost_system -lboost_thread -lboost_program_options
endif

# Add libsodium using pkg-config
LIBBITCOIN_LIBS += $(shell pkg-config --silence-errors --libs libsodium 2>/dev/null || echo "-lsodium")

# Combined flags for easy use
LIBBITCOIN_ALL_LIBS = $(LIBBITCOIN_LDFLAGS) $(LIBBITCOIN_LIBS)
