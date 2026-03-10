CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c11
LDFLAGS = -lm

# Source files
SRC_DIR = src
INCLUDE_DIR = include

SOURCES = \
	$(SRC_DIR)/terrain.c \
	$(SRC_DIR)/dem_hgt.c \
	$(SRC_DIR)/stl_writer.c

OBJECTS = $(SOURCES:.c=.o)

# Targets
.PHONY: all clean cli lib test

all: cli

# Build CLI executable
cli: terrain_cli
terrain_cli: terrain_cli.c $(SOURCES)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -o $@ $^ $(LDFLAGS)

# Build static library
lib: libterrain.a
libterrain.a: $(OBJECTS)
	ar rcs $@ $^

# Compile object files
$(SRC_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c -o $@ $<

# Test with sample coordinates (Norway - Trolltunga area)
test: terrain_cli
	./terrain_cli \
		--nw 60.15,6.70 \
		--se 60.10,6.80 \
		--format rect \
		--resolution auto \
		--zscale 2.0 \
		--output test_terrain.stl

# Clean build artifacts
clean:
	rm -f terrain_cli libterrain.a $(OBJECTS)
	rm -f test_terrain.stl

# Install (optional)
PREFIX ?= /usr/local
install: terrain_cli libterrain.a
	install -d $(PREFIX)/bin
	install -d $(PREFIX)/lib
	install -d $(PREFIX)/include/libterrain
	install terrain_cli $(PREFIX)/bin/
	install libterrain.a $(PREFIX)/lib/
	install $(INCLUDE_DIR)/*.h $(PREFIX)/include/libterrain/
