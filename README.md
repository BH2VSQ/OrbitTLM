# OrbitTLM

OrbitTLM is a C++20 / Qt 6 / CMake / SQLite desktop workbench for **offline satellite telemetry replay and engineering analysis**.

## Data integrity rules

- Imports `decoder.log` and `SSDV_received_frames223.bin` into the **same session**.
- Correlates *identical 223-byte frames* by SHA-256; preserves raw-only records, duplicates, and provenance rather than silently marking them FEC-corrected.
- Distinguishes log capture time from spacecraft UTC: absent onboard UTC is **unknown**, not interpolated.
- KISS escape processing precedes CCSDS Space Packet length validation.
- A structurally valid packet **does not prove** it passed Reed–Solomon error correction.
- Satellite-specific values without a verified conversion schema are exported as **raw**, not invented engineering units.
- Experimental ASRTU-1 quaternion candidates are explicitly labeled `candidate_unverified`: reference frame, component order and antenna alignment are not certified.

## Target features

P0 paired LOG/BIN ingestion, CCSDS frame decoding, deduplication, SQL persistence and data-quality grading.
P1 Qt desktop: frame table, inspectors, filtering, timeline controls, export.
P2 quaternion candidates, attitude visualization, engineering time-series and ordered CSV/JSON.
P3 modular satellite profiles: ASRTU-1 and BY04 / 8104.
P4 TCP receive input and optional SSDV packet reconstruction tools.

## Build

Requires a C++20 compiler, CMake 3.21+, Qt 6.4+ components Core, Widgets, Sql, Network, Charts and Test (for tests). SQLite is accessed using Qt's QSQLITE driver.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Offline batch processing (no desktop display required):

```sh
OrbitTLM-cli --satellite ASRTU-1 --log decoder.log --bin SSDV_received_frames223.bin --json telemetry.json --csv telemetry.csv --quaternion-csv attitude.csv --sqlite replay.sqlite
```

For BY04 use `--satellite BY04` and optional `--profile profiles/BY04.template.json`. Data fields without a confirmed schema remain raw.

Open the `OrbitTLM` executable. Use **Import session** to choose a log and binary file (either may be omitted), then choose the satellite profile and import. Exports include UTC provenance and quality labels.

This project is an independently written decoder and does not include any binary executable from third parties.
