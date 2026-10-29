#!/usr/bin/env python3
"""Validate OrbitTLM locale dictionaries, source coverage, and embedded HTML localization."""
from __future__ import annotations

import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CJK = re.compile(r"[\u3400-\u9fff\uf900-\ufaff]")
CPP_STRING = re.compile(r'"((?:[^"\\]|\\.)*)"')
CPP_RAW = re.compile(r'R"([^ ()\\\n]*)\((.*?)\)\1"', re.S)


def load_locale(name: str) -> dict:
    path = ROOT / "i18n" / name
    data = json.loads(path.read_text(encoding="utf-8"))
    for required in ("code", "displayName", "translations", "webTranslations"):
        if required not in data:
            raise SystemExit(f"{path}: missing key {required}")
    return data


def source_literals() -> set[str]:
    keys: set[str] = set()
    for path in list((ROOT / "src").glob("*.cpp")) + list((ROOT / "src").glob("*.h")):
        source = path.read_text(encoding="utf-8")
        source = CPP_RAW.sub("", source)
        for match in CPP_STRING.finditer(source):
            token = match.group(1)
            try:
                value = json.loads('"' + token + '"')
            except json.JSONDecodeError:
                value = token
            if CJK.search(value):
                keys.add(value)
    return keys


def catalog_strings() -> set[str]:
    """Return every CJK source label/value in the official telemetry catalogs."""
    found: set[str] = set()

    def visit(value) -> None:
        if isinstance(value, str):
            if CJK.search(value):
                found.add(value)
        elif isinstance(value, dict):
            for key, child in value.items():
                if isinstance(key, str) and CJK.search(key):
                    found.add(key)
                visit(child)
        elif isinstance(value, list):
            for child in value:
                visit(child)

    for path in (ROOT / "telemetry-trees").glob("*.json"):
        visit(json.loads(path.read_text(encoding="utf-8")))
    return found


def apply_dictionary(text: str, *maps: dict[str, str]) -> str:
    for dictionary in maps:
        for key in sorted(dictionary, key=len, reverse=True):
            value = dictionary[key]
            if key and value != key and CJK.search(key):
                text = text.replace(key, value)
    return text


def main() -> int:
    zh = load_locale("zh-CN.json")
    en = load_locale("en-US.json")
    zh_map, en_map = zh["translations"], en["translations"]
    zh_web, en_web = zh["webTranslations"], en["webTranslations"]

    if set(zh_map) != set(en_map):
        raise SystemExit("zh-CN/en-US translations key sets differ")
    if set(zh_web) != set(en_web):
        raise SystemExit("zh-CN/en-US webTranslations key sets differ")
    if any(key != value for key, value in zh_map.items()):
        raise SystemExit("zh-CN translations must preserve source text as values")
    if any(key != value for key, value in zh_web.items()):
        raise SystemExit("zh-CN webTranslations must preserve source snippets as values")
    untranslated = [key for key, value in en_map.items() if CJK.search(value)]
    untranslated += [key for key, value in en_web.items() if CJK.search(value)]
    if untranslated:
        raise SystemExit("English locale has untranslated CJK values: " + repr(untranslated[:10]))

    missing = sorted(source_literals() - set(zh_map))
    if missing:
        raise SystemExit("C++ CJK string literals absent from locale files: " + repr(missing[:20]))

    missing_catalog = sorted(catalog_strings() - set(zh_map))
    if missing_catalog:
        raise SystemExit("Telemetry catalog source strings absent from locale files: " + repr(missing_catalog[:20]))

    for path, content in ((ROOT / "web/index.template.html", (ROOT / "web/index.template.html").read_text(encoding="utf-8")),):
        localized = apply_dictionary(content, en_web, en_map)
        if CJK.search(localized):
            raise SystemExit(f"English localization left CJK text in {path}")

    source = (ROOT / "src/mainwindow_data.cpp").read_text(encoding="utf-8")
    match = re.search(r'R"ORBITTLM_HTML\((.*?)\)ORBITTLM_HTML"', source, re.S)
    if not match:
        raise SystemExit("Fallback HTML template not found")
    fallback = apply_dictionary(match.group(1), en_web, en_map)
    if CJK.search(fallback):
        raise SystemExit("English localization left CJK text in fallback HTML")

    cpp_sources = "\n".join(path.read_text(encoding="utf-8") for path in
                               list((ROOT / "src").glob("*.cpp")) + list((ROOT / "src").glob("*.h")))
    if "QList<QPair<QString,QString>> htmlLabels" in cpp_sources or "QList<QPair<QString, QString>> htmlLabels" in cpp_sources:
        raise SystemExit("Hardcoded HTML translation-pair table found in C++; move it into locale JSON")
    if "englishDisplay" in cpp_sources:
        raise SystemExit("Catalog English labels must be displayed through the embedded i18n dictionary")

    cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
    if "qt_add_resources(orbittlm_ui orbittlm_i18n" not in cmake:
        raise SystemExit("i18n resource embedding is not configured")
    for name in ("zh-CN.json", "en-US.json"):
        if f'"${{CMAKE_CURRENT_SOURCE_DIR}}/i18n/{name}"' not in cmake:
            raise SystemExit(f"{name} is not included in the embedded resources")

    print(f"i18n validation passed: {len(zh_map)} text keys, {len(zh_web)} web snippets, {len(source_literals())} C++ source strings, {len(catalog_strings())} telemetry catalog source strings")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
