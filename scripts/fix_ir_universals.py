#!/usr/bin/env python3
import pathlib

universals = (
    pathlib.Path(__file__).parent
    / "../applications/main/infrared/resources/infrared/assets"
)

for universal in universals.glob("*.ir"):
    text = universal.read_text()
    lines = text.splitlines()
    signal = []
    comment = []
    signals = []
    for line in lines:
        if line.startswith("#"):
            comment.append(line)
            continue
        signal.append(line)
        if line.startswith(("data: ", "command: ")):  # Last line of this signal
            signals.append(("\n".join(signal), "\n".join(comment)))
            signal.clear()
            comment.clear()
    found = dict()
    for signal, comment in signals:
        if signal in found:
            if (
                universal.stem == "projectors"
                and found[signal] == 1
                and signal.startswith("name: Power")
            ):
                # Projectors need double press of power to confirm shutdown, so 1 dupe is fine
                found[signal] += 1
                continue
            replace = f"\n{comment}\n{signal}"
            pos = text.rfind(replace)
            text = text[:pos] + text[pos + len(replace) :]
            continue
        found[signal] = 1
    universal.write_text(text)
