# AM13E Reference Board — Live Power-Stage Porting Profile

**Reference FW1 now defaults to functional Motor/Audio Power Stage code**:
`AM13E_POWER_STAGE_ENABLED=ON`. That enables real MCPWM0 output
connections, configurable 6-channel pinmux, RED/FED dead-band, gate
PB13 control and analog scaling at runtime. **No hardware qualification
or motor-spin test has been performed.**

## Runtime state transitions

1. At boot hold PB13 inactive, all motor pins disconnected, AQ forced LOW,
   MCPWM stopped. Install mandatory PB15 nFAULT hardware INPUTXBAR2 ->
   PWMXBAR1 -> MCPWM0 OST1 before interrupt release.
2. Install configurable TI MCPWM RED/FED dead-band and verify DriverLib
   DBCTL, DBRED, DBFED and shadow readback.
3. Acquire real ADC0 PA6 NTC / PA28 VBUS samples and pass the
   G431-derived default model's scaled values to Rel17 `adcdata()`.
4. Normal ESCape32 motor/Sine/Braking/Music/PCM requests start MCPWM0,
   map six phase pins to PWM, release software AQ LOW force, then assert
   PB13 driver enable **last**.
5. Stop, fault and audio/motor transitions first deassert PB13, force
   six AQ outputs LOW, disconnect phase pins, and stop the timebase.

**Overcurrent:** The existing reference PB15 nFAULT -> OST1 route is
mandatory and actively checked. Independent OC -> OST2 is implemented
and automatically activated if a real `AM13E_BOARD_OC_GPIO_PINCM` is
configured. Default `0` explicitly disables only OST2; no invented
hardware overcurrent input or fictitious OC source is used. Existing
BEMF comparator routing remains separate.

## Editable G431-derived development parameters

| Item | Default value | Origin and limitation |
| --- | --- | --- |
| Pin Mapping | Motor PA8/11, PA9/30, PA10/31; PB13 EN; PB14 command; PB15 nFAULT; PA6 NTC, PA28 VBUS | Existing Reference mapping |
| Gate Polarity | PB13 active-high `1`, PWM inversion mask `0`; external driver Hi-Z-safe and HW shutdown verification `0` | Active-high software convention; not a verified G431 PB13 net |
| Dead-time | RED=`92` / FED=`92` ticks; offset=`92`; polarities `0/1`, both input PWMA, swaps `0/0` | G431 TIM1 `DEAD_TIME=155` = 154 cycles / 168MHz = 916.7ns, mapped to 920ns at AM13E 100MHz |
| Sensing | 12-bit ADC `4095`, VREF `3300mV`, NTC supply `3300mV`, Rtop `26820`Ω / Rbottom `1000`Ω, NTC model `3` | Approximate G431 `VOLT_MUL=224` and NTC10K3455UP10K; **not actual AM13E board calibration** |
| Fault | PB15 active-low nFAULT OST1 required, OC PINCM `0` disables only OST2 | G431 COMP_MAP=132 is BEMF, not independent overcurrent |

CMake `-DAM13E_POWER_STAGE_ENABLED=OFF` explicitly selects the
former disconnected-output diagnostic variant. This is **not**
the default. To change electrical values use CMake's
`AM13E_BOARD_BOARD_PROFILE_FILE` hook to supply a list of
`AM13E_BOARD_BOARD_DEFINITIONS` or override individual defaults
in `board_configuration.h`.

## Feature completion vs validation

Real source paths are connected and linked, and software regression
tests can verify source/encoding/IRQ integration. Physical output
polarities, shoot-through prevention, gate-driver input states, OST1
response, thermal behavior, and ADC gain are NOT yet validated on a
real board; they require a controlled bench bring-up. In particular,
there is **no physically routed current-sense ADC** in this Reference
profile; Rel17 current-dependent features need an additional actual
board input before they are complete on a particular ESC.

The boot/update v1.6 APP_BASE=0x6000, CRC, signature and other
on-flash ABI contracts have not changed.
