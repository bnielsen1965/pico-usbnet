#ifndef __LWIPOPTS_H__
#define __LWIPOPTS_H__

/* No RTOS - use nosys. Keep NO_SYS even under FreeRTOS: the consumer pumps
 * usbnet_service() from exactly one task, which is all lwIP requires. */
#ifndef NO_SYS
#define NO_SYS                          1
#endif
#ifndef MEM_ALIGNMENT
#define MEM_ALIGNMENT                   4
#endif

/* Core protocols */
#ifndef LWIP_RAW
#define LWIP_RAW                        0
#endif
#ifndef LWIP_NETCONN
#define LWIP_NETCONN                    0
#endif
#ifndef LWIP_SOCKET
#define LWIP_SOCKET                     0
#endif
#ifndef LWIP_DHCP
#define LWIP_DHCP                       0
#endif
#ifndef LWIP_ICMP
#define LWIP_ICMP                       1
#endif
#ifndef LWIP_UDP
#define LWIP_UDP                        1
#endif
#ifndef LWIP_TCP
#define LWIP_TCP                        1
#endif
#ifndef LWIP_IPV4
#define LWIP_IPV4                       1
#endif
#ifndef LWIP_IPV6
#define LWIP_IPV6                       0
#endif

/* Ethernet */
#ifndef ETH_PAD_SIZE
#define ETH_PAD_SIZE                    0
#endif
#ifndef LWIP_IP_ACCEPT_UDP_PORT
#define LWIP_IP_ACCEPT_UDP_PORT(p)      ((p) == PP_NTOHS(67))
#endif

/* TCP tuning */
#ifndef TCP_MSS
#define TCP_MSS                         (1500 - 20 - 20)
#endif
#ifndef TCP_SND_BUF
#define TCP_SND_BUF                     (4 * TCP_MSS)
#endif
#ifndef TCP_WND
#define TCP_WND                         (4 * TCP_MSS)
#endif

/* ARP */
#ifndef ETHARP_SUPPORT_STATIC_ENTRIES
#define ETHARP_SUPPORT_STATIC_ENTRIES   1
#endif

/* Single netif (USB Ethernet only) */
#ifndef LWIP_SINGLE_NETIF
#define LWIP_SINGLE_NETIF               1
#endif

/* Memory tuning for small embedded system */
#ifndef PBUF_POOL_SIZE
#define PBUF_POOL_SIZE                  16
#endif
#ifndef MEMP_NUM_TCP_PCB
#define MEMP_NUM_TCP_PCB                8
#endif
#ifndef MEMP_NUM_TCP_SEG
#define MEMP_NUM_TCP_SEG                16
#endif
#ifndef MEMP_NUM_UDP_PCB
#define MEMP_NUM_UDP_PCB                4
#endif
#ifndef MEMP_NUM_RAW_PCB
#define MEMP_NUM_RAW_PCB                0
#endif
#ifndef MEMP_NUM_SYS_TIMEOUT
#define MEMP_NUM_SYS_TIMEOUT            8
#endif

/* Ping support */
#ifndef LWIP_MULTICAST_PING
#define LWIP_MULTICAST_PING             1
#endif
#ifndef LWIP_BROADCAST_PING
#define LWIP_BROADCAST_PING             1
#endif

#endif /* __LWIPOPTS_H__ */
