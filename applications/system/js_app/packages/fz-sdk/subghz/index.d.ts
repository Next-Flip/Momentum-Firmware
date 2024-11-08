/**
 * Module for using Sub-GHz transciever
 * @version Available with JS feature `subghz`
 * @module
 */

/**
 * @brief Initialize Sub-GHz module
 */
export declare function setup(): void;

/**
 * @brief Deinitialize Sub-GHz module
 */
export declare function end(): void;

/**
 * @brief Set radio to receive mode
 */
export declare function setRx(): void;

/**
 * @brief Set radio to idle mode
 */
export declare function setIdle(): void;

/**
 * @brief Return current RSSI on current frequency, or undefined if radio is not in receive mode
 */
export declare function getRssi(): number | undefined;

type RadioState = "RX" | "TX" | "IDLE" | "";

/**
 * @brief Get current radio mode/state
 */
export declare function getState(): RadioState;

/**
 * @brief Get currently selected frequency
 */
export declare function getFrequency(): number;

/**
 * @brief Change current frequency, radio must be in idle mode
 * 
 * Returns the effective frequency, since radio module cant use all precise
 * values and instead chooses closest one available
 * 
 * @param frequency What frequency to use
 */
export declare function setFrequency(frequency: number): number;

/**
 * @brief Check whether the radio module in use is internal or external
 */
export declare function isExternal(): boolean;

/**
 * @brief Transmit a .sub file, return true on success or error on failure
 * @param path What .sub file to transmit
 * @param repeat How many times to repeat the signal
 */
export declare function transmitFile(path: string, repeat?: number): true;
