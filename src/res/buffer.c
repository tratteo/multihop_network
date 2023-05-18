#include "buffer.h"

buffer *buffer_allocate_read(void *ptr)
{
    buffer *b = (buffer *)malloc(sizeof(buffer));
    b->pointer = ptr;
    b->size = 0;
    b->mode = BUF_READ;
    b->offset = 0;
    return b;
}
buffer *buffer_allocate_write(size_t size)
{
    void *ptr = malloc(size);
    buffer *b = (buffer *)malloc(sizeof(buffer));
    b->pointer = ptr;
    b->size = size;
    b->mode = BUF_WRITE;
    b->offset = 0;
    return b;
}
void buffer_free(buffer *buf)
{
    if (buf->mode == BUF_WRITE)
    {
        free(buf->pointer);
    }
    free(buf);
}
void buffer_write(buffer *buf, void *data, size_t size)
{
    if (buf->mode != BUF_WRITE)
        return;
    if (buf->offset + size > buf->size)
        return;
    memcpy(buf->pointer + buf->offset, data, size);
    buf->offset += size;
}

void *buffer_read(buffer *buf, size_t size)
{
    if (buf->mode != BUF_READ)
        return NULL;
    void *ptr = NULL;
    memcpy(ptr, buf->pointer + buf->offset, size);
    buf->offset += size;
    buf->size += size;
    return ptr;
}