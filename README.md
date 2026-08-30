# pico-usbnet

A standalone CMake library that turns a Raspberry Pi Pico into a **USB Ethernet
gadget**: TinyUSB CDC-NCM + lwIP netif + optional DHCP/DNS servers.

## Features

- CDC-NCM USB Ethernet (cross-platform: Linux, macOS, Windows via MS OS 2.0 descriptor)
- lwIP `NO_SYS` stack compiled into the library — one `usbnet_service()` call pumps everything
- OS-agnostic core: call `usbnet_service()` from a superloop or wrap it in a FreeRTOS task
- Built-in DHCP server (TinyUSB dhserver) with a configurable lease table
- Built-in DNS server (TinyUSB dnserver) resolving a configurable name, plus an optional consumer callback
- MAC address derived from the board unique ID by default (locally-administered, unicast), or consumer-supplied
- All TinyUSB / lwIP defaults overridable via `#ifndef` (see `src/include/tusb_config.h` and `src/include/lwipopts.h`)
- Drop-in integration via CMake `add_subdirectory()` (also works as a git submodule)

## Requirements

- Raspberry Pi Pico (or compatible RP2040 board)
- Pico SDK installed and `PICO_SDK_PATH` set (tested with 2.3.0)
- CMake 3.12+
- ARM GCC toolchain (via Pico SDK)

## Usage in Your CMake Project

### 1. Include the library

Add the library as a subdirectory in your `CMakeLists.txt` (after `pico_sdk_init()`):

```cmake
add_subdirectory(/path/to/pico-usbnet build_usbnet)
```

### 2. Link your target

```cmake
target_link_libraries(your_target pico_stdlib pico-usbnet)

# TinyUSB is used directly for NCM; disable USB stdio or the two
# USB interfaces will conflict
pico_enable_stdio_usb(your_target 0)
```

### 3. Use in code

```c
#include "pico-usbnet.h"

static dhcp_entry_t dhcp_entries[] = {
    {{0}, USBNET_INIT_IP4(192, 168, 7, 2), 24 * 60 * 60},
    {{0}, USBNET_INIT_IP4(192, 168, 7, 3), 24 * 60 * 60},
};

int main(void) {
    stdio_init_all();

    usbnet_config_t cfg = {
        .ip = USBNET_INIT_IP4(192, 168, 7, 1),
        .netmask = USBNET_INIT_IP4(255, 255, 255, 0),
        .gateway = USBNET_INIT_IP4(0, 0, 0, 0),
        .mac = NULL,                        // NULL = derive from board unique ID
        .dns_name = "pico-usbnet",          // NULL = no DNS server
        .dns_query = NULL,                  // optional extra resolver
        .dhcp_enabled = true,
        .dhcp_entries = dhcp_entries,
        .dhcp_num_entries = 2,
        .dhcp_domain = "pico-usbnet",
    };

    if (usbnet_init(&cfg) != 0) {
        // init failed (bad config, DHCP/DNS server could not start)
        return 1;
    }

    while (1) {
        usbnet_service();
        // ... application work ...
    }
}
```

Under FreeRTOS, run `usbnet_service()` from exactly one task (lwIP stays
`NO_SYS=1`; no lwIP OS porting needed):

```c
void network_task(void *arg) {
    for (;;) {
        usbnet_service();
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
```

## API Reference

All in `src/include/pico-usbnet.h`.

```c
int usbnet_init(const usbnet_config_t *cfg);
```

Initialize TinyUSB, lwIP, the netif and (optionally) the DHCP and DNS servers.
Must be called from a single context before `usbnet_service()`. Returns `0` on
success, `-1` on failure (each init step is bounded by
`USBNET_INIT_TIMEOUT_MS`, default 2000 ms).

```c
void usbnet_service(void);
```

Pump the network stack: TinyUSB task, one in-flight RX frame and lwIP timeout
handlers. Call periodically from the same context that called `usbnet_init()`.

```c
struct netif *usbnet_netif(void);
```

The module's network interface (valid after `usbnet_init()`).

```c
bool usbnet_is_up(void);
```

True once the netif is up and the USB link is configured (the host has
enumerated the device).

### `usbnet_config_t`

| Field | Meaning |
|---|---|
| `ip` | Device address, e.g. 192.168.7.1 |
| `netmask` | e.g. 255.255.255.0 |
| `gateway` | 0.0.0.0 = no gateway |
| `mac` | 6-byte MAC address; NULL = derive from board unique ID (locally-administered, unicast) |
| `dns_name` | Name to resolve to `ip` (also `<name>.<dhcp_domain>`); NULL = no DNS server |
| `dns_query` | Optional extra resolver for other names (NULL = none) |
| `dhcp_enabled` | Enable the built-in DHCP server |
| `dhcp_entries` | Lease table (TinyUSB `dhcp_entry_t`); copied during init |
| `dhcp_num_entries` | Number of entries (max `USBNET_MAX_DHCP_ENTRIES`, default 8) |
| `dhcp_domain` | Domain advertised in DHCP; NULL = `USBNET_DHCP_DOMAIN_DEFAULT` ("pico-usbnet") |

Use `USBNET_INIT_IP4(a, b, c, d)` to fill the `ip4_addr_t` fields in a struct literal.

## Overridable Defaults

Every TinyUSB and lwIP option in `src/include/tusb_config.h` /
`src/include/lwipopts.h` is wrapped in `#ifndef` — redefine any of them (via
`-D` compile definitions or a pre-include) before building. Notable module
options:

| Macro | Default | Meaning |
|---|---|---|
| `USBNET_INIT_TIMEOUT_MS` | 2000 | Bounded init wait per step |
| `USBNET_MAX_DHCP_ENTRIES` | 8 | Max DHCP lease table size |
| `USBNET_DHCP_DOMAIN_DEFAULT` | `"pico-usbnet"` | Default DHCP domain |
| `USBNET_USB_VID` | `0xCafe` | USB vendor ID |
| `USBNET_USB_MANUFACTURER` | `"Pico USBNET"` | USB manufacturer string |
| `USBNET_USB_PRODUCT` | `"Pico USB Ethernet"` | USB product string |
| `USBNET_USB_INTERFACE` | `"USB Network Interface"` | USB interface string |

The USB serial number is always the RP2040 board unique ID.

Note: the module's `tusb_config.h` and `lwipopts.h` are found first (they are
in the library's public include path). Do not define your own copies of either
header in a consuming project.

## Example

See the [`example/`](example/) directory: a minimal firmware at 192.168.7.1
with DHCP (192.168.7.2-.4) and DNS (`pico-usbnet`), verified with `ping` and
`nslookup` on a Linux host.

## License

See [LICENSE](LICENSE).
