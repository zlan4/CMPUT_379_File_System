#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "fs-sim.h"
#include "disk-ops.h"
#include "inode-ops.h"

char current_disk_name[1000] = ""; // Name of currently mounted disk
Superblock superblock; // Superblock
uint8_t buffer[1024]; // Buffer size
int current_inode_index = 127; // Currently in the root directory
bool is_mounted = false; // File system not mounted yet
int disk_fd = -1; // File descriptor for disk file

// Performs comprehensive consistency checks on a file system superblock
int check_consistency(Superblock *sb, char *disk_name) {
    // Check 1: Free inodes must be all 0s
    for (int i = 0; i < 126; i++) {
        if (!(sb->inode[i].isused_size & 0x80)) {
            for (int j = 0; j < 5; j++) {
                if (sb->inode[i].name[j] != 0) {
                    return 1;
                }
            }
            if (sb->inode[i].isused_size != 0 || sb->inode[i].start_block != 0 || sb->inode[i].isdir_parent != 0) {
                return 1;
            }
        } else {
            // Inode is in use, check if name starts with zero byte
            if (sb->inode[i].name[0] == 0) {
                return 1;
            }
        }
    }
    // Check 2: File inodes must have valid start block and size
    for (int i = 0; i < 126; i++) {
        if (sb->inode[i].isused_size & 0x80) {
            bool is_dir = sb->inode[i].isdir_parent & 0x80;
            int size = sb->inode[i].isused_size & 0x7F;
            int start = sb->inode[i].start_block;
            if (!is_dir) {
                // Files must have valid block range (1-127)
                if (start < 1 || start > 127) {
                    return 2;
                }
                // File extends beyond disk
                if (start + size - 1 > 127) {
                    return 2;
                }
            } else {
                // Check 3: Directory size and start block must be zero
                if (size != 0 || start != 0) {
                    return 3;
                }
            }
        }
    }
    // Check 4: Parent inode validation
    for (int i = 0; i < 126; i++) {
        if (sb->inode[i].isused_size & 0x80) {
            int parent_index = sb->inode[i].isdir_parent & 0x7F;
            // An inode cannot be its own parent
            if (parent_index == i || parent_index == 126) {
                return 4;
            }
            // Parent must be valid
            if (parent_index != 127) {
                if (parent_index < 0 || parent_index > 125) {
                    return 4;
                }
                // Parent must exist and be a directory
                if (!(sb->inode[parent_index].isused_size & 0x80) || !(sb->inode[parent_index].isdir_parent & 0x80)) {
                    return 4;
                }
            }
        }
    }
    // Check 5: Unique names in each directory
    for (int parent = 0; parent < 126; parent++) {
        if (sb->inode[parent].isused_size & 0x80 && (sb->inode[parent].isdir_parent & 0x80)) {
            // Check all children of this directory for duplicates
            for (int i = 0; i < 126; i++) {
                if (sb->inode[i].isused_size & 0x80 && (sb->inode[i].isdir_parent & 0x7F) == parent) {
                    for (int j = i + 1; j < 126; j++) {
                        if (sb->inode[j].isused_size & 0x80 && (sb->inode[j].isdir_parent & 0x7F) == parent && 
                            memcmp(sb->inode[i].name, sb->inode[j].name, 5) == 0) {
                            return 5;
                        }
                    }
                }
            }
        }
    }
    // Check 6: Free space list consistency
    bool block_allocated[128] = {false};
    for (int i = 0; i < 128; i++) {
        int byte_index = i / 8;
        int bit_index = i % 8;
        bool is_allocated = (sb->free_block_list[byte_index] >> (7 - bit_index)) & 1;
        if (is_allocated) block_allocated[i] = true;
    }
    // Verify files use allocated blocks
    for (int i = 0; i < 126; i++) {
        if (sb->inode[i].isused_size & 0x80 && !(sb->inode[i].isdir_parent & 0x80)) {
            int start = sb->inode[i].start_block;
            int size = sb->inode[i].isused_size & 0x7F;
            for (int j = start; j < start + size; j++) {
                if (!block_allocated[j]) {
                    return 6;
                }
            }
        }
    }
    return 0; // All checks passed
}

// Mounts the file system residing on the specified virtual disk
void fs_mount(char *new_disk_name) {
    int new_fd = open(new_disk_name, O_RDWR); // Try to open the new disk
    if (new_fd == -1) {
        fprintf(stderr, "Error: Cannot find disk %s\n", new_disk_name);
        return;
    }
    Superblock new_sb; // Read superblock from new disk
    uint8_t block_data[1024];
    // Read block 0 (superblock)
    if (lseek(new_fd, 0, SEEK_SET) == -1) {
        close(new_fd);
        return;
    }
    if (read(new_fd, block_data, 1024) != 1024) {
        close(new_fd);
        return;
    }
    memcpy(&new_sb, block_data, sizeof(Superblock));
    int error_code = check_consistency(&new_sb, new_disk_name); // Perform consistency checks
    if (error_code != 0) {
        close(new_fd);
        fprintf(stderr, "Error: File system in %s is inconsistent (error code: %d)\n", new_disk_name, error_code);
        return;
    }
    // New disk is valid, switch to it
    if (is_mounted) {
        close_disk();
    }
    disk_fd = new_fd; // Update global disk file descriptor
    memcpy(&superblock, &new_sb, sizeof(Superblock)); // Copy new superblock to global superblock
    strcpy(current_disk_name, new_disk_name);
    current_inode_index = 127;
    is_mounted = true;
}

// Creates a new file or directory in the current working directory with the given name and the given number of blocks, and stores the attributes in the first available inode
void fs_create(char name[5], int size) {
    if (!is_mounted) {
        fprintf(stderr, "Error: No file system is mounted\n");
        return;
    }
    int inode_index = find_free_inode(); // Find free inode
    if (inode_index == -1) {
        fprintf(stderr, "Error: Superblock in disk %s is full, cannot create %.5s\n", current_disk_name, name);
        return;
    }
    // Check if name is reserved or not unique
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0 || !is_name_unique_in_directory(current_inode_index, name)) {
        fprintf(stderr, "Error: File or directory %.5s already exists\n", name);
        return;
    }
    int actual_size = size;
    if (size == 0) {
        // Directory
        superblock.inode[inode_index].isdir_parent = 0x80 | (current_inode_index == 127 ? 127 : current_inode_index);
        superblock.inode[inode_index].start_block = 0;
    } else {
        int start_block = find_contiguous_blocks(size); // File: find contiguous blocks
        if (start_block == -1) {
            fprintf(stderr, "Error: Cannot allocate %d blocks on %s\n", size, current_disk_name);
            return;
        }
        superblock.inode[inode_index].start_block = start_block;
        uint8_t parent = (current_inode_index == 127 ? 127 : current_inode_index);
        superblock.inode[inode_index].isdir_parent = parent; // MSB 0 for files
        update_free_blocks(start_block, size, true);
    }
    memcpy(superblock.inode[inode_index].name, name, 5); // Set inode fields
    superblock.inode[inode_index].isused_size = 0x80 | (actual_size & 0x7F);
    write_block(0, (uint8_t*)&superblock); // Write updated superblock back to disk
}

// Deletes the file or directory with the given name in the current working directory
void fs_delete(char name[5]) {
    if (!is_mounted) {
        fprintf(stderr, "Error: No file system is mounted\n");
        return;
    }
    Inode *inode = find_inode_by_name(name, current_inode_index); // Find the inode by name in current directory
    if (!inode) {
        fprintf(stderr, "Error: File or directory %.5s does not exist\n", name);
        return;
    }
    int inode_index = -1;
    // Find index of the inode in superblock array
    for (int i = 0; i < 126; i++) {
        if (&superblock.inode[i] == inode) {
            inode_index = i;
            break;
        }
    }
    // Perform recursive deletion (handles both files and directories)
    if (inode_index != -1) {
        recursive_delete(inode_index);
        write_block(0, (uint8_t*)&superblock); // Persist changes
    }
}

// Opens the file with the given name and reads the block num-th block of the file into the buffer
void fs_read(char name[5], int block_num) {
    if (!is_mounted) {
        fprintf(stderr, "Error: No file system is mounted\n");
        return;
    }
    Inode *inode = find_inode_by_name(name, current_inode_index);
    // Find file inode (must be regular file, not directory)
    if (!inode || (inode->isdir_parent & 0x80)) {
        fprintf(stderr, "Error: File %.5s does not exist\n", name);
        return;
    }
    int size = inode->isused_size & 0x7F;
    // Validate block number is within file bounds
    if (block_num < 0 || block_num >= size) {
        fprintf(stderr, "Error: %.5s does not have block %d\n", name, block_num);
        return;
    }
    int block_to_read = inode->start_block + block_num; // Calculate physical block
    read_block(block_to_read, buffer); // Read into global buffer
}

// Opens the file with the given name and writes the content of the buffer to the block num-th block of the file
void fs_write(char name[5], int block_num) {
    if (!is_mounted) {
        fprintf(stderr, "Error: No file system is mounted\n");
        return;
    }
    Inode *inode = find_inode_by_name(name, current_inode_index);
    // Find file inode (must be regular file, not directory)
    if (!inode || (inode->isdir_parent & 0x80)) {
        fprintf(stderr, "Error: File %.5s does not exist\n", name);
        return;
    }
    int size = inode->isused_size & 0x7F;
    // Validate block number is within file bounds
    if (block_num < 0 || block_num >= size) {
        fprintf(stderr, "Error: %.5s does not have block %d\n", name, block_num);
        return;
    }
    int block_to_write = inode->start_block + block_num; // Calculate physical block
    write_block(block_to_write, buffer); // Write into global buffer
}

// Flushes the buffer by zeroing it and writes the new bytes into the buffer
void fs_buff(uint8_t buff[1024]) {
	if (!is_mounted) {
        fprintf(stderr, "Error: No file system is mounted\n");
        return;
    }
    memset(buffer, 0, 1024); // Clear buffer
    memcpy(buffer, buff, 1024); // Copy new data
}

// Lists all files and directories that exist in the current directory, including the special directories . and ..
void fs_ls(void) {
    if (!is_mounted) {
        fprintf(stderr, "Error: No file system is mounted\n");
        return;
    }
    int parent_index; // Calculate parent directory index
    if (current_inode_index == 127) {
        parent_index = 127;
    } else {
        parent_index = superblock.inode[current_inode_index].isdir_parent & 0x7F;
    }
    // Print special entries
    printf("%-5s %3d\n", ".", count_children(current_inode_index));
    printf("%-5s %3d\n", "..", count_children(parent_index));
    // List all entries in current directory
    for (int i = 0; i < 126; i++) {
        if (superblock.inode[i].isused_size & 0x80) {
            int file_parent = superblock.inode[i].isdir_parent & 0x7F;
            if (file_parent == current_inode_index) {
                if (superblock.inode[i].isdir_parent & 0x80) {
                    printf("%-5.5s %3d\n", superblock.inode[i].name, count_children(i));
                } else {
                    int size = superblock.inode[i].isused_size & 0x7F;
                    printf("%-5.5s %3d KB\n", superblock.inode[i].name, size);
                }
            }
        }
    }
}

// Re-organizes the data blocks such that there is no free block between the used blocks, and between the superblock and the used blocks
void fs_defrag(void) {
    if (!is_mounted) {
        fprintf(stderr, "Error: No file system is mounted\n");
        return;
    }
    typedef struct {
        int inode_index; // Index in inode table
        int start_block; // Current physical start block
        int size; // Size in blocks
    } FileEntry;
    FileEntry files[126];
    int file_count = 0;
    // Gather all regular files (not directories)
    for (int i = 0; i < 126; i++) {
        if (superblock.inode[i].isused_size & 0x80 && !(superblock.inode[i].isdir_parent & 0x80)) {
            files[file_count].inode_index = i;
            files[file_count].start_block = superblock.inode[i].start_block;
            files[file_count].size = superblock.inode[i].isused_size & 0x7F;
            file_count++;
        }
    }
    // Sort files by current location
    for (int i = 0; i < file_count - 1; i++) {
        for (int j = 0; j < file_count - i - 1; j++) {
            if (files[j].start_block > files[j + 1].start_block) {
                FileEntry temp = files[j];
                files[j] = files[j + 1];
                files[j + 1] = temp;
            }
        }
    }
    int next_free_block = 1; // Start after superblock (block 0)
    for (int i = 0; i < file_count; i++) {
        int old_start = files[i].start_block;
        int new_start = next_free_block;
        int file_size = files[i].size;
        // Only move if file is not already in correct position
        if (old_start != new_start) {
            // Copy each block to new location
            for (int block_offset = 0; block_offset < file_size; block_offset++) {
                int old_block = old_start + block_offset;
                int new_block = new_start + block_offset;
                uint8_t block_data[1024];
                read_block(old_block, block_data); // Read from old location
                write_block(new_block, block_data); // Write to new location
                uint8_t zero_block[1024] = {0}; // Zero out old block
                write_block(old_block, zero_block);
            }
            superblock.inode[files[i].inode_index].start_block = new_start; // Update inode with new location
            update_free_blocks(old_start, file_size, false); // Free old blocks
            update_free_blocks(new_start, file_size, true); // Allocate new blocks
        }
        next_free_block += file_size; // Move pointer for next file
    }
    write_block(0, (uint8_t*)&superblock); // Persist changes to disk
}

// Changes the current working directory to a directory with the specified name in the current working directory
void fs_cd(char name[5]) {
    if (!is_mounted) {
        fprintf(stderr, "Error: No file system is mounted\n");
        return;
    }
    // Current directory
    if (strcmp(name, ".") == 0) {
        return;
    // Parent directory
    } else if (strcmp(name, "..") == 0) {
        if (current_inode_index != 127) {
            current_inode_index = superblock.inode[current_inode_index].isdir_parent & 0x7F;
        }
        return;
    }
    Inode *inode = find_inode_by_name(name, current_inode_index);
    // Find named directory in current directory
    if (!inode || !(inode->isdir_parent & 0x80)) {
        fprintf(stderr, "Error: Directory %.5s does not exist\n", name);
        return;
    }
    // Update current directory index
    for (int i = 0; i < 126; i++) {
        if (&superblock.inode[i] == inode) {
            current_inode_index = i;
            break;
        }
    }
}