#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_FILENAME_LEN 255

// Define block sizes
#define BLOCK_SIZE_512 512
#define BLOCK_SIZE_1024 1024

// Define maximum number of blocks for a 4MB file system
#define MAX_BLOCKS_512 (4 * 1024 * 1024 / BLOCK_SIZE_512)  // 8192 blocks for 512-byte block size
#define MAX_BLOCKS_1024 (4 * 1024 * 1024 / BLOCK_SIZE_1024) // 4096 blocks for 1 KB block size

#define FAT_ENTRIES_512 (MAX_BLOCKS_512 + 1)
#define FAT_ENTRIES_1024 (MAX_BLOCKS_1024 + 1)

typedef struct {
    char filename[MAX_FILENAME_LEN + 1];
    unsigned int size;
    unsigned int permissions;
    time_t creation_time;
    time_t modification_time;
    unsigned int first_block;
    unsigned int is_password_protected;
    char password_hash[32];  // SHA-256 hash (placeholder)
} DirectoryEntry;

typedef struct {
    unsigned short *fat; // FAT table, 12-bit entries, stored in 16-bit slots for simplicity
} FAT12;

typedef struct {
    unsigned int block_size;
    unsigned int total_blocks;
    unsigned int free_blocks;
    FAT12 fat_table;
} SuperBlock;

typedef struct {
    SuperBlock superblock;
    DirectoryEntry *directory_entries;
    unsigned char (*data_blocks)[];
} FileSystem;

void createFileSystem(const char *filename, unsigned int block_size) {
    FileSystem fs;
    memset(&fs, 0, sizeof(FileSystem));

    // Initialize super block
    fs.superblock.block_size = block_size;
    fs.superblock.total_blocks = (block_size == BLOCK_SIZE_512) ? MAX_BLOCKS_512 : MAX_BLOCKS_1024;
    fs.superblock.free_blocks = fs.superblock.total_blocks;

    // Allocate memory for FAT table and directory entries
    fs.superblock.fat_table.fat = calloc(fs.superblock.total_blocks + 1, sizeof(unsigned short));
    fs.directory_entries = calloc(fs.superblock.total_blocks, sizeof(DirectoryEntry));
    fs.data_blocks = calloc(fs.superblock.total_blocks, block_size);

    if (!fs.superblock.fat_table.fat || !fs.directory_entries || !fs.data_blocks) {
        perror("Memory allocation failed");
        exit(EXIT_FAILURE);
    }

    // Mark all FAT entries as free (0)
    for (unsigned int i = 0; i < fs.superblock.total_blocks + 1; i++) {
        fs.superblock.fat_table.fat[i] = 0;
    }

    // Create the file system file
    FILE *file = fopen(filename, "wb");
    if (!file) {
        perror("Failed to create file");
        exit(EXIT_FAILURE);
    }

    // Write the superblock to the file
    fwrite(&fs.superblock, sizeof(SuperBlock), 1, file);

    // Write the FAT table to the file
    fwrite(fs.superblock.fat_table.fat, sizeof(unsigned short), fs.superblock.total_blocks + 1, file);

    // Write the directory entries to the file
    fwrite(fs.directory_entries, sizeof(DirectoryEntry), fs.superblock.total_blocks, file);

    // Write the data blocks to the file
    fwrite(fs.data_blocks, block_size, fs.superblock.total_blocks, file);

    fclose(file);
    printf("File system created: %s with block size %u bytes\n", filename, block_size);

    // Free allocated memory
    free(fs.superblock.fat_table.fat);
    free(fs.directory_entries);
    free(fs.data_blocks);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <block size (0.5 or 1)> <file system name>\n", argv[0]);
        return EXIT_FAILURE;
    }

    float block_size_kb = atof(argv[1]);
    unsigned int block_size = (block_size_kb == 0.5) ? BLOCK_SIZE_512 : BLOCK_SIZE_1024;

    if (block_size != BLOCK_SIZE_512 && block_size != BLOCK_SIZE_1024) {
        printf("Supported block sizes are 0.5 KB and 1 KB\n");
        return EXIT_FAILURE;
    }

    char *fs_name = argv[2];
    createFileSystem(fs_name, block_size);
    return EXIT_SUCCESS;
}
