# Compile/link fixture only. PB1 is not an assigned product ERPM output.
# NEVER FLASH: electrical routing has not been qualified.
set(AM13E_BOARD_BOARD_DEFINITIONS
    ERPM_PIN=1
    AM13E_BOARD_ERPM_GPIO=GPIO1
    AM13E_BOARD_ERPM_MASK=2U
    AM13E_BOARD_ERPM_PINCM=33U)
