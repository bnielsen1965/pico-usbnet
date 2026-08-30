# pico-usbnet Example

A minimal USB Ethernet gadget: the Pico comes up at **192.168.7.1/24** with a
DHCP server (offers 192.168.7.2-.4) and a DNS server that resolves
**pico-usbnet** to 192.168.7.1. The board LED blinks while the stack is pumped.

## Requirements

- Raspberry Pi Pico (or compatible RP2040 board)
- Pico SDK installed
- CMake 3.12+
- A USB host (e.g. a Linux machine) to plug into

## Build

### 1. Set environment variables

```bash
export PICO_SDK_PATH=/path/to/pico-sdk
export PICO_BOARD=pico
```

Or run the provided setup script:

```bash
./setup.sh
```

### 2. Build

```bash
mkdir -p build && cd build
cmake ..
make
```

The compiled firmware (`example.uf2`) will be in the `build` directory.

## Flash

Copy `example.uf2` to the Pico while holding the BOOTSEL button, or use:

```bash
picotool load build/example.uf2
```

## Verify on the host

Plug the Pico in via USB. The host should create a USB Ethernet interface and
obtain 192.168.7.2 via DHCP, then:

```bash
ping 192.168.7.1        # ICMP (broadcast/multicast ping enabled)
nslookup pico-usbnet    # resolves to 192.168.7.1
```

## Configuration

Edit `main.c` to adjust the `usbnet_config_t` (IP, netmask, gateway, MAC,
DNS name, DHCP leases).
