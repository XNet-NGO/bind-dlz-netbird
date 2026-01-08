# Makefile for bind-dlz-netbird

# Compiler settings
CC = gcc
# Note: Static linking into a shared object (.so) requires dependencies (libcurl, libjansson) 
# to be compiled with -fPIC. On some distributions (Debian/Ubuntu), the default .a libraries 
# are NOT compiled with -fPIC, which will cause a linker error. 
# If that happens, you must recompile libcurl/libjansson from source with CFLAGS="-fPIC".
CFLAGS = -fPIC -Wall -Wextra -O2
LDFLAGS = -shared

# Attempt to determine necessary dependencies for static linking using pkg-config
PKG_CONFIG ?= pkg-config

# Get all dependencies (ssl, crypto, z, etc.) required by libcurl/jansson
# We strip direct -lcurl -ljansson because we will explicitly link them with -Bstatic
AUX_LIBS := $(shell $(PKG_CONFIG) --libs --static libcurl jansson 2>/dev/null | sed -e 's/-lcurl//g' -e 's/-ljansson//g' )

# Fallback if pkg-config fails
ifeq ($(AUX_LIBS),)
	AUX_LIBS = -lssl -lcrypto -lz
endif

# Link curl and jansson statically (-Bstatic), everything else dynamically (-Bdynamic)
LIBS = -Wl,-Bstatic -lcurl -ljansson -Wl,-Bdynamic $(AUX_LIBS) -lpthread

# Target library name
TARGET = netbird_dlz.so

# Source files
SRCS = netbird_dlz.c
OBJS = $(SRCS:.c=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

%.o: %.c dlz_minimal.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

# Installation hint (adjust path as needed for your BIND installation)
install: $(TARGET)
	@echo "Copying $(TARGET) to /usr/lib/bind/ (requires sudo)"
	# sudo cp $(TARGET) /usr/lib/bind/
