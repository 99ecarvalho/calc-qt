#!/usr/bin/env python3
"""Extract the UI strings the Linux app uses from the upstream Resources.resw files.

The full files are about 14 MB for all locales. The app needs a few dozen keys,
so the build keeps only those and writes one small JSON file:
{"en-US": {"Date_Years": "years", ...}, "pt-BR": {...}, ...}
"""
import json
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

KEYS = [
    # Mode names
    "StandardModeText", "ScientificModeText", "ProgrammerModeText", "DateCalculationModeText",
    # Date calculation
    "Date_DifferenceOption.Content", "Date_AddSubtractOption.Content",
    "DateDiff_FromHeader.Header", "DateDiff_ToHeader.Header", "AddSubtract_Date_FromHeader.Header",
    "Date_DifferenceLabel.Text", "DateLabel.Text",
    "AddOption.Content", "SubtractOption.Content",
    "YearsLabel.Text", "MonthsLabel.Text", "DaysLabel.Text",
    "Date_Year", "Date_Years", "Date_Month", "Date_Months",
    "Date_Week", "Date_Weeks", "Date_Day", "Date_Days",
    "Date_SameDates", "Date_OutOfBoundMessage", "CalculationFailed",
]


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: extract-strings.py <Resources dir> <output.json>", file=sys.stderr)
        return 2
    resources, output = Path(sys.argv[1]), Path(sys.argv[2])
    wanted = set(KEYS)
    out = {}
    for resw in sorted(resources.glob("*/Resources.resw")):
        strings = {}
        for data in ET.parse(resw).getroot().iter("data"):
            name = data.get("name")
            value = data.find("value")
            if name in wanted and value is not None and value.text:
                strings[name] = value.text
        if strings:
            out[resw.parent.name] = strings
    if "en-US" not in out:
        print("extract-strings.py: en-US strings not found", file=sys.stderr)
        return 1
    missing = wanted - out["en-US"].keys()
    if missing:
        print(f"extract-strings.py: missing en-US keys: {sorted(missing)}", file=sys.stderr)
        return 1
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(out, ensure_ascii=False, sort_keys=True, separators=(",", ":")), encoding="utf-8")
    return 0


if __name__ == "__main__":
    sys.exit(main())
