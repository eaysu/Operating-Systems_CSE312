#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h> // For getpass function

#define MAX_FILENAME_LEN 255
#define BLOCK_SIZE 1024
#define MAX_BLOCKS (4 * 1024 * 1024 / BLOCK_SIZE) // 4096 blocks for 1 KB block size
#define FAT_ENTRIES (MAX_BLOCKS + 1) // FAT has an entry for each block plus one

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
    unsigned short fat[FAT_ENTRIES]; // FAT table, 12-bit entries, stored in 16-bit slots for simplicity
} FAT12;

typedef struct {
    unsigned int block_size;
    unsigned int total_blocks;
    unsigned int free_blocks;
    FAT12 fat_table;
} SuperBlock;

typedef struct {
    SuperBlock superblock;
    DirectoryEntry directory_entries[MAX_BLOCKS];
    unsigned char data_blocks[MAX_BLOCKS][BLOCK_SIZE]; // Ensure BLOCK_SIZE is consistent
} FileSystem;

FileSystem fs;

void loadFileSystem(const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("Failed to open file system");
        exit(EXIT_FAILURE);
    }
    if (fread(&fs, sizeof(FileSystem), 1, file) != 1) {
        perror("Failed to read file system");
        fclose(file);
        exit(EXIT_FAILURE);
    }
    fclose(file);
}

void saveFileSystem(const char *filename) {
    FILE *file = fopen(filename, "wb");
    if (!file) {
        perror("Failed to save file system");
        exit(EXIT_FAILURE);
    }
    if (fwrite(&fs, sizeof(FileSystem), 1, file) != 1) {
        perror("Failed to write file system");
        fclose(file);
        exit(EXIT_FAILURE);
    }
    fclose(file);
}

void listDirectory(const char *path) {
    if (strcmp(path, "\\") == 0 || strcmp(path, "") == 0) {
        // List root directories
        printf("Listing directory: %s\n", path);
        for (int i = 0; i < MAX_BLOCKS; i++) {
            if (fs.directory_entries[i].filename[0] != '\0' && strchr(fs.directory_entries[i].filename, '\\') == NULL) {
                printf("%s\n", fs.directory_entries[i].filename);
            }
        }
    } else {
        // List subdirectories or files within the specified path
        printf("Listing directory: %s\n", path);
        int path_len = strlen(path);
        for (int i = 0; i < MAX_BLOCKS; i++) {
            if (strncmp(fs.directory_entries[i].filename, path, path_len) == 0 &&
                (fs.directory_entries[i].filename[path_len] == '\\' || fs.directory_entries[i].filename[path_len] == '\0')) {
                printf("%s\n", fs.directory_entries[i].filename + path_len + 1);
            }
        }
    }
}

int directoryExists(const char *path) {
    for (int i = 0; i < MAX_BLOCKS; i++) {
        if (strcmp(fs.directory_entries[i].filename, path) == 0) {
            return 1;
        }
    }
    return 0;
}

void makeDirectory(const char *path) {
    char *token, *subpath, *saveptr;
    char fullpath[MAX_FILENAME_LEN + 1] = "";
    int directory_created = 0;

    subpath = strdup(path);

    token = strtok_r(subpath, "\\", &saveptr);
    while (token != NULL) {
        if (fullpath[0] != '\0') {
            strcat(fullpath, "\\");
        }
        strcat(fullpath, token);
        printf("Path: %s\n", fullpath);

        if (!directoryExists(fullpath)) {
            // Check if we are trying to create a subdirectory without its parent
            if (strrchr(fullpath, '\\') != NULL) {
                char parent[MAX_FILENAME_LEN + 1] = "";
                strncpy(parent, fullpath, strrchr(fullpath, '\\') - fullpath);
                parent[strrchr(fullpath, '\\') - fullpath] = '\0'; // null-terminate the parent path
                if (!directoryExists(parent)) {
                    printf("Error: Parent directory %s does not exist\n", parent);
                    free(subpath);
                    return;
                }
            }

            for (int i = 0; i < MAX_BLOCKS; i++) {
                if (fs.directory_entries[i].filename[0] == '\0') {
                    strcpy(fs.directory_entries[i].filename, fullpath);
                    fs.directory_entries[i].size = 0;
                    fs.directory_entries[i].permissions = 0755;
                    fs.directory_entries[i].creation_time = time(NULL);
                    fs.directory_entries[i].modification_time = time(NULL);
                    fs.directory_entries[i].first_block = 0;
                    fs.directory_entries[i].is_password_protected = 0;
                    memset(fs.directory_entries[i].password_hash, 0, 32);
                    printf("Directory created: %s\n", fullpath);
                    directory_created = 1;
                    break;
                }
            }
        }

        token = strtok_r(NULL, "\\", &saveptr);
    }

    if (!directory_created) {
        printf("Directory already exists: %s\n", path);
    }

    free(subpath);
}

void removeDirectory(const char *dirname) {
    for (int i = 0; i < MAX_BLOCKS; i++) {
        if (strcmp(fs.directory_entries[i].filename, dirname) == 0) {
            memset(&fs.directory_entries[i], 0, sizeof(DirectoryEntry));
            printf("Directory removed: %s\n", dirname);
            return;
        }
    }
    printf("Failed to remove directory: %s not found\n", dirname);
}

int findFreeBlock() {
    for (unsigned int i = 2; i < fs.superblock.total_blocks; i++) { // FAT-12 reserves first 2 entries
        if (fs.superblock.fat_table.fat[i] == 0) {
            fs.superblock.fat_table.fat[i] = 0xFFF; // Mark block as end-of-chain
            fs.superblock.free_blocks--;
            return i;
        }
    }
    return -1;
}

void freeBlock(unsigned int block_index) {
    while (block_index != 0xFFF) {
        unsigned int next_block = fs.superblock.fat_table.fat[block_index];
        fs.superblock.fat_table.fat[block_index] = 0;
        block_index = next_block;
        fs.superblock.free_blocks++;
    }
}

void writeFile(const char *filename, const char *source_path, const char *password) {
    FILE *srcFile = fopen(source_path, "rb");
    if (!srcFile) {
        perror("Failed to open source file");
        return;
    }

    fseek(srcFile, 0, SEEK_END);
    unsigned int fileSize = ftell(srcFile);
    fseek(srcFile, 0, SEEK_SET);

    int first_block_index = findFreeBlock();
    if (first_block_index == -1) {
        printf("Error: No free blocks available\n");
        fclose(srcFile);
        return;
    }

    for (int i = 0; i < MAX_BLOCKS; i++) {
        if (fs.directory_entries[i].filename[0] == '\0') {
            strcpy(fs.directory_entries[i].filename, filename);
            fs.directory_entries[i].size = fileSize;
            fs.directory_entries[i].permissions = 0644;
            fs.directory_entries[i].creation_time = time(NULL);
            fs.directory_entries[i].modification_time = time(NULL);
            fs.directory_entries[i].first_block = first_block_index;
            fs.directory_entries[i].is_password_protected = (password != NULL);
            if (password != NULL) {
                strncpy(fs.directory_entries[i].password_hash, password, 32);  // In practice, hash the password
            } else {
                memset(fs.directory_entries[i].password_hash, 0, 32);
            }

            unsigned int remaining_size = fileSize;
            unsigned int current_block = first_block_index;
            while (remaining_size > 0) {
                unsigned int bytes_to_write = (remaining_size < BLOCK_SIZE) ? remaining_size : BLOCK_SIZE;
                char buffer[BLOCK_SIZE];
                size_t bytesRead = fread(buffer, 1, bytes_to_write, srcFile);
                if (bytesRead != bytes_to_write) {
                    printf("Error: Failed to read from source file\n");
                    fclose(srcFile);
                    return;
                }

                // Write buffer to the block
                memcpy(&fs.data_blocks[current_block], buffer, bytes_to_write);

                remaining_size -= bytes_to_write;
                if (remaining_size > 0) {
                    int next_block = findFreeBlock();
                    if (next_block == -1) {
                        printf("Error: No free blocks available\n");
                        fclose(srcFile);
                        return;
                    }
                    current_block = next_block;
                }
            }

            printf("File written: %s\n", filename);
            fclose(srcFile);
            return;
        }
    }
    printf("Failed to write file: No free entries\n");
    fclose(srcFile);
}

void readFile(const char *filename, const char *destination, const char *password) {
    for (int i = 0; i < MAX_BLOCKS; i++) {
        if (strcmp(fs.directory_entries[i].filename, filename) == 0) {
            if (fs.directory_entries[i].is_password_protected) {
                if (password == NULL || strncmp(fs.directory_entries[i].password_hash, password, 32) != 0) {
                    printf("Failed to read file: Incorrect or missing password\n");
                    return;
                }
            }
            if ((fs.directory_entries[i].permissions & 0400) == 0) {
                printf("Failed to read file: Permission denied\n");
                return;
            }

            FILE *destFile = fopen(destination, "wb");
            if (!destFile) {
                perror("Failed to open destination file");
                return;
            }

            unsigned int block_index = fs.directory_entries[i].first_block;
            unsigned int remaining_size = fs.directory_entries[i].size;
            char buffer[BLOCK_SIZE];

            while (remaining_size > 0) {
                if (block_index >= MAX_BLOCKS) {
                    printf("Error: Block index out of range\n");
                    fclose(destFile);
                    return;
                }

                unsigned int bytes_to_read = (remaining_size < BLOCK_SIZE) ? remaining_size : BLOCK_SIZE;
                memcpy(buffer, &fs.data_blocks[block_index], bytes_to_read);

                if (fwrite(buffer, 1, bytes_to_read, destFile) != bytes_to_read) {
                    perror("Failed to write to destination file");
                    fclose(destFile);
                    return;
                }

                remaining_size -= bytes_to_read;
                // Move to the next block (in real implementation, get the next block index)
                block_index++;
            }

            fclose(destFile);
            printf("File read successfully: %s\n", filename);
            return;
        }
    }
    printf("Failed to read file: %s not found\n", filename);
}


void deleteFile(const char *filename) {
    for (int i = 0; i < MAX_BLOCKS; i++) {
        if (strcmp(fs.directory_entries[i].filename, filename) == 0) {
            memset(&fs.directory_entries[i], 0, sizeof(DirectoryEntry));
            printf("File deleted: %s\n", filename);
            return;
        }
    }
    printf("Failed to delete file: %s not found\n", filename);
}

void changePermissions(const char *filename, const char *permissions) {
    for (int i = 0; i < MAX_BLOCKS; i++) {
        if (strcmp(fs.directory_entries[i].filename, filename) == 0) {
            if (permissions[0] == '+') {
                if (strchr(permissions, 'r')) {
                    fs.directory_entries[i].permissions |= 0400; // Add read permission
                }
                if (strchr(permissions, 'w')) {
                    fs.directory_entries[i].permissions |= 0200; // Add write permission
                }
            } else if (permissions[0] == '-') {
                if (strchr(permissions, 'r')) {
                    fs.directory_entries[i].permissions &= ~0400; // Remove read permission
                }
                if (strchr(permissions, 'w')) {
                    fs.directory_entries[i].permissions &= ~0200; // Remove write permission
                }
            }
            printf("Permissions changed: %s to %o\n", filename, fs.directory_entries[i].permissions);
            return;
        }
    }
    printf("Failed to change permissions: %s not found\n", filename);
}

void addPassword(const char *filename, const char *password) {
    for (int i = 0; i < MAX_BLOCKS; i++) {
        if (strcmp(fs.directory_entries[i].filename, filename) == 0) {
            fs.directory_entries[i].is_password_protected = 1;
            strncpy(fs.directory_entries[i].password_hash, password, 32);  // In practice, hash the password
            printf("Password added to file: %s\n", filename);
            return;
        }
    }
    printf("Failed to add password: %s not found\n", filename);
}

void dumpFileSystem() {
    printf("Dumping file system:\n");
    printf("Block size: %u\n", fs.superblock.block_size);
    printf("Total blocks: %u\n", fs.superblock.total_blocks);
    printf("Free blocks: %u\n", fs.superblock.free_blocks);
    for (int i = 0; i < MAX_BLOCKS; i++) {
        if (fs.directory_entries[i].filename[0] != '\0') {
            printf("Filename: %s, Size: %u, Permissions: %o, Creation time: %ld, Modification time: %ld, First block: %u\n",
                   fs.directory_entries[i].filename, fs.directory_entries[i].size, fs.directory_entries[i].permissions,
                   fs.directory_entries[i].creation_time, fs.directory_entries[i].modification_time, fs.directory_entries[i].first_block);
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: %s <filesystem> <operation> [<args>...]\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *fs_name = argv[1];
    const char *operation = argv[2];

    loadFileSystem(fs_name);

    if (strcmp(operation, "dir") == 0) {
        listDirectory((argc == 4) ? argv[3] : "\\");
    } else if (strcmp(operation, "mkdir") == 0) {
        if (argc != 4) {
            printf("Usage: %s %s <dirname>\n", argv[0], operation);
            return EXIT_FAILURE;
        }
        makeDirectory(argv[3]);
    } else if (strcmp(operation, "rmdir") == 0) {
        if (argc != 4) {
            printf("Usage: %s %s <dirname>\n", argv[0], operation);
            return EXIT_FAILURE;
        }
        removeDirectory(argv[3]);
    } else if (strcmp(operation, "write") == 0) {
        if (argc != 5 && argc != 6) {
            printf("Usage: %s %s <filename> <data> [password]\n", argv[0], operation);
            return EXIT_FAILURE;
        }
        writeFile(argv[3], argv[4], (argc == 6) ? argv[5] : NULL);
    } else if (strcmp(operation, "read") == 0) {
        if (argc != 5 && argc != 6) {
            printf("Usage: %s %s <filename> <destination> [password]\n", argv[0], operation);
            return EXIT_FAILURE;
        }
        readFile(argv[3], argv[4], (argc == 6) ? argv[5] : NULL);
    } else if (strcmp(operation, "del") == 0) {
        if (argc != 4) {
            printf("Usage: %s %s <filename>\n", argv[0], operation);
            return EXIT_FAILURE;
        }
        deleteFile(argv[3]);
    } else if (strcmp(operation, "chmod") == 0) {
        if (argc != 5) {
            printf("Usage: %s %s <filename> <permissions>\n", argv[0], operation);
            return EXIT_FAILURE;
        }
        changePermissions(argv[3], argv[4]);
    } else if (strcmp(operation, "addpw") == 0) {
        if (argc != 5) {
            printf("Usage: %s %s <filename> <password>\n", argv[0], operation);
            return EXIT_FAILURE;
        }
        addPassword(argv[3], argv[4]);
    } else if (strcmp(operation, "dumpe2fs") == 0) {
        dumpFileSystem();
    } else {
        printf("Unknown operation: %s\n", operation);
        return EXIT_FAILURE;
    }

    saveFileSystem(fs_name);

    return EXIT_SUCCESS;
}