#pragma once

#include <flipper_format/flipper_format.h>

typedef struct InfraredMetadata InfraredMetadata;

/**
 * @brief Create a new InfraredMetadata instance.
 *
 * @returns pointer to the instance created.
 */
InfraredMetadata* infrared_metadata_alloc(void);

/**
 * @brief Delete an InfraredMetadata instance.
 *
 * @param[in,out] metadata pointer to the instance to be deleted.
 */
void infrared_metadata_free(InfraredMetadata* metadata);

/**
 * @brief Reset the metadata to empty values
 *
 * @param[in,out] metadata pointer to the instance
 */
void infrared_metadata_reset(InfraredMetadata* metadata);

/**
 * @brief Save metadata to a FlipperFormat file.
 *
 * @param[in] metadata pointer to the metadata instance
 * @param[in,out] ff pointer to the FlipperFormat file instance
 * @returns InfraredErrorCodeNone if metadata was successfully saved, otherwise error code
 */
InfraredErrorCode infrared_metadata_save(InfraredMetadata* metadata, FlipperFormat* ff);

/**
 * @brief Read metadata from a FlipperFormat file.
 *
 * @param[out] metadata pointer to the metadata instance
 * @param[in,out] ff pointer to the FlipperFormat file instance  
 * @returns InfraredErrorCodeNone if metadata was successfully read, otherwise error code
 */
InfraredErrorCode infrared_metadata_read(InfraredMetadata* metadata, FlipperFormat* ff);

/**
 * @brief Get the brand from metadata
 *
 * @param[in] metadata pointer to the metadata instance
 * @returns pointer to brand string
 */
const char* infrared_metadata_get_brand(const InfraredMetadata* metadata);

/**
 * @brief Get the device type from metadata
 *
 * @param[in] metadata pointer to the metadata instance
 * @returns pointer to device type string
 */
const char* infrared_metadata_get_device_type(const InfraredMetadata* metadata);

/**
 * @brief Get the model from metadata
 *
 * @param[in] metadata pointer to the metadata instance
 * @returns pointer to model string  
 */
const char* infrared_metadata_get_model(const InfraredMetadata* metadata);

/**
 * @brief Set the brand in metadata
 * 
 * @param[in,out] metadata pointer to the metadata instance
 * @param[in] brand brand string to set
 */
void infrared_metadata_set_brand(InfraredMetadata* metadata, const char* brand);

/**
 * @brief Set the device type in metadata
 *
 * @param[in,out] metadata pointer to the metadata instance
 * @param[in] device_type device type string to set
 */
void infrared_metadata_set_device_type(InfraredMetadata* metadata, const char* device_type);

/**
 * @brief Set the model in metadata
 *
 * @param[in,out] metadata pointer to the metadata instance 
 * @param[in] model model string to set
 */
void infrared_metadata_set_model(InfraredMetadata* metadata, const char* model);

/**
 * @brief Set the contributor in metadata
 *
 * @param[in,out] metadata pointer to the metadata instance
 * @param[in] contributor contributor string to set
 */
void infrared_metadata_set_contributor(InfraredMetadata* metadata, const char* contributor);

/**
 * @brief Set the remote model in metadata
 *
 * @param[in,out] metadata pointer to the metadata instance
 * @param[in] remote_model remote model string to set
 */
void infrared_metadata_set_remote_model(InfraredMetadata* metadata, const char* remote_model);

/**
 * @brief Get the contributor from metadata
 *
 * @param[in] metadata pointer to the metadata instance
 * @returns pointer to contributor string
 */
const char* infrared_metadata_get_contributor(const InfraredMetadata* metadata);

/**
 * @brief Get the remote model from metadata
 *
 * @param[in] metadata pointer to the metadata instance
 * @returns pointer to remote model string
 */
const char* infrared_metadata_get_remote_model(const InfraredMetadata* metadata);
