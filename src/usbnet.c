#include <string.h>

#include "pico/stdlib.h"
#include "pico/unique_id.h"

#include "tusb.h"
#include "dhserver.h"
#include "dnserver.h"
#include "lwip/init.h"
#include "lwip/netif.h"
#include "lwip/pbuf.h"
#include "lwip/timeouts.h"
#include "lwip/etharp.h"
#if LWIP_IPV6
#include "lwip/ethip6.h"
#endif

#include "pico-usbnet.h"

// lwIP network interface
static struct netif g_netif;

// Shared between tud_network_recv_cb() and service_traffic(): a single
// in-flight RX pbuf (renewed after each frame)
static struct pbuf *g_received_frame;

// Module state
static bool g_initialized = false;
static ip4_addr_t g_ipaddr;
static ip4_addr_t g_netmask;
static ip4_addr_t g_gateway;
static const char *g_dns_name;
static const char *g_domain;
static dns_query_proc_t g_dns_query;

// Module-owned copy of the DHCP lease table (the config's array only needs
// to be valid during usbnet_init())
static dhcp_entry_t g_dhcp_entries[USBNET_MAX_DHCP_ENTRIES];
static dhcp_config_t g_dhcp_config;

// lwIP netif driver: send packet over USB
static err_t linkoutput_fn(struct netif *netif, struct pbuf *p) {
    (void) netif;

    for (;;) {
        if (!tud_ready())
            return ERR_USE;

        if (tud_network_can_xmit(p->tot_len)) {
            tud_network_xmit(p, 0);
            return ERR_OK;
        }

        tud_task();
    }
}

static err_t ip4_output_fn(struct netif *netif, struct pbuf *p, const ip4_addr_t *addr) {
    return etharp_output(netif, p, addr);
}

#if LWIP_IPV6
static err_t ip6_output_fn(struct netif *netif, struct pbuf *p, const ip6_addr_t *addr) {
    return ethip6_output(netif, p, addr);
}
#endif

static err_t netif_init_cb(struct netif *netif) {
    LWIP_ASSERT("netif != NULL", (netif != NULL));
    netif->mtu = CFG_TUD_NET_MTU;
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP | NETIF_FLAG_UP;
    netif->state = NULL;
    netif->name[0] = 'E';
    netif->name[1] = 'X';
    netif->linkoutput = linkoutput_fn;
    netif->output = ip4_output_fn;
#if LWIP_IPV6
    netif->output_ip6 = ip6_output_fn;
#endif
    return ERR_OK;
}

static void init_lwip(void) {
    struct netif *netif = &g_netif;

    lwip_init();

    netif->hwaddr_len = sizeof(tud_network_mac_address);
    memcpy(netif->hwaddr, tud_network_mac_address, sizeof(tud_network_mac_address));
    // The netif MAC must match the MAC advertised in the USB NCM descriptor;
    // do not flip any bits here.

    netif_add(netif, &g_ipaddr, &g_netmask, &g_gateway, NULL, netif_init_cb, ip_input);
#if LWIP_IPV6
    netif_create_ip6_linklocal_address(netif, 1);
#endif
    netif_set_default(netif);
}

// True if name equals dns_name or dns_name.domain
static bool dns_name_matches(const char *name, const char *dns_name, const char *domain) {
    if (strcmp(name, dns_name) == 0) return true;

    size_t nlen = strlen(dns_name);
    size_t dlen = strlen(domain);
    if (strlen(name) != nlen + 1 + dlen) return false;

    return strncmp(name, dns_name, nlen) == 0 &&
           name[nlen] == '.' &&
           strcmp(name + nlen + 1, domain) == 0;
}

// DNS query handler: resolves the configured dns_name, then defers to the
// consumer-provided callback if any
static bool dns_query_proc(const char *name, ip4_addr_t *addr) {
    if (g_dns_name && dns_name_matches(name, g_dns_name, g_domain)) {
        *addr = g_ipaddr;
        return true;
    }

    if (g_dns_query) return g_dns_query(name, addr);
    return false;
}

// TinyUSB network callbacks
bool tud_network_recv_cb(const uint8_t *src, uint16_t size) {
    if (g_received_frame) return false;

    if (size) {
        struct pbuf *p = pbuf_alloc(PBUF_RAW, size, PBUF_POOL);
        if (p) {
            memcpy(p->payload, src, size);
            g_received_frame = p;
        }
    }

    return true;
}

uint16_t tud_network_xmit_cb(uint8_t *dst, void *ref, uint16_t arg) {
    struct pbuf *p = (struct pbuf *) ref;
    (void) arg;
    return pbuf_copy_partial(p, dst, p->tot_len, 0);
}

// Free a pending RX frame on USB re-enumeration
void tud_network_init_cb(void) {
    if (g_received_frame) {
        pbuf_free(g_received_frame);
        g_received_frame = NULL;
    }
}

// Feed the in-flight RX frame to lwIP and run the timeout handlers
static void service_traffic(void) {
    if (g_received_frame) {
        if (ethernet_input(g_received_frame, &g_netif) != ERR_OK) {
            pbuf_free(g_received_frame);
        }
        g_received_frame = NULL;
        tud_network_recv_renew();
    }

    sys_check_timeouts();
}

int usbnet_init(const usbnet_config_t *cfg) {
    if (!cfg) return -1;
    if (cfg->dhcp_enabled && (!cfg->dhcp_entries || cfg->dhcp_num_entries == 0)) return -1;
    if (cfg->dhcp_num_entries > USBNET_MAX_DHCP_ENTRIES) return -1;

    // MAC address: as configured, or derived from the board unique ID
    // (locally-administered, unicast)
    if (cfg->mac) {
        memcpy(tud_network_mac_address, cfg->mac, sizeof(tud_network_mac_address));
    } else {
        pico_unique_board_id_t uid;
        pico_get_unique_board_id(&uid);
        tud_network_mac_address[0] = (uid.id[0] & 0xFE) | 0x02;
        for (int i = 1; i < 6; i++) {
            tud_network_mac_address[i] = uid.id[i];
        }
    }

    g_ipaddr = cfg->ip;
    g_netmask = cfg->netmask;
    g_gateway = cfg->gateway;
    g_dns_name = cfg->dns_name;
    g_domain = cfg->dhcp_domain ? cfg->dhcp_domain : USBNET_DHCP_DOMAIN_DEFAULT;
    g_dns_query = cfg->dns_query;

    for (uint8_t i = 0; i < cfg->dhcp_num_entries; i++) {
        g_dhcp_entries[i] = cfg->dhcp_entries[i];
    }
    g_dhcp_config.router = g_gateway;
    g_dhcp_config.port = 67;
    g_dhcp_config.dns = g_ipaddr;
    g_dhcp_config.domain = g_domain;
    g_dhcp_config.num_entry = cfg->dhcp_num_entries;
    g_dhcp_config.entries = g_dhcp_entries;

    // Initialize TinyUSB
    tusb_rhport_init_t dev_init = {
        .role = TUSB_ROLE_DEVICE,
        .speed = TUSB_SPEED_AUTO
    };
    if (!tusb_init(BOARD_TUD_RHPORT, &dev_init)) return -1;

    // Initialize lwIP + netif
    init_lwip();

    absolute_time_t timeout = make_timeout_time_ms(USBNET_INIT_TIMEOUT_MS);
    while (!netif_is_up(&g_netif)) {
        if (time_reached(timeout)) return -1;
        tud_task();
    }

    // DHCP server
    if (cfg->dhcp_enabled) {
        timeout = make_timeout_time_ms(USBNET_INIT_TIMEOUT_MS);
        while (dhserv_init(&g_dhcp_config) != ERR_OK) {
            if (time_reached(timeout)) return -1;
            tud_task();
        }
    }

    // DNS server
    if (cfg->dns_name || cfg->dns_query) {
        timeout = make_timeout_time_ms(USBNET_INIT_TIMEOUT_MS);
        while (dnserv_init(IP_ADDR_ANY, 53, dns_query_proc) != ERR_OK) {
            if (time_reached(timeout)) return -1;
            tud_task();
        }
    }

    g_initialized = true;
    return 0;
}

void usbnet_service(void) {
    if (!g_initialized) return;

    tud_task();
    service_traffic();
}

struct netif *usbnet_netif(void) {
    return &g_netif;
}

bool usbnet_is_up(void) {
    return g_initialized && netif_is_up(&g_netif) && tud_ready();
}
