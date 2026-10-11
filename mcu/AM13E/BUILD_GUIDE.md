# AM13E current architecture and build

Current authority: DESIGN_AUTHORITY.md and the immutable v1.6 originals. REV16_REQUIREMENTS.csv is the 90-ID status/crosswalk, not an acceptance claim.

Original algorithms/parsers/policies remain in src/ and boot/src/. Native config.c/config.h/config.cmake/config.ld stay at each MCU root. Application preset.c retains original sinedata[]. Pure protocol codecs, planners, address translation and staging policy remain outside HAL. DriverLib/register/IRQ implementation lives in each image's own hal/ directory; config.c retains native initialization hooks. No Boot HAL object is linked into Application, or vice versa. Common newlib support is separately compiled into each image.

Use official Arm 15.2.Rel1/GCC15.2.1 and official TI26.01.00.03.STS. Configure CMake with AM13E_SDK_ROOT, CMAKE_C_STANDARD=23 and AM13E_BOOT_LINKER_SCRIPT pointing to boot/mcu/AM13E/config.ld. Build targets AM13E_FW1_REL17_IMAGE and BOOT5_PB14.elf. Host CMake source boot/tests/am13e_host requires AM13E_HOST_PACKED_IMAGE pointing to the actual flat APP binary; run all11 CTests. Native selected-mode regressions and conditional ARM guards are in the AM13E CI workflow.

Config-prefix wire blocks0–3 map4000–4fff; APP blocks4–491 map6000–7ffff. Invalid492–511 and Reserved5000–5fff are rejected. APP-only programming plans start at4. Combined streams require explicit Config input/authorization. Capacity is not image length; original marker/vector does not prove transfer completeness or brownout safety.

Reference pins/clock/gate values are unqualified, not a product schematic. Physical motor/Flash, Boot Bank0 self-update and NONMAIN WRP must remain disabled/unqualified. Existing recovery negative guards are mandatory. Latched static protection currently rejects any protected bank conservatively; exact reversible Boot-only WRP permissions need TI/silicon resolution.

Conditional discrete LEDs now use native open-drain configuration/readback as selected by original LED_OD/LEDn_OD. Conditional BEC uses original cfg.bec-BEC_MIN source calculation and explicit native GPIO bits. No pins, electrical truth table or SWD override are inferred; unsupported debug-pin BEC remains rejected.

REL17_V14 remains the backward-compatible CMake token for the flat APP physical image profile; current wire addressing is Rev1.6 Config-prefix. Historical manifest-based experiments bearing a v1.6 label are unrelated to the newly adopted originals.

SBUS2 slot scheduling and soft-UART timing remain incomplete. AM13E23019 SDK exposes TIMG4 (BiDShot TX) and TIMG12 (motor/audio), and ECAP0/1 (receiver/BEMF). A native role-arbitration implementation or another validated timing peripheral is required; no fictitious spare timer or global motor-clock enable is permitted. Board serial electrical topology, Hall, clock trim and gate-driver data remain essential inputs.

Shared native verification: `bash mcu/AM13E/tools/run_host_regressions.sh fast` selects core production/safety groups; `full` runs every existing native group and is used by CI. These commands complement the 11 Boot CTests, strict ARM links and specialized conditional/safety gates; neither mode claims complete milestone coverage. Compiler identity is printed.
