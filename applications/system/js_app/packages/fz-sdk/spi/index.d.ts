/**
 * SPI bus communication
 * @version Available with JS feature `spi`
 * @module
 */

/**
 * @brief Acquire SPI bus
 */
export declare function acquire(): void;

/**
 * @brief Release SPI bus
 */
export declare function release(): void;

/**
 * @brief Write data to SPI bus and return success status
 * @param data The data to write
 * @param timeout Timeout in milliseconds
 */
export declare function write(data: number[] | ArrayBuffer, timeout?: number): boolean;

/**
 * @brief Read data from SPI bus or return undefined on failure
 * @param length How many bytes to read
 * @param timeout Timeout in milliseconds
 */
export declare function read(length: number, timeout?: number): ArrayBuffer | undefined;

/**
 * @brief Write and read data on SPI bus or return undefined on failure
 * @param data The data to write, its length also indicates how many bytes will be read
 * @param timeout Timeout in milliseconds
 */
export declare function writeRead(data: number[] | ArrayBuffer, timeout?: number): ArrayBuffer | undefined;
