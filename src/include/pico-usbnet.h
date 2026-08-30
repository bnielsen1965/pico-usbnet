#ifndef PICO_USBNET_H
#define PICO_USBNET_H

#include <stdint.h>
#include <stdbool.h>

#include "lwip/ip4_addr.h"
#include "dhserver.h"
#include "dnserver.h"

#ifdef __cplusplus
extern "C" {
#endif

// Initialize an ip4_addr_t from dotted-quad parts in a struct literal
// (host order), e.g. .ip = USBNET_INIT_IP4(192, 168, 7, 1)
#define USBNET_INIT_IP4(a, b, c, d) { PP_HTONL(LWIP_MAKEU32(a, b, c, d)) }

// Maximum number of DHCP entries accepted in usbnet_config_t
#ifndef USBNET_MAX_DHCP_ENTRIES
#define USBNET_MAX_DHCP_ENTRIES 8
#endif

// How long usbnet_init() waits (ms) for each init step before failing
#ifndef USBNET_INIT_TIMEOUT_MS
#define USBNET_INIT_TIMEOUT_MS 2000
#endif

// DHCP domain advertised to clients when usbnet_config_t.dhcp_domain is NULL
#ifndef USBNET_DHCP_DOMAIN_DEFAULT
#define USBNET_DHCP_DOMAIN_DEFAULT "pico-usbnet"
#endif

// Configuration for the USB Ethernet gadget
typedef struct {
    ip4_addr_t ip;               // device address, e.g. 192.168.7.1
    ip4_addr_t netmask;          // e.g. 255.255.255.0
    ip4_addr_t gateway;          // 0.0.0.0 = no gateway
    const uint8_t *mac;          // 6-byte MAC address, NULL = derive from board unique ID
    const char *dns_name;        // name to resolve to `ip` (NULL = no DNS server)
    dns_query_proc_t dns_query;  // optional extra resolver (NULL = none)
    bool dhcp_enabled;           // enable the built-in DHCP server
    const dhcp_entry_t *dhcp_entries;  // lease table used when dhcp_enabled
    uint8_t dhcp_num_entries;    // number of entries in dhcp_entries
    const char *dhcp_domain;     // domain advertised in DHCP (NULL = USBNET_DHCP_DOMAIN_DEFAULT)
} usbnet_config_t;

// Initialize TinyUSB, lwIP, the netif and (optionally) the DHCP and DNS
// servers. The MAC is taken from cfg->mac or, if NULL, derived from the
// board unique ID (locally-administered, unicast).
// Must be called from a single context before usbnet_service().
// Returns 0 on success, -1 on failure.
int usbnet_init(const usbnet_config_t *cfg);

// Pump the network stack: TinyUSB task, one in-flight RX frame and lwIP
// timeouts. Call periodically from the same single context that called
// usbnet_init() (superloop or a dedicated FreeRTOS task).
void usbnet_service(void);

// The module's network interface (valid after usbnet_init())
struct netif *usbnet_netif(void);

// True once the netif is up and the USB link is configured (i.e. the
// host has enumerated the device)
bool usbnet_is_up(void);

#ifdef __cplusplus
}
#endif

#endif // PICO_USBNET_H
