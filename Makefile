CC = gcc
CFLAGS = -Wall -Werror

TARGET = fs

SRCS = command-processor.c disk-ops.c fs-sim.c inode-ops.c main.c
OBJS = command-processor.o disk-ops.o fs-sim.o inode-ops.o main.o
HEADERS = command-processor.h disk-ops.h fs-sim.h inode-ops.h

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

%.o: %.c $(HEADERS)
	$(CC) -c $(CFLAGS) $< -o $@

compile: $(OBJS)

clean:
	rm -f $(OBJS) $(TARGET)
	@echo Cleaned