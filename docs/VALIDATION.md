# Acceptance matrix (do not overstate completion)

| Milestone | Implemented | Validation |
|---|---|---|
| P0: read paired LOG/BIN, exact 223-byte hashing and origin fusion | yes | Qt unit tests |
| P0: 5-byte TM header + 218-byte KISS region | yes | Qt tests |
| P0: Variable-length KISS stream and per-VC cross-frame reassembly | yes | synthetic Qt regressions |
| P0: Application CRC check | configurable opt-in | mission parameters still unverified |
| P0: Inner CCSDS Space Packet | optional candidate only | not mandatory for all subsystems |
| P0: mission-specific filler and all discontinuity cases | partial | awaiting more reference fixtures |
| P0: every ASRTU-1 engineering field | schema-driven extension only | requires confirmed mission subpacket and field definitions |
| P1: Qt frame browser, playback slider, detail, raw hex, CSV/JSON | yes | CI compile; GUI manual QA pending |
| P1: reopen persisted SQLite sessions | yes | SQLite roundtrip; GUI manual QA pending |
| P2: quaternion candidate, plotted time series and 3-D wireframe preview | experimental | no physical coordinate convention established |
| P2: UTC chronological onboard telemetry output | no inferred timestamps | onboard epoch specification required |
| P3: BY04 CCSDS-level support | yes | synthetic regression |
| P3: BY04 engineering telemetry | schema interface only | verified BY04 packet layout needed |
| P4: 223-byte live TCP stream | yes | hardware / gateway tests pending |
| P4: SSDV candidate packet extraction | yes (opt-in per APID) | CRC, FEC and JPEG recovery are not implemented |
| P4: SatNOGS/MMT uploads and ZMQ transport | not implemented | future extension |

**Important:** The sample file named SSDV_received_frames223.bin is a 223-byte CCSDS transfer-frame stream, not a direct concatenation of 256-byte SSDV packets. Never classify packets as valid JPEG just because the file has SSDV in its name.

Samples without an official conversion definition remain \`schema_declared_unverified\`; candidate quaternions cannot be used to command an antenna or claim polarization accuracy.

## Reproducible local validation
\`\`\`sh
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
\`\`\`
Use a reference session with both source files, then compare SHA-256, master-frame counts, VCIDs, APIDs, sequence counts, KISS escapes and missing frames. Source timestamps are kept as *logger-local-unverified*; raw BIN frames lack timestamps.

## BG7ZDQ protocol corrections (2026-10-25)

Subsystems generate variable-length and differently typed application packets, append CRC, and wrap them using KISS. The KISS byte stream is split into 218-byte segments. A 5-byte CCSDS short TM header is prefixed, resulting in exactly 223 RS *information* bytes, which are then RS-encoded. The 223 bytes recorded by the receiver are not full parity-bearing codewords.

Short TM byte 5 is the first header pointer; it is NOT a KISS payload length. The following 218 octets are a KISS transport region, not necessarily one complete CCSDS Space Packet.

Some real KISS payloads include a six-byte CCSDS Space Packet-shaped inner header; this is OPTIONAL. Never require it to accept a TM frame or recover other subsystem packets.

Application CRC width, polynomial, coverage, reflection, init/final value and trailer endian are NOT provided. Default status is not_checked; manual CRC presets are experimental. A FEC-reported input frame is not equivalent to application CRC pass.

KISS transport per VCID supports packets of different lengths, more than one packet in a TM frame, fragmentation and escapes across TM frames. An observed virtual-channel sequence gap abandons an incomplete packet. All-0xAA segments between FEND markers are treated as probable filler but do not confirm CRC integrity.

UTC from spacecraft remains unknown if not explicitly decoded with mission epoch. No timestamps are synthesized from BIN frame ordering.

## CRC implementation follow-up (2026-10-25)

The gr-lilacsat tree has an AFSK reflected CRC-CCITT implementation. That does not establish that the ASRTU-1 spacecraft application packets use the same CRC variant. The HyacinthSat/asrtu-sndmdm-backend application processes telemetry fields and lacks independent application CRC verification. Thus OrbitTLM retains separate FEC evidence, KISS framing status, and packet-CRC status.

New explicit CRC filtering (application-packet level):
- by default, retain all well-framed packets and mark them `not_checked`;
- with a user-selected CRC profile, compute checksum using selected coverage offset and optional unprotected suffix;
- with optional `--only-crc-passed`, keep only packets that passed that explicit configured CRC, while keeping the underlying raw TM frames;
- packets interrupted by missing VC frames, malformed KISS escapes or EOF are not promoted to complete packets and are counted in diagnostics;
- an optional CCSDS Space Packet-shaped header is detected *after* successfully verified trailer removal when an experimental profile is selected.

The sample capture did not provide sufficient evidence to identify a unique spacecraft application CRC variant; common CRC16/CRC32 presets alone do not justify default filtering. Adding a verified mission profile remains an open item.
