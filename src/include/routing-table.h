#include "core/net/linkaddr.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>

// generic entry of the routing table
typedef struct entry
{
    linkaddr_t child;
    linkaddr_t parent;
} routing_entry;

// structure containing a list of all routing entries
typedef struct table
{
    routing_entry *entries;
    bool allow_resize;
    uint8_t size;
    uint8_t _used;
} routing_table;

/// allocate a new routing table to size elements
routing_table *rtable_alloc(uint8_t size, bool allow_resize);

/// try to retrieve a specific entry, returns the index in the routing table and populate the struct [entry] if found, -1 otherwise
int rtable_get(routing_table *table, linkaddr_t *child, routing_entry *entry);

/// try to add a new entry to the table. Succeeds only if the table has space and the [entry.child] is not already present. Does not update the entry
bool rtable_add(routing_table *table, routing_entry *entry);

/// try to update a new entry in the table. Succeeds only if the [entry.child] is already present
bool rtable_update(routing_table *table, routing_entry *entry);

/// free the routing table allocated space
void rtable_free(routing_table *table);