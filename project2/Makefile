# Compiler
CC = gcc

# Compiler Flags
CFLAGS = -Wall -Wextra -O2

# Executable Names
TARGET1 = fs_operations
TARGET2 = createFileSystem

# Source Files
SRC1 = fs_operations.c
SRC2 = create_fs.c

# Object Files
OBJ1 = $(SRC1:.c=.o)
OBJ2 = $(SRC2:.c=.o)

# Default Target
all: $(TARGET1) $(TARGET2)

# fs_operations Target
$(TARGET1): $(OBJ1)
	$(CC) $(CFLAGS) -o $@ $^

# createFileSystem Target
$(TARGET2): $(OBJ2)
	$(CC) $(CFLAGS) -o $@ $^

# Clean Target
clean:
	rm -f $(OBJ1) $(OBJ2) $(TARGET1) $(TARGET2) *.dat

.PHONY: all clean
