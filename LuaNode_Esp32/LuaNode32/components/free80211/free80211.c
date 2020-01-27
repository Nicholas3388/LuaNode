#include "esp_system.h"

int8_t ieee80211_freedom_output(uint32_t *ifx, uint8_t *buffer, uint16_t len);
extern void *g_wifi_menuconfig;
uint32_t zeros[64] = {0x00000000};

/*
 * ieee80211_freedom_output returns -4 if value at addr [content of register a7]
 * + 149 is 0. So we need to find out which address [content of register a7]
 * refers to and set that to 1. Due to window shifting by 8, the contents of
 * register a7 will most likely be in a15. This address turns out to be
 * &g_wifi_menuconfig - 0x253, propably some local variable? I don't know why
 * this works, but after setting this local variable to something other than 0,
 * outputting arbitrary 802.11 frames works fine.
 *
 * ieee80211_freedom_output also seems to read stuff from offsets starting at
 * the address its first parameter refers to, so we need to make sure this
 * address is accessible. In order to make sure other code doesn't interfere
 * with ieee80211_freedom_output we just pass an array of zeros. The first
 * parameter is propably supposed to be a pointer to the wifi peripheral or
 * config options, but since we don't really know what ieee80211_freedom_output
 * expects, we just use this workaround.
 *
 * Parameters:
 * buffer: Raw IEEE 802.11 packet to send, will be sent as-is, apart from bytes
 * 24 - 31 which will be set to 0x00 in case buffer[0] is 0x80. len: Length of
 * IEEE 802.11 packet. Must be larger than 23 and smaller than or equal 0x578.
 *
 * Return values:
 * -3: Invalid packet length.
 * -2: Something is wrong with the frame header (the first two bytes) of the
 * buffer. -1: ?? 0: Everything worked fine.
 */

int8_t free80211_send(uint8_t *buffer, uint16_t len)
{
    int8_t rval = 0;

    *(uint32_t *)((uint32_t)&g_wifi_menuconfig - 0x253) = 1;
    rval = ieee80211_freedom_output(zeros, buffer, len);
    if (rval == -4)
        asm("ill");
    return rval;
}
