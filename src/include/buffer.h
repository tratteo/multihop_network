#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define BUF_READ 0
#define BUF_WRITE 1

typedef struct buffer
{
    void *pointer;
    size_t size;
    uint8_t mode;
    size_t offset;
} buffer;

// Allocate a buffer for write operations
buffer *buffer_allocate_write(size_t size);

// Allocate a buffer for read operations, starting from the provided pointer
buffer *buffer_allocate_read(void *ptr);

// Free the buffer
void buffer_free(buffer *buf);

// Write a value into the buffer
void buffer_write(buffer *buf, void *data, size_t size);

// Read a value from the buffer, returns the pointer to the read value
void *buffer_read(buffer *buf, size_t size);