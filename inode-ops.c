#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fs-sim.h"
#include "disk-ops.h"

int find_free_inode(void) {
    for (int i = 0; i < 126; i++) {
        // Check if current inode is free (MSB of isused_size is 0)
        if (!(superblock.inode[i].isused_size & 0x80)) {
            return i; // Return index of first free inode found
        }
    }
    return -1; // No free inodes available
}

bool is_name_unique_in_directory(int parent_inode, char name[5]) {
    // Loop through all inodes to check for name conflicts
    for (int i = 0; i < 126; i++) {
        // Check if current inode is in use
        if (superblock.inode[i].isused_size & 0x80) {
            int file_parent = superblock.inode[i].isdir_parent & 0x7F;
            // Check if this file/directory has the same parent and same name
            if (file_parent == parent_inode && 
                memcmp(superblock.inode[i].name, name, 5) == 0) {
                return false; // Name is not unique
            }
        }
    }
    return true; // Name is unique
}

Inode* find_inode_by_name(char name[5], int parent_inode) {
    // Loop through all inodes to find matching name
    for (int i = 0; i < 126; i++) {
        if (superblock.inode[i].isused_size & 0x80) {
            int file_parent = superblock.inode[i].isdir_parent & 0x7F;
            // Check if this file/directory has the same parent and same name
            if (file_parent == parent_inode && memcmp(superblock.inode[i].name, name, 5) == 0) {
                return &superblock.inode[i]; // Return pointer to the matching inode
            }
        }
    }
    return NULL; // No matching inode
}

void recursive_delete(int inode_index) {
    if (inode_index < 0 || inode_index >= 126) {
        return;
    }
    Inode *inode = &superblock.inode[inode_index];
    if (inode->isdir_parent & 0x80) {
        // Directory - delete all children first
        for (int i = 0; i < 126; i++) {
            // Skip the current directory and only check used inodes
            if (i != inode_index && superblock.inode[i].isused_size & 0x80) {
                // Extract parent index of child
                int parent = superblock.inode[i].isdir_parent & 0x7F;
                // Check if this inode is a child of the directory being deleted
                if (parent == inode_index) {
                    recursive_delete(i);
                }
            }
        }
    } else {
        // File - free data blocks
        int start = inode->start_block;
        int size = inode->isused_size & 0x7F;
        // Only free blocks if file actually has allocated blocks
        if (start > 0 && size > 0) {
            update_free_blocks(start, size, false); // Zero out data blocks
            uint8_t zero_block[1024] = {0};
            for (int i = 0; i < size; i++) {
                write_block(start + i, zero_block);
            }
        }
    }
    memset(inode, 0, sizeof(Inode)); // Zero out the inode
}

int count_children(int dir_inode_index) {
    int count = 0;
    // Loop through all inodes to count children of specified directory
    for (int i = 0; i < 126; i++) {
        if (superblock.inode[i].isused_size & 0x80) {
            int parent = superblock.inode[i].isdir_parent & 0x7F;
            if (parent == dir_inode_index) {
                count++;
            }
        }
    }
    return count + 2; // Add 2 for special entries "." and ".."
}