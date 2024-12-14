#!/usr/bin/env python3
import pathlib
import string

KEY_LENGTH = 12

file = (
    pathlib.Path(__file__).parent
    / "../applications/main/nfc/resources/nfc/assets/mf_classic_dict.nfc"
)
lines = file.read_text().split("\n")

total = 0
for i, line in reversed(list(enumerate(lines))):
    if line.startswith("#"):
        continue
    if not line:
        continue
    key = line[:KEY_LENGTH]
    if len(key) != KEY_LENGTH or any(char not in string.hexdigits for char in key):
        print(f"Line {i + 1} is not correct: {line}")
    for check in lines[:i]:
        if check.upper()[:KEY_LENGTH] == line.upper()[:KEY_LENGTH]:
            print(f"Line {i + 1} is a duplicate: {line}")
            del lines[i]
            break
    else:  # Didn't break
        total += 1


file.write_text("\n".join(lines))
print(f"Total keys: {total}")
