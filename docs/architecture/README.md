# E61 ESCape32 v1.6 Architecture Artifacts

This directory contains Archify JSON IR that presents the E61 ESCape32 v1.6 firmware from three complementary views:

- `e61-v1.6.architecture.json` — runtime architecture and hardware/software signal paths.
- `e61-v1.6.startup-lifecycle.json` — motor startup, BEMF synchronization, and stop/desync lifecycle.
- `e61-v1.6.dshot-sequence.json` — DShot command-to-commutation sequence.

## Authority and provenance

These artifacts were derived from the supplied `e61-escape32-e61-v1.6.zip` source archive. The archive identifies its source snapshot as `607106a1b03dbeecef6da302a74f3d1ad34380fd` and the firmware itself declares `REVISION 16`, `REVPATCH 0`.

The current GitHub `master` / `archify-v1.6` branch ancestry is newer than that supplied v1.6 archive. Therefore these JSON files intentionally describe **E61 v1.6**, not the current upstream master state.

Repository documentation rule:

> README explains the system. Issues explain the journey. Code proves the current state.

For these visualization artifacts:

> Archify JSON describes the reviewed structure. Generated HTML presents it. Source code remains authoritative.

## E61 v1.6 board defaults represented here

From `CMakeLists.txt` E61 target:

- `SINE_RANGE=0`
- `DUTY_MIN=14`
- `DUTY_SPUP=20`
- `DUTY_RATE=15`
- `DUTY_RAMP=70`
- `THROT_ZTC=1`
- `DEAD_TIME=155`
- `COMP_MAP=123`
- `E61_COMP_BEMF`
- `STSPIN32G4_DRIVER`
- `IO_PA6`

## Generated output

Rendered HTML belongs under `docs/architecture/generated/` and is treated as generated output. The JSON IR in this directory is the version-controlled visualization source.

Example render commands from an Archify checkout:

```bash
node bin/archify.mjs deliver architecture \
  /path/to/ESCape32/docs/architecture/e61-v1.6.architecture.json \
  /path/to/ESCape32/docs/architecture/generated/E61_ESCape32_v1.6_System_Map.html \
  --quality showcase --json

node bin/archify.mjs deliver lifecycle \
  /path/to/ESCape32/docs/architecture/e61-v1.6.startup-lifecycle.json \
  /path/to/ESCape32/docs/architecture/generated/E61_ESCape32_v1.6_Startup_Lifecycle.html \
  --quality showcase --json

node bin/archify.mjs deliver sequence \
  /path/to/ESCape32/docs/architecture/e61-v1.6.dshot-sequence.json \
  /path/to/ESCape32/docs/architecture/generated/E61_ESCape32_v1.6_DShot_Sequence.html \
  --quality showcase --json
```

Do not hand-edit generated HTML as the canonical architecture source. Update the JSON IR, validate it, and regenerate the HTML.
