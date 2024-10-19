# js_gui__byte_input {#js_gui__byte_input}

# Byte input GUI view
Displays a keyboard.

<img src="byte_input.png" width="200" alt="Sample screenshot of the view" />

```js
let eventLoop = require("event_loop");
let gui = require("gui");
let byteInputView = require("gui/byte_input");
```

This module depends on the `gui` module, which in turn depends on the
`event_loop` module, so they _must_ be imported in this order. It is also
recommended to conceptualize these modules first before using this one.

# Example
For an example refer to the `gui.js` example script.

# View props
## `length`
Data buffer length

Type: `number`

## `header`
Single line of text that appears above the keyboard

Type: `string`

## `defaultData`
Data to show in byte input by default

Type: `Uint8Array | ArrayBuffer`

# View events
## `input`
Fires when the user selects the "save" button.

Item type: `ArrayBuffer`
