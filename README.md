# pico-usbnet

A standalone CMake library that turns a Raspberry Pi Pico into a **USB Ethernet
gadget**: TinyUSB CDC-NCM + lwIP netif + optional DHCP/DNS servers.

## Features

- CDC-NCM USB Ethernet (cross-platform: Linux, macOS, Windows 10/11 via MS OS 2.0 descriptor)
- lwIP `NO_SYS` stack compiled into the library — one `usbnet_service()` call pumps everything
- OS-agnostic core: call `usbnet_service()` from a superloop or wrap it in a FreeRTOS task
- Built-in DHCP server (TinyUSB dhserver) with a configurable lease table
- Built-in DNS server (TinyUSB dnserver) resolving a configurable name, plus an optional consumer callback
- MAC address derived from the board unique ID by default (locally-administered, unicast), or consumer-supplied
- Internally distinct lwIP netif MAC (USB descriptor MAC + 1) so the host doesn't reject DHCP frames where source and destination MAC match
- All TinyUSB / lwIP defaults overridable via `#ifndef` (see `src/include/tusb_config.h` and `src/include/lwipopts.h`)
- Drop-in integration via CMake `add_subdirectory()` (also works as a git submodule)

## Requirements

- Raspberry Pi Pico (or compatible RP2040 board)
- Pico SDK installed and `PICO_SDK_PATH` set (tested with 2.3.1)
- **TinyUSB 0.21.0** — fetched automatically at build time by `pico_usbnet_tinyusb.cmake` (Pico SDK 2.3.1 ships 0.18.0; see below)
- CMake 3.12+
- ARM GCC toolchain (via Pico SDK)

### TinyUSB 0.21.0 (fetched automatically)

Pico SDK 2.3.1 pins TinyUSB at 0.18.0. For reliable Windows 10/11 CDC-NCM
support you need 0.21.0. Rather than hand-updating the SDK's submodule, include
`pico_usbnet_tinyusb.cmake` before `pico_sdk_init()` (see below); it clones
TinyUSB 0.21.0 into the build tree once and points the SDK at it.

If you already set `PICO_TINYUSB_PATH` (or want to build offline / air-gapped),
the helper uses your copy as-is and fetches nothing. To build against the SDK's
own submodule instead, update it manually and set `PICO_TINYUSB_PATH` to it:

 ```bash
 cd $PICO_SDK_PATH/lib/tinyusb
 git fetch origin --tags
 git checkout 0.21.0
 ```
 
 ### Automatic TinyUSB Patch (RP2040 E15 ZLP deadlock)

 TinyUSB 0.21.0 still has an RP2040 Errata-15 bug that can permanently stall a
 Bulk-IN endpoint: a Zero-Length Packet deferred by the E15 workaround is
 re-armed on the next SOF without resetting the buffer select, so after an
 even-packet double-buffered transfer (e.g. a 1280-byte NCM data NTB) the ZLP
 lands in the unselected buffer and the endpoint NAKs forever.

 pico-usbnet ships a local patch for this and applies it to the TinyUSB you build
 against (`$PICO_TINYUSB_PATH`) automatically at CMake configure time — so you do
 **not** need to patch the submodule by hand, and it survives `git submodule update`.
 See `patches/tinyusb-rp2040-e15-zlp.patch`.

 Once the fix is merged upstream into your TinyUSB version, disable it with:

 ```bash
 cmake -DPICO_USBNET_TINYUSB_PATCHES=OFF ...
 ```

 ## Usage in Your CMake Project

### 1. Include the library

Fetch TinyUSB 0.21.0 (or use an existing `PICO_TINYUSB_PATH`) **before**
`pico_sdk_init()`, then add the library as a subdirectory **after** it:

```cmake
include(/path/to/pico-usbnet/pico_usbnet_tinyusb.cmake)  # before pico_sdk_init()
pico_sdk_init()
add_subdirectory(/path/to/pico-usbnet build_usbnet)       # after pico_sdk_init()
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
        .gateway = USBNET_INIT_IP4(0, 0, 0, 0), // no gateway: the Pico is an end device, not a router
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

## Multi-core (SMP) builds

If you build FreeRTOS for **more than one core** (`configNUMBER_OF_CORES > 1`),
pin the task that calls `usbnet_service()` — the same task that called
`usbnet_init()` — to a **single core**.

**Why.** TinyUSB's RP2040 DCD keeps per-endpoint state that is updated from two
contexts — your `usbnet_service()` "worker" and the USB interrupt handler — using
non-atomic read-modify-write. The guard around it, `hw_endpoint_lock_update()`, is
a **no-op** in TinyUSB (it carries an open upstream TODO to make it a real
critical section and notes the worker and IRQ would "make sense … on same core").
On one core this is safe (the IRQ only preempts your task). On SMP, if your task
migrates to a core other than the one that owns the USB interrupt, the two
contexts run truly concurrently with no lock and can tear the endpoint state.
`usbnet_init()` calls `tusb_init()`, which binds the USB interrupt to the core it
runs on — so the core that calls `usbnet_init()` is the one that "owns" USB, and
the `usbnet_service()` task must stay on it. Getting this wrong shows up as an
**intermittent NCM IN (TX) wedge after hours of traffic** that only a USB bus
reset clears.

**Fix.** Create the net task with a fixed core affinity (requires
`configUSE_CORE_AFFINITY 1`; the affinity argument is a **bitmask** of cores).
Pinned to core 0:

```c
xTaskCreateAffinitySet(network_task, "usbnet", 1024, NULL, 5,
                       (UBaseType_t)(1u << 0) /* core 0 only */, NULL);
```

Leave any other tasks (e.g. a display/LED task) unpinned — they never touch the
USB stack. Single-core builds are not affected.

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

```c
void usbnet_get_stats(uint32_t *rx_frames, uint32_t *rx_nobuf,
                      uint32_t *tx_frames, uint32_t *tx_drops);
```

Read the four frame/drop counters (see **Diagnostics**). Any argument may be
`NULL` to skip. The counters are cumulative since boot and safe to read from
any task (32-bit aligned reads).

### `usbnet_config_t`

| Field | Meaning |
|---|---|
| `ip` | Device address, e.g. 192.168.7.1 |
| `netmask` | e.g. 255.255.255.0 |
| `gateway` | Advertised to the host as the DHCP router (opt 3). `0.0.0.0` = no gateway (recommended). The Pico is an end device, not a router — do **not** set it to the device's own IP |
| `mac` | 6-byte MAC address; NULL = derive from board unique ID (locally-administered, unicast) |
| `dns_name` | Name to resolve to `ip` (also `<name>.<dhcp_domain>`); NULL = no DNS server |
| `dns_query` | Optional extra resolver for other names (NULL = none) |
| `dhcp_enabled` | Enable the built-in DHCP server |
| `dhcp_entries` | Lease table (TinyUSB `dhcp_entry_t`); copied during init |
| `dhcp_num_entries` | Number of entries (max `USBNET_MAX_DHCP_ENTRIES`, default 8) |
| `dhcp_domain` | Domain advertised in DHCP; NULL = `USBNET_DHCP_DOMAIN_DEFAULT` ("pico-usbnet") |

Use `USBNET_INIT_IP4(a, b, c, d)` to fill the `ip4_addr_t` fields in a struct literal.

## Diagnostics

`usbnet_get_stats()` exposes four cumulative frame counters (since boot) that
let you tell, without a debugger, which layer a network stall is in. Because
they are cumulative, read them twice and compare the deltas — a console command
in a separate task is a fine reader.

| Counter | Bumped when |
|---|---|
| `rx_frames` | a host→device NCM frame is copied into a pool pbuf (accepted into the RX buffer) |
| `rx_nobuf` | `pbuf_alloc(PBUF_POOL)` failed for an incoming frame — the pbuf pool is exhausted and the frame is dropped |
| `tx_frames` | a device→host frame is copied out to the NCM IN endpoint |
| `tx_drops` | the bounded NCM TX wait (`USBNET_XMIT_TIMEOUT_MS`) expired — the host was not reading the NCM IN endpoint, so the frame is dropped |

Reading the deltas:

| Pattern | Diagnosis |
|---|---|
| `rx_frames` flat + `tx_frames` flat | not receiving — NCM OUT endpoint not armed / NTB desync |
| `rx_frames` rising + `tx_frames` flat | receiving but not replying — lwIP/ARP layer |
| `rx_frames` rising + `tx_drops` rising | replying but the host is not reading NCM IN — TX/host side |
| `rx_nobuf` rising | pbuf pool exhaustion (memory pressure) |

A healthy link shows `rx_nobuf = 0` and a flat `tx_drops`; under traffic,
`rx_frames` and `tx_frames` track each other (plus protocol overhead).

## Overridable Defaults

Every TinyUSB and lwIP option in `src/include/tusb_config.h` /
`src/include/lwipopts.h` is wrapped in `#ifndef` — redefine any of them (via
`-D` compile definitions or a pre-include) before building. Notable module
options:

| Macro | Default | Meaning |
|---|---|---|
| `USBNET_INIT_TIMEOUT_MS` | 2000 | Bounded init wait per step |
| `USBNET_XMIT_TIMEOUT_MS` | 10 | Bounded NCM TX wait in `linkoutput`; frames drop after (see Diagnostics) |
| `USBNET_MAX_DHCP_ENTRIES` | 8 | Max DHCP lease table size |
| `USBNET_DHCP_DOMAIN_DEFAULT` | `"pico-usbnet"` | Default DHCP domain |
| `USBNET_USB_VID` | `0xCafe` | USB vendor ID |
| `USBNET_USB_MANUFACTURER` | `"Pico USBNET"` | USB manufacturer string |
| `USBNET_USB_PRODUCT` | `"Pico USB Ethernet"` | USB product string |
| `USBNET_USB_INTERFACE` | `"USB Network Interface"` | USB interface string |
| `CFG_TUD_NCM_IN_NTB_N` | 3 | TX NTB buffers; 1 causes indefinite TX stall if a transfer is NAKed |

The USB serial number is always the RP2040 board unique ID.

Note: the module's `tusb_config.h` and `lwipopts.h` are found first (they are
in the library's public include path). Do not define your own copies of either
header in a consuming project.

## Windows 10/11 Compatibility

CDC-NCM on Windows 10+ uses the built-in `UsbNcm.sys` driver (no manual install
needed — the MS OS 2.0 descriptor with compatible ID `WINNCM` triggers auto-loading).

TinyUSB 0.21.0 includes all the NCM and DHCP fixes needed for reliable Windows 10
operation (notification ordering, ZLP handling, class request ACKs, DHCP broadcast
flag, option ordering, payload padding). No patches are required.

pico-usbnet additionally handles one quirk at the application level:

- **Distinct internal MAC** — The USB descriptor MAC becomes the host's
  interface MAC. The lwIP netif uses a derived MAC (last octet + 1) so that
  DHCP OFFER/ACK frames have a source MAC different from the host's own MAC.
  Without this, Windows silently rejects the DHCP reply.

## Example

See the [`example/`](example/) directory: a minimal firmware at 192.168.7.1
with DHCP (192.168.7.2-.4) and DNS (`pico-usbnet`), verified with `ping` and
`nslookup` on Linux and DHCP on Windows 10.

Resource usage (example, `arm-none-eabi-size`): ~65 KB text, ~40 KB bss.

## License

See [LICENSE](LICENSE).
