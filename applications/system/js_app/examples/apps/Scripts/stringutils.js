let sampleText = "Hello, World!";

let lengthOfText = "Length of text: " + to_string(StringUtils.GetLength(sampleText));
print(lengthOfText);

let start = 7;
let end = 12;
let substringResult = StringUtils.substring(sampleText, start, end);
print(substringResult);

let searchStr = "World";
let result2 = to_string(StringUtils.indexOf(sampleText, searchStr));
print(result2);

let upperCaseText = "Text in upper case: " + StringUtils.toUpperCase(sampleText);
print(upperCaseText);

let lowerCaseText = "Text in lower case: " + StringUtils.toLowerCase(sampleText);
print(lowerCaseText);
