#ifndef __MY_COLLECT_H__
#define __MY_COLLECT_H__
#include <stdbool.h>
#include "contiki.h"
#include "net/rime/rime.h"
#include "net/netstack.h"
#include "core/net/linkaddr.h"
#include "routing-table.h"
#include "buffer.h"
#include "params.h"
#include "packet.h"

// Connection object
struct protocol_conn
{
  // number of nodes in the network
  uint16_t nodes;
  // sink only - routing table
  routing_table *routing_table;
  // timer used to manage topology updates
  struct ctimer topology_timer;
  // whether the topology has been refreshed at the root during the current topology epoch
  bool topology_refreshed;
  // whether the topology is dirty and must be refreshed
  bool topology_dirty;
  // broadcast rime connection structure
  struct broadcast_conn bc;
  // unicast rime connection structure
  struct unicast_conn uc;
  // application callbacks structure
  const struct protocol_callbacks *callbacks;
  // node only - parent of the current node
  linkaddr_t parent;
  // clock timer used to send the beacon
  struct ctimer beacon_timer;
  // current topology hop_to_sink
  uint16_t hop_to_sink;
  // link quality to the current parent
  int16_t parent_rssi;
  // current topology beacon seqn
  uint16_t beacon_seqn;
  // whether the node is the sink
  bool is_sink;
};

// Callback structure
struct protocol_callbacks
{
  // sink received data packet callback
  void (*recv)(const linkaddr_t *originator, uint8_t hops);
  // node received data packet callback
  void (*sr_recv)(struct protocol_conn *c, uint8_t hops);
};

// Initialize the protocol
void open_protocol(
    struct protocol_conn *conn,
    uint16_t channels,
    bool is_sink,
    const struct protocol_callbacks *callbacks,
    uint16_t nodes);

// Close the protocol and free space
void close_protocol(struct protocol_conn *conn);

// Send a packet to the sink, using the parent data of nodes
int send_sink(struct protocol_conn *c);

/// Send packet to a specific node, only if sink
int send_node(struct protocol_conn *c, linkaddr_t *dest);

#endif /* __MY_COLLECT_H__ */
