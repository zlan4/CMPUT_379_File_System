#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "fs-sim.h"
#include <stdbool.h>

// Open disk file for reading and writing
int open_disk(const char *filename) {
    disk_fd = open(filename, O_RDWR); // Open file with read/write access
    return disk_fd; // Returns file descriptor
}

// Close the disk file
void close_disk(void) {
    if (disk_fd != -1) {
        close(disk_fd);
        disk_fd = -1; // Reset to invalid descriptor
    }
}

// Reads a 1024-byte block from the disk into memory
void read_block(int block_num, uint8_t *data) {
    if (disk_fd == -1) {
        return; // No disk open, silent failure
    }
    off_t offset = block_num * 1024; // Calculate byte offset: block number * 1024 bytes per block
    // Move file pointer to the beginning of the specified block
    if (lseek(disk_fd, offset, SEEK_SET) == -1) {
        return;
    }
    read(disk_fd, data, 1024); // Read exactly 1024 bytes (one block) from current file position
}

// Writes a 1024-byte block from memory to disk
void write_block(int block_num, uint8_t *data) {
    if (disk_fd == -1) {
        return; // No disk open, silent failure
    }
    off_t offset = block_num * 1024; // Calculate byte offset: block number * 1024 bytes per block
    // Move file pointer to the beginning of the specified block
    if (lseek(disk_fd, offset, SEEK_SET) == -1) {
        return; // Seek failed, silent failure
    }
    write(disk_fd, data, 1024); // Write exactly 1024 bytes (one block) from current file position
}

// Updates the free block bitmap for a contiguous range of blocks
void update_free_blocks(int start, int size, bool allocated) {
    // Loop through each block in the range to update
    for (int i = 0; i < size; i++) {
        int block = start + i; // Calculate the actual block number (start + offset)
        int byte_index = block / 8; // Calculate which byte in the free_block_list contains this block's bit
        int bit_index = block % 8; // Calculate which bit within the byte represents this block
        if (allocated) {
            superblock.free_block_list[byte_index] |= (1 << (7 - bit_index)); // Mark block as allocated: set the bit to 1
        } else {
            superblock.free_block_list[byte_index] &= ~(1 << (7 - bit_index)); // Mark block as free: set the bit to 0
        }
    }
}

// Finds a contiguous region of free blocks in the bitmap
int find_contiguous_blocks(int size) {
    if (size <= 0) {
        return -1; // Invalid size
    }
    // Search for contiguous free blocks starting from block 1
    for (int start = 1; start <= 127 - size + 1; start++) {
        bool found = true;
        // Check if all size blocks starting from start are free
        for (int i = 0; i < size; i++) {
            int block = start + i;
            int byte_index = block / 8;
            int bit_index = block % 8;
            // Check if bit is set (1 = allocated, 0 = free)
            if ((superblock.free_block_list[byte_index] >> (7 - bit_index)) & 1) {
                found = false; // Block is allocated
                break;
            }
        }
        // If a contiguous region of free blocks is found, return the start position
        if (found) {
            return start;
        }
    }
    return -1; // No contiguous free blocks found
}