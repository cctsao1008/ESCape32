add_target(BOOT1_PA2 STM32F0 IO_PA2)
add_target(BOOT1_PB4 STM32F0)
add_target(BOOT2_PA2 STM32G0 IO_PA2)
add_target(BOOT2_PA6 STM32G0 IO_PA6)
add_target(BOOT2_PB4 STM32G0)
add_target(BOOT3_PA2 STM32F0 AT32F4 IO_PA2)
add_target(BOOT3_PA2_FE STM32F0 AT32F4 IO_PA2 IO_RXTX FAST_EXIT)
add_target(BOOT4_PA2 STM32G4)

# E62 bootloader integration point.
# The AM13E bootloader sources and linker configuration are not ported yet.
# INTERFACE target only: consumers inherit TI_AM13E, but no boot image is
# compiled or advertised as ready.
add_library(BOOT_E62 INTERFACE)
target_compile_definitions(BOOT_E62 INTERFACE TI_AM13E)
