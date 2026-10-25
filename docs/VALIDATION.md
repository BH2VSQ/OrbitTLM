# Acceptance matrix (do not overstate completion)

| Milestone | Implemented | Validation |
|---|---|---|
| P0: read paired LOG/BIN, exact 223-byte hashing and origin fusion | yes | Qt unit tests |
| P0: KISS escape + single complete Space Packet | yes | Qt unit tests |
| P0: multi-frame short-KISS continuation, arbitrary VC transport | no | pending reference fixtures |
| P0: every ASRTU-1 engineering field | schema-driven extension only | requires confirmed mission subpacket and field definitions |
| P1: Qt frame browser, playback slider, detail, raw hex, CSV/JSON | yes | CI compile; GUI manual QA pending |
| P1: reopen persisted SQLite sessions | stored, re-open UI pending | not yet verified |
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
