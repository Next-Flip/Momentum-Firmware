#include <furi.h>
#include "u2f_nfc.h"
#include "u2f.h"
#include "u2f_data.h"
#include <furi_hal_random.h>
#include <nfc/nfc_listener.h>
#include <nfc/protocols/iso14443_4a/iso14443_4a_listener.h>
#include <nfc_device.h>

#define TAG "U2fNfc"

#define NAD_MASK               0x08
#define U2F_APDU_GET_RESPONSE  0xC0
#define U2F_APDU_SELECT        0xA4
#define U2F_APDU_CHUNK_SIZE    96
#define U2F_NFC_PAYLOAD_SIZE   960
#define U2F_NFC_TX_BUFFER_SIZE 480

#define U2F_SW_CONDITIONS_NOT_SATISFIED_1 0x69
#define U2F_SW_CONDITIONS_NOT_SATISFIED_2 0x85
#define U2F_SW_NO_ERROR_1                 0x90
#define U2F_SW_NO_ERROR_2                 0x00

struct U2fNfc {
    Nfc* nfc;
    NfcDevice* nfc_device;
    NfcListener* listener;
    U2fData* u2f_instance;
    uint8_t payload[U2F_NFC_PAYLOAD_SIZE];
    uint16_t payload_len;
    uint16_t payload_cursor;
    bool applet_selected;
    BitBuffer* tx_buffer;
};

static void u2f_nfc_clear_pending_response(U2fNfc* u2f_nfc) {
    furi_assert(u2f_nfc);
    u2f_nfc->payload_len = 0;
    u2f_nfc->payload_cursor = 0;
}

static bool u2f_nfc_transmit_apdu(
    U2fNfc* u2f_nfc,
    Iso14443_4aListener* iso14443_listener,
    const uint8_t* apdu,
    uint16_t apdu_len) {
    furi_assert(u2f_nfc);
    furi_assert(iso14443_listener);
    furi_assert(apdu);

    BitBuffer* tx_buffer = u2f_nfc->tx_buffer;
    bit_buffer_reset(tx_buffer);

    bit_buffer_append_bytes(tx_buffer, apdu, apdu_len);

    Iso14443_4aError error = iso14443_4a_listener_send_block(iso14443_listener, tx_buffer);
    if(error != Iso14443_4aErrorNone) {
        FURI_LOG_W(TAG, "Tx error: %d", error);
        return false;
    }

    return true;
}

static bool
    u2f_nfc_send_pending_response(U2fNfc* u2f_nfc, Iso14443_4aListener* iso14443_listener) {
    furi_assert(u2f_nfc);

    if(u2f_nfc->payload_len == 0) {
        FURI_LOG_W(TAG, "No pending response to send");
        return false;
    }

    uint16_t response_data_len = (u2f_nfc->payload_len >= 2) ? (u2f_nfc->payload_len - 2) :
                                                               u2f_nfc->payload_len;

    if(u2f_nfc->payload_cursor > response_data_len) {
        u2f_nfc_clear_pending_response(u2f_nfc);
        return false;
    }

    uint16_t remaining = response_data_len - u2f_nfc->payload_cursor;
    uint16_t chunk_len = MIN(remaining, (uint16_t)U2F_APDU_CHUNK_SIZE);
    const uint8_t* response = u2f_nfc->payload + u2f_nfc->payload_cursor;

    if(chunk_len == remaining) {
        uint8_t final_response[U2F_APDU_CHUNK_SIZE + 2];
        if(chunk_len > 0) {
            memcpy(final_response, response, chunk_len);
        }
        memcpy(final_response + chunk_len, u2f_nfc->payload + response_data_len, 2);

        bool success =
            u2f_nfc_transmit_apdu(u2f_nfc, iso14443_listener, final_response, chunk_len + 2);

        u2f_nfc_clear_pending_response(u2f_nfc);
        return success;
    }

    uint16_t rest = remaining - chunk_len;
    uint8_t chunk_response[U2F_APDU_CHUNK_SIZE + 2];
    memcpy(chunk_response, response, chunk_len);
    chunk_response[chunk_len] = 0x61;
    chunk_response[chunk_len + 1] = (rest > 0xFF) ? 0x00 : (uint8_t)rest;

    if(!u2f_nfc_transmit_apdu(u2f_nfc, iso14443_listener, chunk_response, chunk_len + 2)) {
        return false;
    }

    u2f_nfc->payload_cursor += chunk_len;
    return true;
}

NfcCommand u2f_nfc_worker_listener_callback(NfcGenericEvent event, void* context) {
    furi_assert(context);
    furi_assert(event.protocol == NfcProtocolIso14443_4a);
    furi_assert(event.event_data);
    U2fNfc* u2f_nfc = context;
    U2fData* u2f = u2f_nfc->u2f_instance;

    NfcCommand ret = NfcCommandContinue;
    Iso14443_4aListenerEvent* iso14443_4a_event = event.event_data;
    Iso14443_4aListener* iso14443_listener = event.instance;

    BitBuffer* tx_buffer = u2f_nfc->tx_buffer;
    bit_buffer_reset(tx_buffer);

    switch(iso14443_4a_event->type) {
    case Iso14443_4aListenerEventTypeReceivedData: {
        BitBuffer* rx_buffer = iso14443_4a_event->data->buffer;
        const uint8_t* rx_data = bit_buffer_get_data(rx_buffer);
        uint16_t rx_size = bit_buffer_get_size_bytes(rx_buffer);

        if(rx_size == 0) {
            FURI_LOG_W(TAG, "No APDU in frame: %u", rx_size);
            break;
        }

        uint8_t ins = rx_data[1];
        FURI_LOG_D(
            TAG, "Req ins=%02x len=%u sel=%u", ins, rx_size, u2f_nfc->applet_selected ? 1 : 0);

        if(u2f_nfc->payload_len > 0) {
            if(rx_size >= 2 && ins == U2F_APDU_GET_RESPONSE) {
                u2f_nfc_send_pending_response(u2f_nfc, iso14443_listener);
                break;
            }

            FURI_LOG_D(TAG, "Replacing pending response with new APDU");
            u2f_nfc_clear_pending_response(u2f_nfc);
        }

        if(!u2f_nfc->applet_selected && ins != U2F_APDU_SELECT) {
            const uint8_t reject_sw[2] = {
                U2F_SW_CONDITIONS_NOT_SATISFIED_1,
                U2F_SW_CONDITIONS_NOT_SATISFIED_2,
            };
            FURI_LOG_W(TAG, "Reject APDU before SELECT: ins=%02x", ins);
            if(!u2f_nfc_transmit_apdu(u2f_nfc, iso14443_listener, reject_sw, sizeof(reject_sw))) {
                break;
            }
            break;
        }

        // Leave 2 bytes for HID len field
        memcpy(u2f_nfc->payload, rx_data, MIN(rx_size, 4));
        if(rx_size > 4) {
            memcpy(u2f_nfc->payload + 6, rx_data + 4, rx_size - 4);
        }

        u2f_confirm_user_present(u2f);
        u2f_nfc->payload_len = u2f_msg_parse(u2f, u2f_nfc->payload, rx_size + 2);

        if(u2f_nfc->payload_len == 0) {
            FURI_LOG_W(TAG, "U2F parsing failed");
            break;
        }

        if(ins == U2F_APDU_SELECT) {
            if(u2f_nfc->payload_len >= 2 &&
               u2f_nfc->payload[u2f_nfc->payload_len - 2] == U2F_SW_NO_ERROR_1 &&
               u2f_nfc->payload[u2f_nfc->payload_len - 1] == U2F_SW_NO_ERROR_2) {
                u2f_nfc->applet_selected = true;
                FURI_LOG_D(TAG, "Select ok");
            } else {
                u2f_nfc->applet_selected = false;
                FURI_LOG_D(TAG, "Applet select failed");
            }
        }

        u2f_nfc_send_pending_response(u2f_nfc, iso14443_listener);
        break;
    }
    case Iso14443_4aListenerEventTypeHalted:
    case Iso14443_4aListenerEventTypeFieldOff: {
        FURI_LOG_D(TAG, "Disconnect");
        u2f_nfc_clear_pending_response(u2f_nfc);
        u2f_nfc->applet_selected = false;
        u2f_set_state(u2f, 0);

        // Update UID
        uint8_t random_uid[4] = {0x08};
        furi_hal_random_fill_buf(&random_uid[1], 3);

        Iso14443_4aData* data =
            (Iso14443_4aData*)nfc_device_get_data(u2f_nfc->nfc_device, NfcProtocolIso14443_4a);
        Iso14443_3aData* base_data = iso14443_4a_get_base_data(data);

        iso14443_4a_set_uid(data, random_uid, sizeof(random_uid));
        nfc_iso14443a_listener_set_col_res_data(
            u2f_nfc->nfc, random_uid, sizeof(random_uid), base_data->atqa, base_data->sak);
        break;
    }
    }

    return ret;
}

U2fNfc* u2f_nfc_start(U2fData* u2f_inst) {
    furi_assert(u2f_inst);
    FURI_LOG_D(TAG, "Init");

    U2fNfc* u2f_nfc = malloc(sizeof(U2fNfc));
    u2f_nfc->u2f_instance = u2f_inst;
    u2f_nfc->nfc = nfc_alloc();
    u2f_nfc->nfc_device = nfc_device_alloc();
    u2f_nfc->tx_buffer = bit_buffer_alloc(U2F_NFC_TX_BUFFER_SIZE);

    nfc_device_load(u2f_nfc->nfc_device, U2F_NFC_FILE);
    Iso14443_4aData* data =
        (Iso14443_4aData*)nfc_device_get_data(u2f_nfc->nfc_device, NfcProtocolIso14443_4a);

    // Randomize UID for Anti-Tracking (ISO14443-3 4-byte random UID starts with 0x08)
    uint8_t random_uid[4] = {0x08};
    furi_hal_random_fill_buf(&random_uid[1], 3);
    iso14443_4a_set_uid(data, random_uid, sizeof(random_uid));

    u2f_nfc->listener = nfc_listener_alloc(u2f_nfc->nfc, NfcProtocolIso14443_4a, data);
    nfc_listener_start(u2f_nfc->listener, u2f_nfc_worker_listener_callback, u2f_nfc);

    return u2f_nfc;
}

void u2f_nfc_stop(U2fNfc* u2f_nfc) {
    furi_assert(u2f_nfc);
    FURI_LOG_D(TAG, "End");
    nfc_listener_stop(u2f_nfc->listener);
    nfc_listener_free(u2f_nfc->listener);
    nfc_device_free(u2f_nfc->nfc_device);
    nfc_free(u2f_nfc->nfc);
    bit_buffer_free(u2f_nfc->tx_buffer);

    free(u2f_nfc);
}
