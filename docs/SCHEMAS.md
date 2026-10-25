# Satellite engineering schemas

Satellite application profiles are **data**, separate from the receiver framing layer.

The decoder accepts a JSON object containing \`satellite\`, \`revision\` and an array of \`fields\`.

Every field has:
- \`name\`: unique signal name such as \`eps.bus_voltage\`
- \`apid\`: CCSDS application ID as integer or \`"0x0F0"\`
- \`offset\`: **byte offset inside Space Packet data, after the 6-byte CCSDS header**
- \`type\`: \`u8\`, \`i8\`, \`u16be\`, \`i16be\`, \`u32be\`, \`i32be\`, \`f32be\`, \`bit\`
- \`scale\` and \`bias\`: \`engineering_value = raw * scale + bias\`
- \`unit\`, \`enums\`, \`status\`: optional metadata
- \`condition\`: optional \`{"offset":8,"hex":"F000"}\` payload prefix match
- \`bit\`: required for type \`bit\`, LSB = bit 0

Example:
\`\`\`json
{"satellite":"BY04","revision":"experimental-01","fields":[
  {"name":"test.raw_counter","apid":"0x123","offset":4,"type":"u16be","scale":1,"status":"schema_declared_unverified"}
]}
\`\`\`

Unsupported / unverified satellite payloads remain raw. A schema cannot turn a structurally valid BIN record into an RS-verified record. Mission-specific subpacket demultiplexing and time-epoch definitions must be verified before assigning definitive engineering units or UTC.

Do not assume \`candidate.quaternion.q0\` etc is a body-to-inertial quaternion, or use it to steer an antenna.
