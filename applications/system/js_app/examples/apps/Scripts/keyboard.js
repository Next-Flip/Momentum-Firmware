let keyboard = require("keyboard");

keyboard.setHeader("Example Text Input");

// Default text is optional
let text = keyboard.text(100, "Default text", true);
print("Got text:", text);