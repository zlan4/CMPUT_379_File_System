#ifndef DISK_OPS_H
#define DISK_OPS_H
#include <stdint.h>
#include <stdbool.h>

int open_disk(const char *filename);
void close_disk(void);
void read_block(int block_num, uint8_t *data);
void write_block(int block_num, uint8_t *data);
void update_free_blocks(int start, int size, bool allocated);
int find_contiguous_blocks(int size);

#endif