#include "src/include/routing-table.h"

routing_table *rtable_alloc(uint8_t size, bool allow_resize)
{
    routing_table *table = malloc(sizeof(routing_table));
    if (table == NULL)
        return NULL;
    table->size = size;
    table->_used = 0;
    table->allow_resize = allow_resize;
    table->entries = malloc(sizeof(routing_entry) * size);
    return table;
}

int rtable_get(routing_table *table, linkaddr_t *child, routing_entry *entry)
{
    uint8_t i = 0;
    for (i = 0; i < table->size; i++)
    {
        routing_entry current = (table->entries)[i];
        if (linkaddr_cmp(child, &(current.child)) != 0)
        {
            entry->child = current.child;
            entry->parent = current.parent;
            return i;
        }
    }
    return -1;
}

bool rtable_update(routing_table *table, routing_entry *entry)
{
    routing_entry e;
    int index = rtable_get(table, &entry->child, &e);
    if (index < 0)
        return false;
    table->entries[index] = *entry;
    return true;
}

bool rtable_add(routing_table *table, routing_entry *entry)
{
    routing_entry dummy;
    if (rtable_get(table, &entry->child, &dummy) >= 0)
        return false;
    if (table->_used > table->size - 1)
    {
        if (!table->allow_resize)
        {
            return false;
        }
        else
        {
            uint16_t newSize = table->size * 2;
            void *dest = malloc(sizeof(routing_entry) * newSize);
            table->entries = memcpy(dest, table->entries, sizeof(routing_entry) * table->size);
            table->size = newSize;
        }
    }
    table->entries[table->_used] = *entry;
    table->_used++;
    return true;
}

void rtable_free(routing_table *table)
{
    free(table->entries);
    free(table);
}