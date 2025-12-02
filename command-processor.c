#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include "fs-sim.h"
#include "disk-ops.h"

// This function reads commands from a file and executes corresponding file system operations
void process_command_file(const char* filename) {
    FILE *input = fopen(filename, "r"); // Open the command file for reading
    if (!input) {
        return; // Silently return if file can't be opened
    }
    char line[1100]; // Buffer size
    int line_num = 0;
    memset(buffer, 0, 1024); // Clear the buffer
    // Process each line in the command file
    while (fgets(line, sizeof(line), input)) {
        line_num++; // Track line number for error reporting
        line[strcspn(line, "\n")] = 0; // Remove trailing newline character
        char command[10]; // Parse command
        char arg1[10] = "", arg2[10] = ""; // Parse arguments
        int args = sscanf(line, "%s %s %s", command, arg1, arg2);
        if (args < 1) {
            continue; // Skip empty lines
        }
        // Mount the file system found on disk
        if (strcmp(command, "M") == 0) {
            // M needs exactly 2 arguments
            if (args != 2) {
                fprintf(stderr, "Command Error: %s, %d\n", filename, line_num);
                continue;
            }
            fs_mount(arg1);
        // Create a file
        } else if (strcmp(command, "C") == 0) {
            // C needs exactly 3 arguments
            if (args != 3) {
                fprintf(stderr, "Command Error: %s, %d\n", filename, line_num);
                continue;
            }
            int size = atoi(arg2);
            // Validate file size (0-127 blocks) and name length (max 5 characters)
            if (size < 0 || size > 127 || strlen(arg1) > 5) {
                fprintf(stderr, "Command Error: %s, %d\n", filename, line_num);
                continue;
            }
            char name[5] = {0};
            strncpy(name, arg1, 5);
            fs_create(name, size);
        // Delete a file
        } else if (strcmp(command, "D") == 0) {
            // D needs exactly 2 arguments
            if (args != 2) {
                fprintf(stderr, "Command Error: %s, %d\n", filename, line_num);
                continue;
            }
            if (strlen(arg1) > 5) {
                fprintf(stderr, "Command Error: %s, %d\n", filename, line_num);
                continue;
            }
            char name[5] = {0};
            strncpy(name, arg1, 5);
            fs_delete(name);
        // Read from a file
        } else if (strcmp(command, "R") == 0) {
            // R needs exactly 3 arguments
            if (args != 3) {
                fprintf(stderr, "Command Error: %s, %d\n", filename, line_num);
                continue;
            }
            int block_num = atoi(arg2);
            // Validate block number (0-126) and name length
            if (block_num < 0 || block_num > 126 || strlen(arg1) > 5) {
                fprintf(stderr, "Command Error: %s, %d\n", filename, line_num);
                continue;
            }
            char name[5] = {0};
            strncpy(name, arg1, 5);
            fs_read(name, block_num);
        // Write to a file 
        } else if (strcmp(command, "W") == 0) {
            // W needs exactly 3 arguments
            if (args != 3) {
                fprintf(stderr, "Command Error: %s, %d\n", filename, line_num);
                continue;
            }
            int block_num = atoi(arg2);
            // Validate block number (0-126) and name length
            if (block_num < 0 || block_num > 126 || strlen(arg1) > 5) {
                fprintf(stderr, "Command Error: %s, %d\n", filename, line_num);
                continue;
            }
            char name[5] = {0};
            strncpy(name, arg1, 5);
            fs_write(name, block_num);
        // Update the buffer
        } else if (strcmp(command, "B") == 0) {
            char *first_space = strchr(line, ' '); // Special handling for buffer command as it can contain spaces
            if (!first_space) {
                fprintf(stderr, "Command Error: %s, %d\n", filename, line_num);
                continue;
            }
            char *content_start = first_space; // Find the buffer content after the command
            while (*content_start == ' ') {
                content_start++; // Skip additional spaces
            }
            // Check if there's actually content after skipping spaces
            if (*content_start == '\0') {
                fprintf(stderr, "Command Error: %s, %d\n", filename, line_num);
                continue;
            }
            // Validate buffer content length (max 1024 bytes)
            if (strlen(content_start) > 1024) {
                fprintf(stderr, "Command Error: %s, %d\n", filename, line_num);
                continue;
            }
            uint8_t buffer_content[1024] = {0};
            strncpy((char*)buffer_content, content_start, 1024); // Copy content to buffer
            fs_buff(buffer_content);
        // List files
        } else if (strcmp(command, "L") == 0) {
            char *rest_of_line = line + strlen(command); // Check if there are any additional characters after "L"
            while (*rest_of_line == ' ') {
                rest_of_line++; // Skip spaces
            }
            // If there's anything left after the command
            if (*rest_of_line != '\0') {
                fprintf(stderr, "Command Error: %s, %d\n", filename, line_num);
                continue;
            }
            fs_ls();
        // Defragment the disk
        } else if (strcmp(command, "O") == 0) {
            char *rest_of_line = line + strlen(command); // Check if there are any additional characters after "O"
            while (*rest_of_line == ' ') {
                rest_of_line++; // Skip spaces
            }
            // If there's anything left after the command
            if (*rest_of_line != '\0') {
                fprintf(stderr, "Command Error: %s, %d\n", filename, line_num);
                continue;
            }
            fs_defrag();
        // Change the current working directory
        } else if (strcmp(command, "Y") == 0) {
            // Y needs exactly 2 arguments
            if (args != 2) {
                fprintf(stderr, "Command Error: %s, %d\n", filename, line_num);
                continue;
            }
            if (strlen(arg1) > 5) {
                fprintf(stderr, "Command Error: %s, %d\n", filename, line_num);
                continue;
            }
            char name[5] = {0};
            strncpy(name, arg1, 5);
            fs_cd(name);
        // Unknown command
        } else {
            fprintf(stderr, "Command Error: %s, %d\n", filename, line_num);
        }
    }
    fclose(input); // Cleanup
    close_disk(); // Close the disk after processing all commands
}