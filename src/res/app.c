#include "contiki.h"
#include "lib/random.h"
#include "net/rime/rime.h"
#include "leds.h"
#include "net/netstack.h"
#include <stdio.h>
#include "core/net/linkaddr.h"
#include "protocol.h"
#include "simple-energest.h"
#include "params.h"
#ifndef CONTIKI_TARGET_SKY
linkaddr_t sink = {{0xF7, 0x9C}}; /* Firefly (testbed): node 1 will be our sink */
#define APP_NODES 10
linkaddr_t dest_list[] = {
    {{0xF3, 0x84}}, /* Firefly node 3 */
    {{0xD8, 0xB5}}, /* Firefly node 9 */
    {{0xF2, 0x33}}, /* Firefly node 12 */
    {{0xD9, 0x23}}, /* Firefly node 17 */
    {{0xF3, 0xC2}}, /* Firefly node 19 */
    {{0xDE, 0xE4}}, /* Firefly node 21 */
    {{0xF2, 0x64}}, /* Firefly node 27 */
    {{0xF7, 0xE1}}, /* Firefly node 30 */
    {{0xF2, 0xD7}}, /* Firefly node 33 */
    {{0xF3, 0xA3}}  /* Firefly node 34 */
};
#else
linkaddr_t sink = {{0x01, 0x00}}; /* TMote Sky (Cooja): node 1 will be our sink */
#define APP_NODES 9
linkaddr_t dest_list[] = {
    {{0x02, 0x00}},
    {{0x03, 0x00}},
    {{0x04, 0x00}},
    {{0x05, 0x00}},
    {{0x06, 0x00}},
    {{0x07, 0x00}},
    {{0x08, 0x00}},
    {{0x09, 0x00}},
    {{0xA, 0x00}}};
#endif

PROCESS(app_process, "App process");
AUTOSTART_PROCESSES(&app_process);

/* Application packet */
typedef struct
{
  uint16_t seqn;
}
__attribute__((packed))
app_msg;

static struct protocol_conn protocol_conn;

static void sink_recv_cb(const linkaddr_t *originator, uint8_t hops);

static void sr_recv_cb(struct protocol_conn *ptr, uint8_t hops);

static struct protocol_callbacks sink_cb = {
    .recv = sink_recv_cb,
    .sr_recv = NULL,
};
static struct protocol_callbacks node_cb = {
    .recv = NULL,
    .sr_recv = sr_recv_cb,
};

PROCESS_THREAD(app_process, ev, data)
{
  static struct etimer periodic;
  static struct etimer rnd;
  static app_msg msg = {.seqn = 0};
  static uint8_t dest_idx = 0;
  static linkaddr_t dest = {{0x00, 0x00}};
  static int ret;

  PROCESS_BEGIN();

  /* Start energest to estimate node duty cycle */
  simple_energest_start();

  if (linkaddr_cmp(&sink, &linkaddr_node_addr))
  {

    printf("App: I am sink %02x:%02x\n", linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1]);
    open_protocol(&protocol_conn, COLLECT_CHANNEL, true, &sink_cb, APP_NODES);

#if APP_DOWNWARD_TRAFFIC == 1
    /* Wait a bit longer at the beginning to gather enough topology information */
    etimer_set(&periodic, MSG_INIT_DELAY);
    while (1)
    {
      PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic));
      etimer_set(&periodic, SR_MSG_PERIOD);
      etimer_set(&rnd, random_rand() % (SR_MSG_PERIOD / 2));
      PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&rnd));

      /* Set application data packet */
      packetbuf_clear();
      memcpy(packetbuf_dataptr(), &msg, sizeof(msg));
      packetbuf_set_datalen(sizeof(msg));

      /* Change the destination link address to a different node */
      linkaddr_copy(&dest, &dest_list[dest_idx]);

      /* Send the packet downwards */
      printf("App: sink sending seqn %d to %02x:%02x\n",
             msg.seqn, dest.u8[0], dest.u8[1]);
      ret = send_node(&protocol_conn, &dest);

      /* Check that the packet could be sent */
      if (ret == 0)
      {
        printf("App: sink could not send seqn %d to %02x:%02x\n",
               msg.seqn, dest.u8[0], dest.u8[1]);
      }

      /* Update sequence number and next destination address */
      msg.seqn++;
      dest_idx++;
      if (dest_idx >= APP_NODES)
      {
        dest_idx = 0;
      }
    }
#endif /* APP_DOWNWARD_TRAFFIC == 1 */
  }
  else
  {
    printf("App: I am normal node %02x:%02x\n", linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1]);
    open_protocol(&protocol_conn, COLLECT_CHANNEL, false, &node_cb, APP_NODES);

#if APP_UPWARD_TRAFFIC == 1
    etimer_set(&periodic, MSG_INIT_DELAY);
    while (1)
    {
      PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic));
      etimer_set(&periodic, MSG_PERIOD);
      etimer_set(&rnd, random_rand() % (MSG_PERIOD / 2));
      PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&rnd));

      packetbuf_clear();
      memcpy(packetbuf_dataptr(), &msg, sizeof(msg));
      packetbuf_set_datalen(sizeof(msg));
      printf("App: send seqn %d\n", msg.seqn);
      send_sink(&protocol_conn);
      msg.seqn++;
    }
#endif /* APP_UPWARD_TRAFFIC == 1 */
  }
  close_protocol(&protocol_conn);
  PROCESS_END();
}

static void sink_recv_cb(const linkaddr_t *originator, uint8_t hops)
{
  app_msg msg;
  if (packetbuf_datalen() != sizeof(msg))
  {
    printf("App: wrong length: %d\n", packetbuf_datalen());
    return;
  }
  memcpy(&msg, packetbuf_dataptr(), sizeof(msg));
  printf("App: recv from %02x:%02x seqn %u hops %u\n",
         originator->u8[0], originator->u8[1], msg.seqn, hops);
}

static void sr_recv_cb(struct protocol_conn *ptr, uint8_t hops)
{
  app_msg sr_msg;
  if (packetbuf_datalen() != sizeof(app_msg))
  {
    printf("App: sr_recv wrong length: %d\n", packetbuf_datalen());
    return;
  }
  memcpy(&sr_msg, packetbuf_dataptr(), sizeof(app_msg));
  printf("App: sr_recv from sink seqn %u hops %u node metric %u\n",
         sr_msg.seqn, hops, ptr->hop_to_sink);
}
