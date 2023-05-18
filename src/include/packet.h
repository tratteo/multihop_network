#include <stdlib.h>
#include "contiki.h"
#include "net/rime/rime.h"
#include "net/netstack.h"
#include <string.h>
#define SOURCE_ROUTE_PACKET 0
#define DATA_PACKET 1

// Allocate the required space and write the data in the header
void _write_packet_header(uint8_t packet_id, void *data_ptr, size_t size);
// Read a packet id from the header and reduce the header
void _read_packet_id(uint8_t *id);