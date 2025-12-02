#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "command-processor.h"
#include "disk-ops.h"

int main(int argc, char *argv[]) {
    // Expected format: ./fs-sim <command-file>
    if (argc != 2) {
        return 1;
    }
    process_command_file(argv[1]);
    close_disk(); // Ensure disk is closed when program exits
    return 0;
}