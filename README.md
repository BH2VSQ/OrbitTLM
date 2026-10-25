# OrbitTLM

OrbitTLM is a C++20 / Qt 6 / CMake / SQLite desktop workbench for **offline satellite telemetry replay and engineering analysis**.

## Data integrity rules

- Imports `decoder.log` and `SSDV_received_frames223.bin` into the **same session**.
- Correlates *identical 223-byte frames* by SHA-256; preserves raw-only records, duplicates, and provenance rather than silently marking them FEC-corrected.
- Distinguishes log capture time from spacecraft UTC: absent onboard UTC is **unknown**, not interpolated.
- Each decoded RS information frame is exactly **5 bytes of short CCSDS TM header + 218 bytes of KISS stream**. RS parity is not present in these 223 bytes.
- Variable-length subsystem packets have CRC added before KISS encoding; multiple packets can share a TM frame, or one packet can span frames.
- KISS escapes are processed across TM frame boundaries, independently per virtual channel, before any optional CCSDS Space Packet-shaped inner header is considered.
- Application CRC is not checked: this version restores KISS packets without using unverified CRC parameters to discard data.
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
OrbitTLM-cli --satellite ASRTU-1 --log decoder.log --bin SSDV_received_frames223.bin --json frames.json --packets-json kiss-packets.json --telemetry-json engineering.json --telemetry-csv engineering.csv --quaternion-csv SAT-S3-quaternions.csv --sqlite replay.sqlite
```

For BY04 use `--satellite BY04` and optional `--profile profiles/BY04.template.json`. Data fields without a confirmed schema remain raw.

Open the `OrbitTLM` executable. Use **Import session** to choose a log and binary file (either may be omitted), then choose the satellite profile and import. Exports include UTC provenance and quality labels.

This project is an independently written decoder and does not include any binary executable from third parties.


## ASRTU-1 application CRC research decision

The two reference projects do not establish the satellite application's CRC width, coverage, polynomial, trailer encoding and per-subsystem handling. gr-lilacsat's AFSK CRC implementation is not proof of the spacecraft's application checksum. A separate OBC software repository is publicly available but the precise downlink CRC call path has not been independently established.

For the current task, which is recovery and offline replay of 223-byte TM information frames, the application CRC is **not required** to preserve and export recovered variable-length KISS packets. To avoid false negatives and mislabeling corrupted packets as verified, OrbitTLM has **removed experimental CRC presets, the only-CRC-passed filter, and the checker implementation**. Frames remain retained with source and FEC evidence. Application packet status is always `not_crc_verified` until a documented satellite-specific implementation is introduced. KISS escape integrity, missing-frame discontinuities and incomplete-packet detection remain active.

## ASRTU-1 full public telemetry catalog

OrbitTLM includes **27 versioned packet kinds and 2,067 published field rows** under `protocols/asrtu1/`. For ASRTU-1 the CLI and desktop automatically load the catalog with no site requests, internet connection, or manually authored JSON schema required.

The KISS application packet is mapped to a subsystem by `main_type` and `sub_type`, then all valid `byte.MSB-bit.length` descriptors are decoded MSB-first. The converter retains field code, Chinese/English labels, source formula, unmodified HEX, raw number, converted value, unit, quality annotation and receiver time evidence. Numerical types include `UX` (unsigned), `SX` (signed), `FX` (IEEE float), `HX/X` (raw HEX), `STR` (text), enumerations, and simple numerical scaling formulae.

The **Engineering telemetry** tab shows packet history, an all-field inspector and per-field graphs. The SAT-S3 calculated quaternion export uses the four published signed-int32 fields at business offsets 12/16/20/24, scaled by 1/10000. The older experimental float candidate export remains a separate command and is explicitly unverified.

### Application packet identification

Two layouts are recognized:
- A native business packet starting directly with its `main_type,sub_type` bytes.
- A legacy KISS envelope containing `main_type` at byte 1 and `sub_type` at byte 6, with subsystem fields starting at byte 7. This interpretation is flagged as a **candidate**.

Unrecognized KISS packets remain visible and exportable without being misclassified. Published field definitions alone cannot prove application CRC validity or prove that every captured channel conveys one of these 27 packet types.

Source tables may contain malformed ranges. Invalid definitions are not silently corrected; they are reported as `invalid_source_bit_position` while the remaining fields continue decoding. Unrecognized formulae also retain raw values.
