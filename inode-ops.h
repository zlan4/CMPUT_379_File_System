#ifndef INODE_OPS_H
#define INODE_OPS_H
#include "fs-sim.h"

int find_free_inode(void);
bool is_name_unique_in_directory(int parent_inode, char name[5]);
Inode* find_inode_by_name(char name[5], int parent_inode);
void recursive_delete(int inode_index);
int count_children(int dir_inode_index);

#endif