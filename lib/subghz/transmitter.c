#include "transmitter.h"

#include "protocols/base.h"
#include "registry.h"

struct SubGhzTransmitter {
    const SubGhzProtocol* protocol;
    SubGhzProtocolEncoderBase* protocol_instance;
    const SubGhzProtocolEncoder* encoder_override;
};

static inline const SubGhzProtocolEncoder* txr_get_enc(const SubGhzTransmitter* t) {
    return t->encoder_override ? t->encoder_override : t->protocol->encoder;
}

SubGhzTransmitter*
    subghz_transmitter_alloc_init(SubGhzEnvironment* environment, const char* protocol_name) {
    SubGhzTransmitter* instance = NULL;
    const SubGhzProtocolRegistry* protocol_registry_items =
        subghz_environment_get_protocol_registry(environment);

    const SubGhzProtocol* protocol =
        subghz_protocol_registry_get_by_name(protocol_registry_items, protocol_name);

    if(protocol && protocol->encoder && protocol->encoder->alloc) {
        instance = malloc(sizeof(SubGhzTransmitter));
        instance->protocol = protocol;
        instance->encoder_override = NULL;
        instance->protocol_instance = instance->protocol->encoder->alloc(environment);
    }
    return instance;
}

void subghz_transmitter_free(SubGhzTransmitter* instance) {
    furi_check(instance);
    txr_get_enc(instance)->free(instance->protocol_instance);
    free(instance);
}

SubGhzProtocolEncoderBase* subghz_transmitter_get_protocol_instance(SubGhzTransmitter* instance) {
    furi_check(instance);
    return instance->protocol_instance;
}

bool subghz_transmitter_stop(SubGhzTransmitter* instance) {
    furi_check(instance);
    bool ret = false;
    const SubGhzProtocolEncoder* enc = txr_get_enc(instance);
    if(enc && enc->stop) {
        enc->stop(instance->protocol_instance);
        ret = true;
    }
    return ret;
}

SubGhzProtocolStatus
    subghz_transmitter_deserialize(SubGhzTransmitter* instance, FlipperFormat* flipper_format) {
    furi_check(instance);
    SubGhzProtocolStatus ret = SubGhzProtocolStatusError;
    const SubGhzProtocolEncoder* enc = txr_get_enc(instance);
    if(enc && enc->deserialize) {
        ret = enc->deserialize(instance->protocol_instance, flipper_format);
    }
    return ret;
}

LevelDuration subghz_transmitter_yield(void* context) {
    SubGhzTransmitter* instance = context;
    return txr_get_enc(instance)->yield(instance->protocol_instance);
}

SubGhzTransmitter* subghz_transmitter_alloc_init_with_encoder(
    SubGhzEnvironment* environment,
    const char* protocol_name,
    const SubGhzProtocolEncoder* encoder_vtable) {
    furi_assert(encoder_vtable);
    furi_assert(encoder_vtable->alloc);

    const SubGhzProtocolRegistry* registry = subghz_environment_get_protocol_registry(environment);
    const SubGhzProtocol* protocol = subghz_protocol_registry_get_by_name(registry, protocol_name);
    if(!protocol) return NULL;

    SubGhzTransmitter* instance = malloc(sizeof(SubGhzTransmitter));
    instance->protocol = protocol;
    instance->encoder_override = encoder_vtable;
    instance->protocol_instance = encoder_vtable->alloc(environment);
    return instance;
}
