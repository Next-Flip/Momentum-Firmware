#include "namespoof.h"

#include <flipper_format/flipper_format.h>
#include <furi_hal_version.h>
#include <furi_hal_random.h>

#define TAG "NameSpoof"

const char* const cat_names[] = {
    "Manx",     "York",     "Dwelf",    "Korat",    "Lykoi",    "Asian",    "Devon",    "Aegean",
    "Bengal",   "Birman",   "Bombay",   "Cymric",   "Cyprus",   "LaPerm",   "Ocicat",   "Sokoke",
    "Somali",   "Sphynx",   "Toyger",   "Havana",   "Angora",   "Levkoy",   "Bambino",  "Burmese",
    "Chausie",  "Cheetoh",  "Donskoy",  "Elf cat",  "Minskin",  "Persian",  "Ragdoll",  "Siamese",
    "Arabian",  "Cornish",  "Selkirk",  "Turkish",  "Bobtail",  "Balinese", "Burmilla", "Javanese",
    "Munchkin", "Nebelung", "Oriental", "Savannah", "Siberian", "Snowshoe", "Thai cat", "Pixiebob",
    "Egyptian", "Napoleon", "Scottish"};

void namespoof_init(void) {
    FuriString* str = furi_string_alloc();
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* file = flipper_format_file_alloc(storage);

    const char* prev_custom_name = version_get_custom_name(NULL);
    bool applied_new_name = false;

    do {
        uint32_t version;
        if(!flipper_format_file_open_existing(file, NAMESPOOF_PATH)) break;
        if(!flipper_format_read_header(file, str, &version)) break;
        if(furi_string_cmp_str(str, NAMESPOOF_HEADER)) break;
        if(version != NAMESPOOF_VERSION) break;

        if(!flipper_format_read_string(file, "Name", str)) break;
        version_set_custom_name(NULL, strdup(furi_string_get_cstr(str)));
        furi_hal_version_set_name(NULL);
        applied_new_name = true;
    } while(false);

    if(!applied_new_name) {
        //Custom name is not set, so make a random one
        // furi_hal_random_init();
        uint32_t lucky_cat = furi_hal_random_get() % COUNT_OF(cat_names);
        version_set_custom_name(NULL, cat_names[lucky_cat]);
        furi_hal_version_set_name(version_get_custom_name(NULL));
        applied_new_name = true;
    }

    if(prev_custom_name) {
        if(!applied_new_name) {
            version_set_custom_name(NULL, NULL);
            furi_hal_version_init();
        }
        free((void*)prev_custom_name);
    }

    flipper_format_free(file);
    furi_record_close(RECORD_STORAGE);
    furi_string_free(str);
}
