# BY04 / 8104 / BY70-4: verified transport, separate mission fields

## Verified public protocol facts

- The published receiver uses BY04 transfer-frame header `0x2052` for its VC1 image service (spacecraft ID `0x205`; VCID 1). Its ASRTU-1 counterpart is `0x0322`.
- A decoded TM information frame is 223 bytes, comprising 5-byte short CCSDS-compatible transfer-frame header and 218-byte data region.
- BY04 VC1 carries **native DSLWP SSDV 218-byte image packets**, not KISS stream data. The receiver uses `ssdv_dslwp_mode`, and the first 9 bytes encode image ID, packet index, width, height, quality/mode, and MCU data. The final 4 bytes are a **DSLWP image-specific** CRC32 using initial value `0x4EE4FDE1` and polynomial `0xEDB88320`, with big-endian stored checksum.
- BY04 VC0/VC2 telemetry continues the KISS byte-stream path. Inner CCSDS Space Packet headers may be recognized when length/format checks pass. APID is an **application process ID**, not a BY04 subsystem engineering telemetry field code.
- BG7ZDQ explicitly said BY04 and ASRTU-1 have **different spacecraft IDs and engineering field layouts**. Therefore ASRTU-1's published SAT-S3 quaternion offsets are NOT valid for BY04.

## Current OrbitTLM decoder

- Parse BY04 223-byte TM information frame, add `tm.spacecraft_id` metadata.
- Filter `BY04` KISS decoder by spacecraft ID `0x205` and exclude VC1 image frames.
- From valid KISS/Space Packet envelope, show APID, 14-bit sequence count, packet sequence flags and declared data length; keep full raw payload and evidence of FEC and unknown application CRC.
- Show these as `CCSDS 包头已解析；工程字段待确认`, never as unrecognized all-zero engineering telemetry.
- For images, scan VC1 frame payload as 218-byte DSLWP SSDV, deduplicate and sort by image/packet ID, validate the **image format-specific** CRC, and preserve failed packets as candidates for offline examination.
- CLI `--satellite BY04 --telemetry-json ... --telemetry-csv ...` exports transport-level evidence, and `--by04-ssdv-bin ...` saves VC1 native image packets.

## Known remaining limitation

No verifiable BY04 business field dictionary (engineering voltage, thermal, quaternion, GPS orbital elements) was found in the cited receiver or ASRTU-1 backend. We do not invent these conversions. `BY04-CCSDS` is a protocol header decode, NOT a completed physical telemetry parser. Official BY04 subsystem tables or decoded sample values would be necessary for full engineering parameters.

## Sources

- https://github.com/BG7ZDQ/ASRTU_Series_Receiver/blob/main/libs/demod/ssdv_receiver.cpp
- https://github.com/BG7ZDQ/ASRTU_Series_Receiver/blob/main/third_party/ssdv_dslwp/ssdv.c
- https://community.libre.space/t/cz-6c-launches-jamx01-by70-4-2026-08-25-03-30utc/15173?page=10
- https://db.satnogs.org/satellite/EMBU-7314-7883-5821-1365/
