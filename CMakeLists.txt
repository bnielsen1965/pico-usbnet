# pico-usbnet: USB Ethernet gadget (TinyUSB CDC-NCM + lwIP netif + DHCP/DNS servers)
#
# Consumed via add_subdirectory() from a project that has already called
# pico_sdk_init() (see example/ for a standalone build).

if (NOT PICO_TINYUSB_PATH)
    set(PICO_TINYUSB_PATH ${PICO_SDK_PATH}/lib/tinyusb)
endif()
if (NOT PICO_LWIP_PATH)
    set(PICO_LWIP_PATH ${PICO_SDK_PATH}/lib/lwip)
endif()

add_library(pico-usbnet STATIC
    src/usbnet.c
    src/usb_descriptors.c
    # TinyUSB DHCP/DNS servers
    ${PICO_TINYUSB_PATH}/lib/networking/dhserver.c
    ${PICO_TINYUSB_PATH}/lib/networking/dnserver.c
    # lwIP core, compiled into the library (the SDK's pico_lwip_nosys target is
    # INTERFACE-only, so its sources would otherwise be compiled into the consumer)
    ${PICO_LWIP_PATH}/src/core/altcp.c
    ${PICO_LWIP_PATH}/src/core/altcp_alloc.c
    ${PICO_LWIP_PATH}/src/core/altcp_tcp.c
    ${PICO_LWIP_PATH}/src/core/def.c
    ${PICO_LWIP_PATH}/src/core/dns.c
    ${PICO_LWIP_PATH}/src/core/inet_chksum.c
    ${PICO_LWIP_PATH}/src/core/init.c
    ${PICO_LWIP_PATH}/src/core/ip.c
    ${PICO_LWIP_PATH}/src/core/mem.c
    ${PICO_LWIP_PATH}/src/core/memp.c
    ${PICO_LWIP_PATH}/src/core/netif.c
    ${PICO_LWIP_PATH}/src/core/pbuf.c
    ${PICO_LWIP_PATH}/src/core/raw.c
    ${PICO_LWIP_PATH}/src/core/stats.c
    ${PICO_LWIP_PATH}/src/core/sys.c
    ${PICO_LWIP_PATH}/src/core/tcp.c
    ${PICO_LWIP_PATH}/src/core/tcp_in.c
    ${PICO_LWIP_PATH}/src/core/tcp_out.c
    ${PICO_LWIP_PATH}/src/core/timeouts.c
    ${PICO_LWIP_PATH}/src/core/udp.c
    ${PICO_LWIP_PATH}/src/core/ipv4/autoip.c
    ${PICO_LWIP_PATH}/src/core/ipv4/dhcp.c
    ${PICO_LWIP_PATH}/src/core/ipv4/etharp.c
    ${PICO_LWIP_PATH}/src/core/ipv4/icmp.c
    ${PICO_LWIP_PATH}/src/core/ipv4/igmp.c
    ${PICO_LWIP_PATH}/src/core/ipv4/ip4.c
    ${PICO_LWIP_PATH}/src/core/ipv4/ip4_addr.c
    ${PICO_LWIP_PATH}/src/core/ipv4/ip4_frag.c
    ${PICO_LWIP_PATH}/src/netif/ethernet.c
)

# src/include holds pico-usbnet.h plus the module's tusb_config.h / lwipopts.h
# defaults; it must be found before any SDK-provided alternative
target_include_directories(pico-usbnet PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/src/include
    ${PICO_TINYUSB_PATH}/lib/networking
)

target_link_libraries(pico-usbnet PUBLIC
    pico_stdlib
    tinyusb_device
    pico_lwip_nosys
    pico_unique_id
)
