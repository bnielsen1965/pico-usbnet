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

# ---- Optional: apply local TinyUSB patches ----
# The pinned TinyUSB has an RP2040 Errata-15 bug: a Zero-Length Packet deferred by
# the E15 workaround is re-armed on the next SOF without resetting the buffer select,
# so after an even-packet double-buffered transfer the ZLP lands in the unselected
# buffer and the Bulk-IN endpoint NAKs forever (a permanent TX deadlock).
#
# Apply the local patch to the TinyUSB we consume at build time so consumers don't
# have to patch the submodule by hand. It is idempotent (safe to re-run) and survives
# `git submodule update`. Disable with -DPICO_USBNET_TINYUSB_PATCHES=OFF once the fix
# lands in upstream TinyUSB (see patches/tinyusb-rp2040-e15-zlp.patch).
option(PICO_USBNET_TINYUSB_PATCHES
       "Apply pico-usbnet's local TinyUSB patches to ${PICO_TINYUSB_PATH} at build time" ON)

if (PICO_USBNET_TINYUSB_PATCHES)
    # --- RP2040 Errata-15: deferred-ZLP buffer-select deadlock ---
    # Idempotency is checked by looking for a line that only exists once the
    # patch is applied. `patch`'s own return codes are not reliable for this
    # (a plain re-apply fails rc=1 and leaves .rej cruft), so we detect first.
    set(_pwn_e15_patch ${CMAKE_CURRENT_LIST_DIR}/patches/tinyusb-rp2040-e15-zlp.patch)
    set(_pwn_e15_target ${PICO_TINYUSB_PATH}/src/portable/raspberrypi/rp2040/dcd_rp2040.c)
    file(READ ${_pwn_e15_target} _pwn_e15_src)
    if (_pwn_e15_src MATCHES "const bool fresh = \\(ep->xferred_len == 0\\)")
        message(STATUS "TinyUSB patch already applied: ${_pwn_e15_patch}")
    else()
        execute_process(
            COMMAND patch -p1 -f -d ${PICO_TINYUSB_PATH}
            INPUT_FILE ${_pwn_e15_patch}
            RESULT_VARIABLE _pwn_e15_rc
            OUTPUT_VARIABLE _pwn_e15_out
            ERROR_VARIABLE _pwn_e15_err)
        if (NOT _pwn_e15_rc EQUAL 0)
            message(FATAL_ERROR
                "Failed to apply TinyUSB patch ${_pwn_e15_patch} to ${PICO_TINYUSB_PATH}:\n"
                "${_pwn_e15_err}\n"
                "The TinyUSB version may have changed; rebase the patch or build with "
                "-DPICO_USBNET_TINYUSB_PATCHES=OFF.")
        endif()
        message(STATUS "Applied TinyUSB patch: ${_pwn_e15_patch}")
    endif()
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
