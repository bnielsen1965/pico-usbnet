#include "pico/stdlib.h"

#include "pico-usbnet.h"

// DHCP leases offered to the host (first matching MAC gets the first free entry)
static dhcp_entry_t g_dhcp_entries[] = {
    {{0}, USBNET_INIT_IP4(192, 168, 7, 2), 24 * 60 * 60},
    {{0}, USBNET_INIT_IP4(192, 168, 7, 3), 24 * 60 * 60},
    {{0}, USBNET_INIT_IP4(192, 168, 7, 4), 24 * 60 * 60},
};

// Example: bring up USB Ethernet (with DHCP and DNS) and pump the stack in a
// superloop while blinking the board LED
int main(void) {
    stdio_init_all();

    usbnet_config_t cfg = {
        .ip = USBNET_INIT_IP4(192, 168, 7, 1),
        .netmask = USBNET_INIT_IP4(255, 255, 255, 0),
        .gateway = USBNET_INIT_IP4(0, 0, 0, 0), // no gateway: the Pico is an end device, not a router
        .mac = NULL,
        .dns_name = "pico-usbnet",
        .dns_query = NULL,
        .dhcp_enabled = true,
        .dhcp_entries = g_dhcp_entries,
        .dhcp_num_entries = sizeof(g_dhcp_entries) / sizeof(g_dhcp_entries[0]),
        .dhcp_domain = "pico-usbnet",
    };

    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

    if (usbnet_init(&cfg) != 0) {
        printf("usbnet_init failed\n");
        while (1) {
            gpio_put(PICO_DEFAULT_LED_PIN, 1);
            sleep_ms(100);
        }
    }

    printf("USB Ethernet ready: 192.168.7.1 (dns: pico-usbnet)\n");

    // Superloop: pump the network stack and blink the board LED
    uint32_t last_blink = 0;
    bool led_on = false;
    while (1) {
        usbnet_service();

        uint32_t now = to_ms_since_boot(get_absolute_time());
        if (now - last_blink >= 500) {
            last_blink = now;
            led_on = !led_on;
            gpio_put(PICO_DEFAULT_LED_PIN, led_on);
        }
    }
}
