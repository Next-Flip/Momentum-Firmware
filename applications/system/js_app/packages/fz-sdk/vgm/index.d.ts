/**
 * Module for interactive with Flipper Video Game Module (VGM)
 * @version Available with JS feature `vgm`
 * @module
 */

/**
 * @brief Get current VGM pitch, or undefined if VGM not present
 */
export declare function getPitch(): number | undefined;

/**
 * @brief Get current VGM roll, or undefined if VGM not present
 */
export declare function getRoll(): number | undefined;

/**
 * @brief Get current VGM yaw, or undefined if VGM not present
 */
export declare function getYaw(): number | undefined;

/**
 * @brief Wait until yaw changed by specified amount
 * 
 * Returns how much the yaw changed from initial value if it exceeded the
 * specified yaw angle, or returns 0 if it was not exceeded within the timeout
 * Or returns undefined if VGM is not present
 * 
 * @param angle How much the yaw needs to change
 * @param timeout Maximum time in milliseconds to wait for specified yaw change, default 3000ms
 */
export declare function deltaYaw(angle: number, timeout?: number): number | undefined;
