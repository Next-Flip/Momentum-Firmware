let keyboard = require("keyboard");

// Default text is optional
let text = keyboard.text(100, "Some default text", true);

print("Got text:", text);