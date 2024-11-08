/**
 * Module for USB mass storage emulation
 * @version Available with JS feature `usbdisk`
 * @module
 */

/**
 * @brief Create a disk image and format it as FatFs, error on failure
 * @param path Where to create the disk image
 * @param size How big the disk image should be
 * @version Available with JS feature `usbdisk-createimage`
 */
export declare function createImage(path: string, size: number): void;

/**
 * @brief Start emulating mass storage device
 * @param path The disk image to emulate
 */
export declare function start(path: string): void;

/**
 * @brief Stop emulating mass storage device
 */
export declare function stop(): void;

/**
 * @brief Check if the mass storage device was exected
 * 
 * Useful as a loop condition with a delay, so UsbDisk keeps running until ejected
 * 
 */
export declare function wasEjected(): boolean;
