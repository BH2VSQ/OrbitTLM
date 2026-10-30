# Locale maintenance

`zh-CN.json` contains the canonical source keys. `en-US.json` and `ja-JP.json`
must contain exactly the same keys in `translations` and `webTranslations`.
Edit complete Japanese phrases directly; do not translate through the generated
English labels or use repeated substring replacement on translated Japanese.
The application substitutes source spans once and preserves the original text
for switching back to another language.

Run `python tools/validate_i18n.py`. CTest and Linux/Windows CI also run this
validator. New keys produced by `generate_catalog_i18n.py` need Japanese entries
before validation passes; that tool maintains Chinese/English and never
overwrites manually reviewed Japanese text.

Preserve `%1`, `%2`, `%v`, `%m`, `${...}`, identifiers, invalid-value sentinels,
units and formulas. Web entries may be HTML or JavaScript fragments; changing
visible labels must preserve their syntax. Do not reject Japanese merely for
containing kanji. `温度`, `未解析` and `GPS位置X`, for example, are valid Japanese.

Japanese conventions: テレメトリ, センサ, バッテリ, パラメータ, 生データ値,
工学値, クォータニオン, 電力分配, バス電圧, 磁気トルカ and 磁気モーメント.
The main engineering tab uses 機器テレメトリ; HK is reserved for spacecraft
housekeeping data. Packet reassembly is パケット再組立て; image reconstruction
is 画像再構成. Unchecked CRC is 未検証, not a CRC failure.

Cold-propulsion shorthand and project names such as 冯相机, 时空表, DBW,
增阻球 and 离轨球 retain their source labels until their exact meaning/name is
confirmed. 快包/慢包 retain the packet family labels without inventing a bitrate
or transmission interval. Packet codes and field IDs always remain unchanged.

Terminology references:
- [JAXA spacecraft information base: raw values and engineering values](https://sma.jaxa.jp/TechDoc/Docs/JAXA-JERG-2-700-TP004.pdf)
- [JAXA: housekeeping and mission data](https://www.isas.jaxa.jp/feature/terrestrial/04.html)
- [JAXA power-system design standard](https://sma.jaxa.jp/TechDoc/Docs/JAXA-JERG-2-214A.pdf)
