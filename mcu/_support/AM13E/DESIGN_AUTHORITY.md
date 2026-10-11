# AM13E current design authority — Rev1.6

Adopted directly from the user-provided ESCape32_AM13E_Integration_Design_v1.6.zip.
The three byte-identical originals in v1.6/ govern current AM13E integration.
Precedence: original ESCape32 Rel17 source (1d718c143380c3eb7e6581478990e1496632e5b8) for functional and externally observable behavior; Rev1.6 for integration architecture and target contracts; official TI SDK/TRM for hardware facts; previous revisions and audits as historical evidence.

Superseded Rev1.6 documents and crosswalks are retired from the active tree; Git history preserves them. No Rev1.5 originals were present or invented. REV16_REQUIREMENTS.csv tracks only current requirements.

Rev1.6 is distinct from the previously quarantined manifest-based image architecture with the same revision number. The current originals require Cfg.id=0x32EA and a flat APP vector at 0x6000, not an image manifest or Signature-last launch rule.

Current contract: logical blocks 0–3 CFG (0x4000–0x4fff); 4–491 APP (0x6000–0x7ffff); 492–511 invalid; Reserved never mapped. APP-only uploads must skip CFG; combined streams require explicit Config programming authorization in the host packager. Physical partitions and original command framing remain.

Application hardware belongs to mcu/AM13E/hal/; Boot hardware belongs to boot/mcu/AM13E/hal/. Algorithms/parsers/policies remain outside HAL. Top-level config files and preset.c/sinedata[] remain native entry/data contracts. The unified add_target(name mcu ...) selects each native config.cmake; private TI support provides SDK/ABI/source configuration.

Full source/runtime parity remains pending. Host tests and strict ARM links do not establish hardware qualification. Motor drive, physical Flash, Boot self-update and NONMAIN WRP remain unqualified; existing safety gates remain mandatory.

SHA-256:
- Integration_Design.md: 6ebb98767056eb5b010f41f7644263a272fbfd0f8b33a045be97cfc1c884400c
- Integration_Mapping.md: c68dbeb0ee0625da06f1ad7950ebabd81d9e6b70591ceb99f29622edcd3811e1
- Interface_Contracts.md: 70776e87fb712a528360974fc15fd1c5753516c550f5387c4a2a9dc833aff633
