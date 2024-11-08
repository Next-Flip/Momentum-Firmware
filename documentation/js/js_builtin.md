# Built-in methods {#js_builtin}

## require
Load a module plugin.

### Parameters
- Module name

### Examples:
```js
let serial = require("serial"); // Load "serial" module
```

## delay
### Parameters
- Delay value in ms

### Examples:
```js
delay(500); // Delay for 500ms
```
## print
Print a message on a screen console.

### Parameters
The following argument types are supported:
- String
- Number
- Bool
- undefined

### Examples:
```js
print("string1", "string2", 123);
```

## console.log
## console.warn
## console.error
## console.debug
Same as `print`, but output to serial console only, with corresponding log level.

## toString
Convert a number to string with an optional base.

### Examples:
```js
toString(123) // "123"
toString(123, 16) // "0x7b"
```

## parseInt
Converts a string to a number.

### Examples:
```js
parseInt("123") // 123
```

## toUpperCase
Transforms a string to upper case.

### Examples:
```js
toUpperCase("Example") // "EXAMPLE"
```

## toLowerCase
Transforms a string to lower case.

### Examples:
```js
toLowerCase("Example") // "example"
```

## __dirname
Path to the directory containing the current script.

### Examples:
```js
print(__dirname); // /ext/apps/Scripts/Examples
```

## __filename
Path to the current script file.

### Examples:
```js
print(__filename); // /ext/apps/Scripts/Examples/path.js
```
