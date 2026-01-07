# Makefile for bind-dlz-netbird

# Compiler settings
CC = gcc
CFLAGS = -fPIC -Wall -Wextra -O2
LDFLAGS = -shared
LIBS = -lcurl -ljansson -lpthread

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
