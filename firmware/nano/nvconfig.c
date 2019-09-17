
#include <stdint.h>
#include "nvconfig.h"
#include "radio.h"
#include "diag.h"
#include <avr/io.h>
#include <avr/eeprom.h>

#include <string.h>

static void * eeprom_addr = (void *) 0;

#define NVCONFIG_MAGIC 0x442a

typedef struct {
    uint16_t magic;
    uint8_t tx_id[4];
    uint8_t hop_channels[NR_HOP_CHANNELS];
} nvconfig_layout;

void nvconfig_load()
{
    nvconfig_layout nvdata;

    eeprom_read_block((void *) &nvdata, eeprom_addr, sizeof(nvdata));
    if (nvdata.magic == NVCONFIG_MAGIC) {
        // Great.
        // Copy the config data.
        memcpy(radio_state.tx_id, nvdata.tx_id, 4);
        memcpy(radio_state.hop_channels, nvdata.hop_channels, NR_HOP_CHANNELS);
    }
}

void nvconfig_save()
{
    nvconfig_layout nvdata;
    nvdata.magic = NVCONFIG_MAGIC;
    memcpy(nvdata.tx_id, radio_state.tx_id, 4);
    memcpy(nvdata.hop_channels, radio_state.hop_channels, NR_HOP_CHANNELS);
    eeprom_update_block((void *) &nvdata, eeprom_addr, sizeof(nvdata));
}


