#pragma once
/* Minimal host mock of lib/subghz/types.h -- only the pieces generic.h/.c
   actually reference (SubGhzRadioPreset, the SubGhzProtocolStatus values
   generic.c's serialize/deserialize helpers return). Field layout and enum
   values below match both the Unleashed and the G4MEOVER-FW/Momentum copy
   of this header for the subset used here. */
#include "furi.h"

#define SUBGHZ_KEY_FILE_VERSION 1
#define SUBGHZ_KEY_FILE_TYPE    "Flipper SubGhz Key File"

typedef struct {
    FuriString* name;
    uint32_t frequency;
    uint8_t* data;
    size_t data_size;
    float latitude;
    float longitude;
} SubGhzRadioPreset;

typedef enum {
    SubGhzProtocolStatusOk = 0,
    SubGhzProtocolStatusError = (-1),
    SubGhzProtocolStatusErrorParserOthers = (-2),
    SubGhzProtocolStatusErrorParserHeader = (-3),
    SubGhzProtocolStatusErrorParserBitCount = (-5),
    SubGhzProtocolStatusErrorParserKey = (-6),
    SubGhzProtocolStatusErrorParserFrequency = (-7),
    SubGhzProtocolStatusErrorParserPreset = (-8),
    SubGhzProtocolStatusErrorParserCustomPreset = (-9),
    SubGhzProtocolStatusErrorParserProtocolName = (-10),
    SubGhzProtocolStatusErrorValueBitCount = (-11),
    SubGhzProtocolStatusErrorParserLatitude = (-14),
    SubGhzProtocolStatusErrorParserLongitude = (-15),
} SubGhzProtocolStatus;
