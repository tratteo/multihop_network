// enable many-to-one traffic
#define APP_UPWARD_TRAFFIC 1
// enable one-to-many traffic
#define APP_DOWNWARD_TRAFFIC 1

#define COLLECT_CHANNEL 0xAA
// RSSI threshold, under which a connection is discarded
#define RSSI_THRESHOLD -95

#define MSG_INIT_DELAY (INIT_BEACON_DELAY + TOPOLOGY_UPDATE_DELAY + (5 * FORWARD_DELAY))
// period of the many-to-one messages
#define MSG_PERIOD (30 * CLOCK_SECOND)
// period of the one-to-many messages
#define SR_MSG_PERIOD (15 * CLOCK_SECOND)
// initial beacon delay
#define INIT_BEACON_DELAY (5 * CLOCK_SECOND)

// how much to wait after receiving a beacon and changing topology before sending a dedicated topology update
// reducing the delay, increases responsitivity to topology updates but increases traffic as less piggybacked messages are used
#define TOPOLOGY_UPDATE_DELAY (BEACON_PERIOD / 6)
// period of the topology reconstruction protocol
#define BEACON_PERIOD (CLOCK_SECOND * 30)
// random delay for forwarding a message
#define FORWARD_DELAY (random_rand() % (CLOCK_SECOND))
