# ASRTU-1 offline subsystem engineering field catalog

The source field definitions are normalized in `protocols/asrtu1/*.json`. No HTTP telemetry page is required at run time.

## Coverage

27 business types: satellite platform `SAT-S0...S9`, attitude/orbit control `ZGK-S0/S1`, power distribution `PDB-S0`, thermal controller `TCB-S0`, telemetry/telecommand `TTC-S0/S1/S2`, imaging `CAM-S0` and `CMS-S0`, data baseband `BBB-S0`, GNSS `GPS-S0`, and Y/Z star sensors `SSY-S1/S2/S3`, `SSZ-S1/S2/S3`.

Every field JSON item preserves:
```json
{"id":"SAT-C30012.7.32","bits":"0012.7.32","name_cn":"解算四元数q0",
 "formula":"F=SX/10000","name_en":"Calculated Quaternion q0"}
```

The bit address is **business-packet-relative**, not an absolute offset in the 223-byte transfer frame. For `0012.7.32`, byte 12 bit 7 is the first transmitted (most significant) bit of a 32-bit field.

## Formula handling

- `UX`: unsigned multi-bit binary integer (up to 64-bit numeric; wider fields retained as HEX)
- `SX`: signed two's complement integer, including non-byte-aligned fields
- `FX`: big-endian IEEE 754 float32/float64 where field width is 32/64
- `HX` and `X`: uninterpreted raw hexadecimal data
- `STR`: textual byte data when byte-aligned
- `F={UX|...}`: source table enumerations, including hexadecimal codes and labels
- Arithmetic `+ - * /`, decimal constants, parentheses, negative coefficients, unit suffixes and precision-marked `F.N=` expressions

No execution of source formula strings takes place; they use a closed arithmetic grammar. Ambiguous source entries are marked instead of guessed.

## Output trust model

Engineering units and numerical scaling are defined by the public field table, but this alone does **not** imply that the received packet CRC passed. Every exported record keeps the original raw field, source formula, native/legacy packet-identification evidence, receiver clock basis, and upstream FEC status. Without a confirmed onboard time epoch, onboard UTC is **null**.

`SAT-S3` quaternion export does not assign an inertial or terrestrial reference frame and must not be interpreted as validated spacecraft orientation or ground antenna polarization.

## Reproducible use

```powershell
.\OrbitTLM-cli.exe --satellite ASRTU-1 --log decoder.log --bin SSDV_received_frames223.bin \
  --telemetry-json engineering.json --telemetry-csv engineering.csv \
  --quaternion-csv sat-s3.csv --packets-json kiss-packets.json
```

Satellite `BY04` deliberately does not reuse ASRTU-1 business offsets. It has its own profile extension point.
