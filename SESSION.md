# pico-usbnet — Session Brief

## Goal

Build `pico-usbnet`: a standalone, reusable CMake library that turns a Raspberry Pi
Pico into a **USB Ethernet gadget** (TinyUSB CDC-NCM + lwIP netif + optional DHCP/DNS
servers). Extracted from the `usbpixels` project so it can be consumed as a submodule
by usbpixels (FreeRTOS) and future projects.

This session builds ONLY the module. No LED code, no HTTP app, no command protocol.

## Source material (read-only reference — do NOT modify)

Project: `/home/burnin/Projects/usbpixels`

| File | What to take from it |
|---|---|
| `src/core1_network.c` | The core of the module (details below) |
| `src/usb_descriptors.c` | NCM device/interface/endpoint/string descriptors (whole file) |
| `src/tusb_config.h` | TinyUSB config (NCM enabled, OS_NONE) — becomes module default |
| `src/lwipopts.h` | lwIP tuning (NO_SYS, pbuf pools, ICMP/ping on) — becomes module default |
| `CMakeLists.txt` | The lwIP source list (lines 43–76), dhserver/dnserver (38–41), link libs (79–89) |
| `src/main.c` | Note `set_sys_clock_khz(125000)` — NOT part of the module, consumer's job |

### Exact regions of `core1_network.c` to lift

- Lines 26–51: `netif_data`, `received_frame`, IP/netmask/gateway consts, DHCP entries + `dhcp_config_t`
- Lines 186–242: `linkoutput_fn`, `ip4_output_fn`, `ip6_output_fn`, `netif_init_cb`, `init_lwip()`
- Lines 245–251: `dns_query_proc()` (the "usbpixels" name resolution — make this a configurable callback)
- Lines 254–291: `tud_network_recv_cb`, `tud_network_xmit_cb`, `service_traffic()`, `tud_network_init_cb`
- Lines 346–362: the init sequence from `core1_network_task()` (tusb_init → init_lwip → dhserv_init → dnserv_init)

### Explicitly OUT of scope (stays in consumer apps)

- `http_handler.c`, `process_command()`, `send_cmd_*()`, `protocol.h` — app protocol
- `fsdata_custom.c`, web pages — app content
- Everything in `core0_leds.c`
- The core0/core1 task structure and the multicore FIFO

## API design (start here, adjust as needed)

`src/include/pico-usbnet.h`:

```c
typedef struct {
    uint32_t ip;          // e.g. 192.168.7.1 (host order, or use ip4_addr_t)
    uint32_t netmask;
    uint32_t gateway;     // 0 = none
    const char *dns_name; // name to resolve to `ip` (NULL = no DNS server)
    // DHCP server (optional)
    bool dhcp_enabled;
    uint8_t  dhcp_num_entries;
    // ... or a pointer to a dhcp_entry_t array
} usbnet_config_t;

int      usbnet_init(const usbnet_config_t *cfg); // tusb + lwip + netif + dhcp/dns
void     usbnet_service(void);  // pump: tud_task + ethernet_input + sys_check_timeouts
struct netif *usbnet_netif(void);
bool     usbnet_is_up(void);
```

Design rules:

1. **OS-agnostic core.** The module owns all state; a bare-metal consumer calls
   `usbnet_service()` from its superloop. A FreeRTOS consumer just wraps it in a task.
   Do NOT add FreeRTOS code to the module itself (keep it usable by bare-metal projects).
2. **lwIP stays `NO_SYS=1`** even for RTOS consumers. It works fine under FreeRTOS as
   long as exactly one task pumps `usbnet_service()`. Avoids lwIP OS-mode complexity.
3. **All defaults overridable via `#ifndef`**, the way `tusb_config.h` already does.
   A consumer can redefine options before the module's headers are included.
4. **MAC address**: usbpixels hardcodes `{0x02,0x02,0x84,0x6A,0x96,0x00}` in
   `usb_descriptors.c:31` (it links `pico_unique_id` but never uses it). The module
   should default to deriving the MAC from `pico_unique_id` (locally-administered,
   unicast) with a config option to supply a fixed MAC.
5. Keep the single-`received_frame` model in `service_traffic()` (one in-flight RX
   pbuf; `tud_network_recv_renew()` after processing). It's simple and proven.
6. Keep the init retry loops (`while (!netif_is_up())`, `while (dhserv_init() != ERR_OK)`)
   but consider making them bounded/fatal-returning so a misconfigured consumer gets an
   error instead of a hang — decide during the session.

## Build structure (mirror pico-ws2812 / pico-command-line)

```
pico-usbnet/
  CMakeLists.txt          # pico_sdk_import pattern, static lib target: pico-usbnet
  src/
    include/pico-usbnet.h
    usbnet.c              # netif driver + tud callbacks + init/service
    usb_descriptors.c     # from usbpixels, MAC handling updated
    tusb_config.h         # module default (CFG_TUD_NCM 1, OPT_OS_NONE)
    lwipopts.h            # module default (NO_SYS 1, pools from usbpixels)
  example/
    main.c                # minimal: usbnet_init + usbnet_service loop + LED blink
    CMakeLists.txt
```

CMake notes (from `usbpixels/CMakeLists.txt`):

- The lwIP **core source list** (usbpixels lines 44–76) must be compiled into the
  library — the SDK's `pico_lwip_nosys` target alone does not cover what's needed
  (usbpixels compiles those files explicitly; keep that list).
- Also compile `tinyusb/lib/networking/dhserver.c` and `dnserver.c`.
- Link: `pico_stdlib`, `pico_lwip_nosys`, `tinyusb_device`, `pico_unique_id`,
  `hardware_flash` (may not be needed — verify).
- Include dirs: expose `src/include` PUBLIC. The module's `tusb_config.h` /
  `lwipopts.h` must be found BEFORE any SDK defaults — verify the include order in the
  example build; if a consumer app also defines either header, that's a consumer bug.
- `PICO_SDK_PATH` here is `/home/burnin/Projects/pico-sdk`. Note: this SDK checkout
  predates the `pico_fatfs`/`pico_sdio` modules — irrelevant here, but don't assume
  newer SDK modules exist.
- The example must set `pico_enable_stdio_usb(example 0)` (TinyUSB is used directly
  for NCM; a second USB-CD interface would conflict).
- `BOARD_TUD_RHPORT 0` (USB controller 0).

## Verification plan

1. Example firmware builds clean, UF2 produced.
2. Flash to a Pico, plug into a Linux host. Expect:
   - Host enumerates a USB Ethernet interface.
   - Host gets 192.168.7.2/3 (or next free) via the module's DHCP server.
   - `ping 192.168.7.1` works (ICMP + broadcast/multicast ping enabled).
   - `nslookup <dns_name>` resolves to 192.168.7.1.
3. Optional proof of TCP: a 20-line raw `tcp_listen` accept+echo in the example
   (guarded/optional) — only if ping+DNS feel insufficient.
4. RAM/flash check: `arm-none-eabi-size` the example; report text/bss. The module
   alone (no app) should land well under 100 KB flash, ~40–60 KB bss.

## Known quirks to preserve (or consciously fix, with a comment)

- `init_lwip()` does `netif->hwaddr[5] ^= 0x01` (core1_network.c:235) — flips the last
  octet's LSB. Harmless with a fixed MAC; think carefully if MAC comes from
  unique_id (don't accidentally set the multicast bit, bit 0 of byte 0 must stay 0).
- `ETH_PAD_SIZE 0`, `LWIP_SINGLE_NETIF 1`, `LWIP_IP_ACCEPT_UDP_PORT(p) == 67`
  (only accept DHCP-offer UDP at IP level).
- Memory tuning: `PBUF_POOL_SIZE 16`, `MEMP_NUM_TCP_PCB 8`, `MEMP_NUM_TCP_SEG 16`,
  `MEMP_NUM_UDP_PCB 4` — keep as defaults; small but proven for this use.
- `TCP_MSS 1460`, `TCP_SND_BUF`/`TCP_WND` = 4×MSS.
- `tud_network_init_cb()` must free a pending `received_frame` on re-init
  (handles USB re-enumeration).

## Definition of done

- [ ] `git init`, clean tree, README.md (usage + API + example build instructions)
- [ ] Example builds, pings, DHCP + DNS verified on a real host
- [ ] Zero usbpixels-specific symbols/strings in the module
- [ ] Submodule-ready: can be `git submodule add`-ed into another project and built
      via `add_subdirectory()`
