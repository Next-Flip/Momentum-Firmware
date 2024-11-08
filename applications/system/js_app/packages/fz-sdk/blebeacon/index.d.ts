/**
 * Module for using the BLE extra beacon
 * @version Available with JS feature `blebeacon`
 * @module
 */

/**
 * @brief Check if the BLE beacon is active
 */
export declare function isActive(): boolean;

/**
 * @brief Set BLE beacon configuration
 * @param mac The MAC address to use
 * @param power The power level to use, in GapAdvPowerLevel scale: 0x00 (-40dBm) to 0x1F (+6dBm)
 * @param minInterval Minimum advertisement interval
 * @param maxInterval Maximum advertisement interval
 */
export declare function setConfig(mac: Uint8Array, power?: number, minInterval?: number, maxInterval?: number): void;

/**
 * @brief Set BLE beacon advertisement data
 * @param data The advertisement data to use
 */
export declare function setData(data: Uint8Array): void;

/**
 * @brief Start BLE beacon
 */
export declare function start(): void;

/**
 * @brief Stop BLE beacon
 */
export declare function stop(): void;

/**
 * @brief Set whether the BLE beacon will remain active after the script exits
 * @param keep True if BLE beacon should remain active after script exit
 */
export declare function keepAlive(keep: boolean): void;
