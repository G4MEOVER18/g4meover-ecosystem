#pragma once

#include "base.h"

#define SUBGHZ_PROTOCOL_PRASTEL_NAME "Prastel"

typedef struct SubGhzProtocolDecoderPrastel SubGhzProtocolDecoderPrastel;
typedef struct SubGhzProtocolEncoderPrastel SubGhzProtocolEncoderPrastel;

extern const SubGhzProtocolDecoder subghz_protocol_prastel_decoder;
extern const SubGhzProtocolEncoder subghz_protocol_prastel_encoder;
extern const SubGhzProtocol subghz_protocol_prastel;

/**
 * Allocate SubGhzProtocolEncoderPrastel.
 * @param environment Pointer to a SubGhzEnvironment instance
 * @return SubGhzProtocolEncoderPrastel* pointer to a SubGhzProtocolEncoderPrastel instance
 */
void* subghz_protocol_encoder_prastel_alloc(SubGhzEnvironment* environment);

/**
 * Free SubGhzProtocolEncoderPrastel.
 * @param context Pointer to a SubGhzProtocolEncoderPrastel instance
 */
void subghz_protocol_encoder_prastel_free(void* context);

/**
 * Deserialize and generating an upload to send.
 * @param context Pointer to a SubGhzProtocolEncoderPrastel instance
 * @param flipper_format Pointer to a FlipperFormat instance
 * @return status
 */
SubGhzProtocolStatus
    subghz_protocol_encoder_prastel_deserialize(void* context, FlipperFormat* flipper_format);

/**
 * Forced transmission stop.
 * @param context Pointer to a SubGhzProtocolEncoderPrastel instance
 */
void subghz_protocol_encoder_prastel_stop(void* context);

/**
 * Getting the level and duration of the upload to be loaded into DMA.
 * @param context Pointer to a SubGhzProtocolEncoderPrastel instance
 * @return LevelDuration
 */
LevelDuration subghz_protocol_encoder_prastel_yield(void* context);

/**
 * Allocate SubGhzProtocolDecoderPrastel.
 * @param environment Pointer to a SubGhzEnvironment instance
 * @return SubGhzProtocolDecoderPrastel* pointer to a SubGhzProtocolDecoderPrastel instance
 */
void* subghz_protocol_decoder_prastel_alloc(SubGhzEnvironment* environment);

/**
 * Free SubGhzProtocolDecoderPrastel.
 * @param context Pointer to a SubGhzProtocolDecoderPrastel instance
 */
void subghz_protocol_decoder_prastel_free(void* context);

/**
 * Reset decoder SubGhzProtocolDecoderPrastel.
 * @param context Pointer to a SubGhzProtocolDecoderPrastel instance
 */
void subghz_protocol_decoder_prastel_reset(void* context);

/**
 * Parse a raw sequence of levels and durations received from the air.
 * @param context Pointer to a SubGhzProtocolDecoderPrastel instance
 * @param level Signal level true-high false-low
 * @param duration Duration of this level in, us
 */
void subghz_protocol_decoder_prastel_feed(void* context, bool level, uint32_t duration);

/**
 * Getting the hash sum of the last randomly received parcel.
 * @param context Pointer to a SubGhzProtocolDecoderPrastel instance
 * @return hash Hash sum
 */
uint32_t subghz_protocol_decoder_prastel_get_hash_data(void* context);

/**
 * Serialize data SubGhzProtocolDecoderPrastel.
 * @param context Pointer to a SubGhzProtocolDecoderPrastel instance
 * @param flipper_format Pointer to a FlipperFormat instance
 * @param preset The modulation on which the signal was received, SubGhzRadioPreset
 * @return status
 */
SubGhzProtocolStatus subghz_protocol_decoder_prastel_serialize(
    void* context,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset);

/**
 * Deserialize data SubGhzProtocolDecoderPrastel.
 * @param context Pointer to a SubGhzProtocolDecoderPrastel instance
 * @param flipper_format Pointer to a FlipperFormat instance
 * @return status
 */
SubGhzProtocolStatus
    subghz_protocol_decoder_prastel_deserialize(void* context, FlipperFormat* flipper_format);

/**
 * Getting a textual representation of the received data.
 * @param context Pointer to a SubGhzProtocolDecoderPrastel instance
 * @param output Resulting text
 */
void subghz_protocol_decoder_prastel_get_string(void* context, FuriString* output);
