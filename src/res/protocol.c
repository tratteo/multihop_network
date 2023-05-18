#include <stdbool.h>
#include "contiki.h"
#include "lib/random.h"
#include "net/rime/rime.h"
#include "leds.h"
#include "net/netstack.h"
#include <stdio.h>
#include "core/net/linkaddr.h"
#include "protocol.h"

#define LOG_ENABLED 0

// Unicast recv callback
void _unicast_recv(struct unicast_conn *c, const linkaddr_t *from);
// Broadcast recv callback
void _broadcast_recv(struct broadcast_conn *conn, const linkaddr_t *sender);
// Callback when the beacon timer expires, only on sink
void _beacon_timer_cb(void *ptr);
// callback when the topology dedicated update expires
void _topology_timer_cb(void *ptr);
// Handle packets based on the id
void _handle_packet(uint8_t packet_id, struct protocol_conn *conn);
// Build the route towards the specified destination from the sink.
// Returns the length of the route and populates in [path] an array of hops.
// The path returned includes the first hop to do from the sink, I.E:
// Path 1 > 4 > 6 > 8 --> [00:04, 00:06, 00:08].
// Therefore when copying the path to the header, can skip the first entry.
// Remember to free the [path] populated
uint8_t _build_route(routing_table *routing_table, linkaddr_t *dest, linkaddr_t **path);

// Rime Callback structures
struct broadcast_callbacks bc_cb = {
	.recv = _broadcast_recv,
	.sent = NULL};
struct unicast_callbacks uc_cb = {
	.recv = _unicast_recv,
	.sent = NULL};

void open_protocol(struct protocol_conn *conn, uint16_t channels,
				   bool is_sink, const struct protocol_callbacks *callbacks, uint16_t nodes)
{
	linkaddr_copy(&conn->parent, &linkaddr_null);
	conn->hop_to_sink = is_sink ? 0 : UINT16_MAX;
	conn->parent_rssi = INT16_MIN;
	conn->beacon_seqn = 0;
	conn->is_sink = is_sink;
	conn->callbacks = callbacks;
	conn->nodes = nodes;
	conn->topology_dirty = false;
	conn->topology_refreshed = false;

	// Open the underlying Rime primitives
	broadcast_open(&conn->bc, channels, &bc_cb);
	unicast_open(&conn->uc, channels + 1, &uc_cb);
	if (is_sink)
	{
		// Allocate a new routing table structure
		conn->routing_table = rtable_alloc(nodes, true);
		conn->beacon_seqn = 1;
		// Send first beacon after some time
		ctimer_set(&conn->beacon_timer, INIT_BEACON_DELAY, _beacon_timer_cb, conn);
	}
}

void close_protocol(struct protocol_conn *conn)
{
	rtable_free(conn->routing_table);
}

#pragma region TopologyBeacon
struct beacon_msg
{
	uint16_t seqn;
	uint16_t hop_to_sink;
} __attribute__((packed));

void _send_beacon(struct protocol_conn *conn)
{
	struct beacon_msg beacon = {
		.seqn = conn->beacon_seqn, .hop_to_sink = conn->hop_to_sink};

	// Send the beacon message in broadcast
	packetbuf_clear();
	packetbuf_copyfrom(&beacon, sizeof(beacon));
	broadcast_send(&conn->bc);
}

void _topology_timer_cb(void *ptr)
{
	struct protocol_conn *conn = (struct protocol_conn *)ptr;
	if (conn->topology_dirty && !conn->topology_refreshed)
	{
		// Send dedicated topology update
		printf("Protocol: dedicated topology update\n");
		packetbuf_clear();
		send_sink(conn);
		conn->topology_dirty = false;
		conn->topology_refreshed = false;
	}
}

void _beacon_timer_cb(void *ptr)
{
	struct protocol_conn *conn = (struct protocol_conn *)ptr;
	_send_beacon(conn);
	if (conn->is_sink)
	{
		conn->beacon_seqn += 1;
		ctimer_set(&conn->beacon_timer, BEACON_PERIOD, _beacon_timer_cb, conn);
	}
}

void _broadcast_recv(struct broadcast_conn *bc_conn, const linkaddr_t *sender)
{
	struct beacon_msg beacon;

	/* Get the pointer to the overall structure protocol_conn from its field bc */
	struct protocol_conn *conn = (struct protocol_conn *)(((uint8_t *)bc_conn) -
														  offsetof(struct protocol_conn, bc));
	// No need for the sink to listen for broadcast messages
	if (conn->is_sink)
		return;

	/* Check if the received broadcast packet looks legitimate */
	if (packetbuf_datalen() != sizeof(struct beacon_msg))
	{
		if (LOG_ENABLED)
			printf("Protocol error: broadcast message of wrong size\n");
		return;
	}
	memcpy(&beacon, packetbuf_dataptr(), sizeof(struct beacon_msg));

	int16_t rssi = packetbuf_attr(PACKETBUF_ATTR_RSSI);

	if (LOG_ENABLED)
		printf("Protocol: beacon metrics from %02x:%02x seqn %u hop_to_sink %u rssi %d\n",
			   sender->u8[0], sender->u8[1],
			   beacon.seqn, beacon.hop_to_sink + 1, rssi);
	if (rssi < RSSI_THRESHOLD || beacon.seqn < conn->beacon_seqn)
		return; // The beacon is either too weak or too old, ignore it
	if (beacon.seqn == conn->beacon_seqn)
	{ // The beacon is not new, check the hop_to_sink
		if (beacon.hop_to_sink + 1 > conn->hop_to_sink)
			return; // Worse or equal than what we have, ignore it
		if (rssi <= conn->parent_rssi)
			return;
	}
	if (LOG_ENABLED)
		printf("Protocol: accept beacon from %02x:%02x seqn %u hop_to_sink %u rssi %d\n",
			   sender->u8[0], sender->u8[1],
			   beacon.seqn, beacon.hop_to_sink + 1, rssi);
	linkaddr_t old_parent = conn->parent;
	/* Otherwise, memorize the new parent, the hop_to_sink, and the seqn */
	linkaddr_copy(&conn->parent, sender);
	conn->hop_to_sink = beacon.hop_to_sink + 1;
	conn->parent_rssi = rssi;
	conn->beacon_seqn = beacon.seqn;

	ctimer_set(&conn->beacon_timer, FORWARD_DELAY, _beacon_timer_cb, conn);

	// If the new parent is different from the old, send a dedicated topology update, send the update rigth away, before sending the beacon
	if (linkaddr_cmp(&old_parent, sender) == 0)
	{
		if (LOG_ENABLED)
		{
			printf("Protocol: new parent %02x:%02x, hop_to_sink %d, seqn %d\n", sender->u8[0], sender->u8[1], conn->hop_to_sink, conn->beacon_seqn);
			printf("Protocol topology: setting topology to dirty\n");
		}
		conn->topology_dirty = true;
		ctimer_set(&conn->topology_timer, TOPOLOGY_UPDATE_DELAY + FORWARD_DELAY, _topology_timer_cb, conn);
	}
}
#pragma endregion TopologyBeacon

#pragma region Data

struct piggyback_header
{
	linkaddr_t source;
	linkaddr_t parent;
	uint8_t hops;
} __attribute__((packed));

int send_sink(struct protocol_conn *conn)
{
	if (linkaddr_cmp(&conn->parent, &linkaddr_null) != 0)
	{
		if (LOG_ENABLED)
			printf("Protocol: error sending to sink, no parent info\n");
		return -1;
	}

	struct piggyback_header hdr = {.source = linkaddr_node_addr, .parent = conn->parent, .hops = 0};
	// Piggyback topology information
	if (conn->topology_dirty && !conn->topology_refreshed)
	{
		if (packetbuf_datalen() > 0)
			printf("Protocol: piggyback topology update\n");
		conn->topology_refreshed = true;
		conn->topology_dirty = false;
	}
	_write_packet_header(DATA_PACKET, &hdr, sizeof(hdr));
	if (LOG_ENABLED)
		printf("Protocol: send to sink, first hop %02x:%02x\n", conn->parent.u8[0], conn->parent.u8[1]);
	return unicast_send(&conn->uc, &conn->parent);
}

void _unicast_recv(struct unicast_conn *uc_conn, const linkaddr_t *from)
{
	/* Get the pointer to the overall structure protocol_conn from its field uc */
	struct protocol_conn *conn = (struct protocol_conn *)(((uint8_t *)uc_conn) -
														  offsetof(struct protocol_conn, uc));

	uint8_t packet_id;
	_read_packet_id(&packet_id);
	_handle_packet(packet_id, conn);
}

int send_node(struct protocol_conn *c, linkaddr_t *dest)
{
	if (!c->is_sink)
		return -1;

	linkaddr_t *init_path = NULL;

	if (LOG_ENABLED)
	{
		size_t j = 0;
		printf("Protocol: rtable ");
		for (j = 0; j < c->routing_table->size; j++)
		{
			routing_entry entry = c->routing_table->entries[j];
			printf("(%02x:%02x)-(%02x:%02x) |", entry.child.u8[0], entry.child.u8[1], entry.parent.u8[0], entry.parent.u8[1]);
		}
		printf("\n");
	}

	uint8_t length = _build_route(c->routing_table, dest, &init_path);
	// Check whether there are routing information to reach the desired node
	if (init_path == NULL || length <= 0)
	{
		if (LOG_ENABLED)
			printf("Protocol error: no routing information, %p, %d\n", init_path, length);
		return -1;
	}

	// Save the next hop
	linkaddr_t first_hop = init_path[0];
	// Get the path after the first hop
	linkaddr_t *path = &init_path[1];
	length = length - 1;

	// Write the length and the hops in the header
	buffer *w_buf = buffer_allocate_write(sizeof(uint8_t) + sizeof(uint8_t) + (sizeof(linkaddr_t) * length));
	buffer_write(w_buf, &length, sizeof(uint8_t));
	uint8_t hops = 0;
	buffer_write(w_buf, &hops, sizeof(uint8_t));

	if (LOG_ENABLED)
		printf("Protocol: sink toward %02x:%02x, route_length %d > ", dest->u8[0], dest->u8[1], length);
	// Write all the path route in the header
	uint8_t i;
	for (i = 0; i < length; i++)
	{
		linkaddr_t current = path[i];
		buffer_write(w_buf, &current, sizeof(linkaddr_t));
		if (LOG_ENABLED)
			printf("%02x:%02x ", current.u8[0], current.u8[1]);
	}
	if (LOG_ENABLED)
		printf("\n");

	// Write the header
	_write_packet_header(SOURCE_ROUTE_PACKET, w_buf->pointer, w_buf->size);
	int res = unicast_send(&c->uc, &first_hop);
	// Free resources
	buffer_free(w_buf);
	free(init_path);
	return res;
}

uint8_t _build_route(routing_table *routing_table, linkaddr_t *dest, linkaddr_t **path)
{
	routing_entry entry;
	linkaddr_t current = *dest;
	// Initialize a list of the maximum route size and set the first element as the destination
	linkaddr_t lookup_list[routing_table->size];
	lookup_list[0] = *dest;

	uint8_t path_length = 1;
	do
	{
		int res = rtable_get(routing_table, &current, &entry);
		// Entry not found in the table, return a NULL path (drop the packet)
		if (res < 0)
		{
			*path = NULL;
			return 0;
		}

		current = entry.parent;
		// If we reached the sink, break the loop
		if (linkaddr_cmp(&current, &linkaddr_node_addr) != 0)
			break;

		lookup_list[path_length++] = current;
	} while (path_length < routing_table->size);

	// Loop detected, return a NULL path (drop the packet)
	if (path_length >= routing_table->size)
	{
		if (LOG_ENABLED)
			printf("Protocol: error loop detected\n");
		*path = NULL;
		return 0;
	}

	// Build the actual path as the reversed of the lookup list
	*path = malloc(path_length * sizeof(linkaddr_t));
	uint8_t i = 0;
	for (i = 0; i < path_length; i++)
	{
		current = lookup_list[path_length - i - 1];
		// Safety check
		if (linkaddr_cmp(&current, &linkaddr_null) != 0)
			continue;

		(*path)[i] = current;
	}
	return path_length;
}

void _handle_packet(uint8_t packet_id, struct protocol_conn *conn)
{
	switch (packet_id)
	{
	case DATA_PACKET:
	{
		if (packetbuf_datalen() < sizeof(struct piggyback_header))
		{
			if (LOG_ENABLED)
				printf("Protocol error: short data packet header %d\n", packetbuf_datalen());
			return;
		}
		struct piggyback_header hdr;
		memcpy(&hdr, packetbuf_dataptr(), sizeof(hdr));
		hdr.hops++;
		packetbuf_hdrreduce(sizeof(hdr));
		if (conn->is_sink)
		{
			routing_entry entry = {.child = hdr.source, .parent = hdr.parent};
			// Get the current routing information of the child
			int index = rtable_get(conn->routing_table, &hdr.source, &entry);
			if (LOG_ENABLED)
				printf("Protocol: routing get: (%02x:%02x > %02x:%02x) present %d\n", entry.child.u8[0], entry.child.u8[1], entry.parent.u8[0], entry.parent.u8[1], index);
			if (index < 0)
			{
				// No routing info found, add new one
				rtable_add(conn->routing_table, &entry);
				if (LOG_ENABLED)
					printf("Protocol: routing add: (%02x:%02x > %02x:%02x)\n", entry.child.u8[0], entry.child.u8[1], entry.parent.u8[0], entry.parent.u8[1]);
			}
			else if (linkaddr_cmp(&entry.parent, &hdr.parent) == 0)
			{
				// Parent is changed, update the routing table
				entry.parent = hdr.parent;
				rtable_update(conn->routing_table, &entry);
				if (LOG_ENABLED)
					printf("Protocol: routing update: (%02x:%02x > %02x:%02x)\n", entry.child.u8[0], entry.child.u8[1], entry.parent.u8[0], entry.parent.u8[1]);
			}
			// Deliver the message to the app if was a message and not simple a topology dedicated update
			if (packetbuf_datalen() > 0)
			{
				conn->callbacks->recv(&hdr.source, hdr.hops);
			}
		}
		else
		{
			if (LOG_ENABLED)
				printf("Protocol: forwarding packet towards %02x:%02x\n", conn->parent.u8[0], conn->parent.u8[1]);

			_write_packet_header(packet_id, &hdr, sizeof(hdr));
			unicast_send(&conn->uc, &conn->parent);
		}

		break;
	}

	case SOURCE_ROUTE_PACKET:
	{
		if (packetbuf_datalen() < sizeof(uint8_t) + sizeof(uint8_t))
		{
			if (LOG_ENABLED)
				printf("Protocol error: short source packet header %d\n", packetbuf_datalen());
			return;
		}
		// Allocate the buffer to read the header sequentially
		buffer *r_buf = buffer_allocate_read(packetbuf_dataptr());
		uint8_t length = *((uint8_t *)buffer_read(r_buf, sizeof(uint8_t)));
		uint8_t hops = *((uint8_t *)buffer_read(r_buf, sizeof(uint8_t)));

		// Check whether the header has the correct data (routing info)
		if (packetbuf_datalen() - r_buf->offset < length * sizeof(linkaddr_t))
		{
			if (LOG_ENABLED)
				printf("Protocol error: short source packet header, missing route info %d\n", packetbuf_datalen());
			return;
		}
		hops++;
		// No more hopsto do, we are the destination, deliver the packet to the app
		if (length <= 0)
		{
			packetbuf_hdrreduce(r_buf->offset);
			conn->callbacks->sr_recv(conn, hops);
		}
		else
		{
			// Read the next hop
			linkaddr_t next_hop = *((linkaddr_t *)buffer_read(r_buf, sizeof(linkaddr_t)));
			// Decrement the route length
			length--;

			// ! DEBUG =============
			// uint8_t t_offset = r_buf->offset;
			// uint8_t i = 0;
			// printf("Protocol: debug next hop %02x:%02x | route %d: ", next_hop.u8[0], next_hop.u8[1], length);
			// for (i = 0; i < length; i++)
			// {
			// 	linkaddr_t c;
			// 	memcpy(&c, packetbuf_dataptr() + t_offset, sizeof(linkaddr_t));
			// 	t_offset += sizeof(linkaddr_t);
			// 	printf("%02x:%02x > ", c.u8[0], c.u8[1]);
			// }
			// printf("\n");
			// // ! DEBUG =============

			// Allocate a new buffer to write the header sequentially
			buffer *w_buf = buffer_allocate_write(sizeof(uint8_t) + sizeof(uint8_t) + (sizeof(linkaddr_t) * length));
			// Write route length and hops
			buffer_write(w_buf, &length, sizeof(uint8_t));
			buffer_write(w_buf, &hops, sizeof(uint8_t));

			// If there are hops to write, write them all copying them from the dataptr,
			// skipping the first one exploiting the already correct read buffer offset
			if (length > 0)
			{
				buffer_write(w_buf, packetbuf_dataptr() + r_buf->offset, length * sizeof(linkaddr_t));
			}

			// Cleare the header since we are going to allocate a new one with different size
			packetbuf_hdrreduce(w_buf->size + sizeof(linkaddr_t));
			_write_packet_header(SOURCE_ROUTE_PACKET, w_buf->pointer, w_buf->size);
			buffer_free(w_buf);
			if (LOG_ENABLED)
				printf("Protocol: forward to %02x:%02x\n", next_hop.u8[0], next_hop.u8[1]);
			// Send to the net hop
			unicast_send(&conn->uc, &next_hop);
		}
		buffer_free(r_buf);
		break;
	}
	default:
	{
		if (LOG_ENABLED)
			printf("Protocol: uknown packet received %d\n", packet_id);
		break;
	}
	}
}

#pragma endregion Data