# AM13E current architecture and build

Current authority: DESIGN_AUTHORITY.md and the immutable v1.6 originals. REV16_REQUIREMENTS.csv is the 90-ID status/crosswalk, not an acceptance claim.

Original algorithms/parsers/policies remain in src/ and boot/src/. Native config.c/config.h/config.cmake/config.ld stay at each MCU root. Application preset.c retains original sinedata[]. Pure protocol codecs, planners, address translation and staging policy remain outside HAL. DriverLib/register/IRQ implementation lives in each image's own hal/ directory; config.c retains native initialization hooks. No Boot HAL object is linked into Application, or vice versa. Common newlib support is separately compiled into each image.

Use official Arm 15.2.Rel1/GCC15.2.1 and official TI26.01.00.03.STS. Configure CMake with AM13E_SDK_ROOT, CMAKE_C_STANDARD=23 and AM13E_BOOT_LINKER_SCRIPT pointing to boot/mcu/AM13E/config.ld. Build targets AM13E_FW1_REL17_IMAGE and BOOT5_PB14.elf. Host CMake source boot/tests/am13e_host requires AM13E_HOST_PACKED_IMAGE pointing to the actual flat APP binary; run all11 CTests. Native selected-mode regressions and conditional ARM guards are in the AM13E CI workflow.

Config-prefix wire blocks0–3 map4000–4fff; APP blocks4–491 map6000–7ffff. Invalid492–511 and Reserved5000–5fff are rejected. APP-only programming plans start at4. Combined streams require explicit Config input/authorization. Capacity is not image length; original marker/vector does not prove transfer completeness or brownout safety.

Reference pins/clock/gate values are unqualified, not a product schematic. Physical motor/Flash, Boot Bank0 self-update and NONMAIN WRP must remain disabled/unqualified. Existing recovery negative guards are mandatory. Latched static protection currently rejects any protected bank conservatively; exact reversible Boot-only WRP permissions need TI/silicon resolution.
