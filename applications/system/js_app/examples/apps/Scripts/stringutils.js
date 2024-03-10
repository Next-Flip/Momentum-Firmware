let sampleText = "Hello, World!";

let lengthOfText = "Length of text: " + to_string(GetLength(sampleText));
print(lengthOfText);

let start = 7;
let end = 12;
let substringResult = substring(sampleText, start, end);
print(substringResult);

let searchStr = "World";
let result2 = to_string(indexOf(sampleText, searchStr));
print(result2);

let upperCaseText = "Text in upper case: " + toUpperCase(sampleText);
print(upperCaseText);

let lowerCaseText = "Text in lower case: " + toLowerCase(sampleText);
print(lowerCaseText);
