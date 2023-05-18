#include "src/include/packet.h"

void _write_packet_header(uint8_t packet_id, void *data_ptr, size_t size)
{
    size_t res = packetbuf_hdralloc(sizeof(uint8_t) + size);
    if (res == 0)
        return;
    memcpy(packetbuf_hdrptr(), &packet_id, sizeof(uint8_t));
    if (size > 0)
    {
        memcpy(packetbuf_hdrptr() + sizeof(uint8_t), data_ptr, size);
    }
}

void _read_packet_id(uint8_t *id)
{
    if (packetbuf_datalen() < sizeof(uint8_t))
    {
        return;
    }
    memcpy(id, packetbuf_dataptr(), sizeof(uint8_t));
    packetbuf_hdrreduce(sizeof(uint8_t));
}