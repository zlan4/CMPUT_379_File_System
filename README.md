[![Review Assignment Due Date](https://classroom.github.com/assets/deadline-readme-button-22041afd0340ce965d47ae6ef1cefeee28c7c493a6346c4f15d667ab976d596c.svg)](https://classroom.github.com/a/zegDotdy)
- - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Name : Emily Lan
SID : 1757014
CCID : zlan4
- - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Design Choices
- - - - - - - - - - - - - - - - - - - - - - - - - - - - -
I created different C files to separate responsibilities into well-defined components, each with a central purpose. command-processor.c is responsible for reading input files containing file system commands and identifying errors and the line causing the issue. command-processor.c then calls the functions found in fs-sim.c to handle these commands. The functions in fs-sim.c facilitate commands such as mounting the file system, creating a new file or directory, reading a file into the buffer, and more. This file also handles the six consistency checks specified in the assignment, as well as appropriate error handling for each function that processes a file system command. disk-ops.c is a file containing helper functions that carry out disk operations such as opening the disk, finding contiguous regions of free blocks, reading from a block, and more. These functions are called by files such as fs-sim.c. For example, in the fs_create function, find_contiguous_blocks from disk-ops.c is called because files must be allocated a number of contiguous blocks of memory. Similarly, inode-ops.c also contains helper functions. This file deals with inode-related operations such as counting the number of files in a particular directory, determining whether a file name already exists in a specified directory, implementing recursive file and directory deletion, and more. fs-sim.c also uses the helper functions in this file. For example, the is_name_unique_in_directory function is used to appropriately handle scenarios where a file being created already exists in the directory and write an appropriate error message. Finally, main.c runs the process_command_file function in command-processor.c to start reading commands from an input file and run the file system program. This approach in division of responsibility improves modularity and enhances code organization, which in turn makes testing and maintenance much easier.
- - - - - - - - - - - - - - - - - - - - - - - - - - - - -
System Calls/Library Functions Used
- - - - - - - - - - - - - - - - - - - - - - - - - - - - -
1. fopen()
2. memset()
3. strcmp()
4. fprintf()
5. strncpy()
6. strlen()
7. fclose()
8. close()
9. open()
10. lseek()
11. read()
12. write()
13. memcpy()
14. memcmp()
- - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Testing Implementation
- - - - - - - - - - - - - - - - - - - - - - - - - - - - -
I tested my code by running the test script provided with python3 test.py. I also ran valgrind --tool=memcheck --leak-check=yes ./fs <input_file> manually for each of the test input files provided to ensure that my program was free of memory leaks.