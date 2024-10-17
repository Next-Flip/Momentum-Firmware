// Connect a w25q32 SPI device to the Flipper Zero.
// D1=pin 2 (MOSI), SLK=pin 5 (SCK), GND=pin 8 (GND), D0=pin 3 (MISO), CS=pin 4 (CS), VCC=pin 9 (3V3)
let spi = require("spi");

// Display textbox so user can scroll to see all output.
let textbox = require("textbox");
textbox.setConfig("end", "text");
textbox.addText("SPI demo\n");
textbox.show();

// writeRead returns a buffer the same length as the input buffer.
// We send 6 bytes of data, starting with 0x90, which is the command to read the manufacturer ID.
// Can also use Uint8Array([0x90, 0x00, ...]) as write parameter
// Optional timeout parameter in ms. We set to 100ms.
let data_buf = spi.writeRead([0x90, 0x0, 0x0, 0x0, 0x0, 0x0], 100);
let data = Uint8Array(data_buf);
if (data.length === 6) {
    if (data[4] === 0xEF) {
        textbox.addText("Found Winbond device\n");
        if (data[5] === 0x15) {
            textbox.addText("Device ID: W25Q32\n");
        } else {
            textbox.addText("Unknown device ID: " + to_hex_string(data[5]) + "\n");
        }
    } else if (data[4] === 0x0) {
        textbox.addText("Be sure Winbond W25Q32 is connected to Flipper Zero SPI pins.\n");
    } else {
        textbox.addText("Unknown device. Manufacturer ID: " + to_hex_string(data[4]) + "\n");
    }
}

textbox.addText("\nReading JEDEC ID\n");

// Acquire the SPI bus. Multiple calls will happen with Chip Select (CS) held low.
spi.acquire();

// Send command (0x9F) to read JEDEC ID.
// Can also use Uint8Array([0x9F]) as write parameter
// Note: you can pass an optional timeout parameter in milliseconds.
spi.write([0x9F]);

// Request 3 bytes of data.
// Note: you can pass an optional timeout parameter in milliseconds.
data_buf = spi.read(3);

// Release the SPI bus as soon as we are done with the set of SPI commands.
spi.release();

data = Uint8Array(data_buf);
textbox.addText("JEDEC MF ID: " + to_hex_string(data[0]) + "\n");
textbox.addText("JEDEC Memory Type: " + to_hex_string(data[1]) + "\n");
textbox.addText("JEDEC Capacity ID: " + to_hex_string(data[2]) + "\n");

if (data[0] === 0xEF) {
    textbox.addText("Found Winbond device\n");
}
let capacity = data[1] << 8 | data[2];
if (capacity === 0x4016) {
    textbox.addText("Device: W25Q32\n");
} else if (capacity === 0x4015) {
    textbox.addText("Device: W25Q16\n");
} else if (capacity === 0x4014) {
    textbox.addText("Device: W25Q80\n");
} else {
    textbox.addText("Unknown device\n");
}

// Wait for user to close the app
while (textbox.isOpen()) {
    delay(250);
}
