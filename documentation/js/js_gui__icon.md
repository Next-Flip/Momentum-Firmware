# GUI Icons {#js_gui__icon}

Retrieves and loads icons for use with GUI views such as [Dialog](#js_gui__dialog).

# Example
For an example, refer to the `gui.js` example script.

# API reference

## getBuiltin()
Gets a built-in firmware icon by its name.
All known icons bundled with firmware are supported.

**Parameters**
  - `icon`: name of the icon

**Returns**

An `IconData` object.

<br>

## loadFxbm()
Loads a `.fxbm` icon (XBM Flipper sprite, from `flipperzero-game-engine`) from file.
It will be automatically unloaded when the script exits.

**Parameters**
  - `path`: path to the `.fxbm` file

**Returns**

An `IconData` object.
