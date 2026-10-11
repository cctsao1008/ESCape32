# Compile/link fixture only. PB0 is not an assigned product parking input.
# NEVER FLASH: electrical routing and polarity have not been qualified.
set(AM13E_BOARD_BOARD_DEFINITIONS
    PARK_PIN=1
    AM13E_BOARD_PARK_GPIO=GPIO1
    AM13E_BOARD_PARK_MASK=1U
    AM13E_BOARD_PARK_PINCM=32U)
