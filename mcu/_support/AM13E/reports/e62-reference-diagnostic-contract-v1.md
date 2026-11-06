# SIM_ONLY SC5 passive evidence contract v1

AM13E_REFERENCE_DIAGNOSTICS requires dynamic SC5 and an explicit SIM_ONLY board
profile. It never supplies reference codes, changes control decisions, or enables
capture. The bounded first64/last256 journal retains gaps explicitly.

20 uint32 words per record: sequence; event (1 stage,2 DAC pending,3 active DAC
matched,4 interval-select returned); serviced SysTick; native ECAP counter;
selected code; pending code; expected DACH; active DACH; calculated; published;
ready; armed; staged sample ID; acquisition tick; completion tick; max age;
staged blackout epoch; firmware freshness result; latest-reference-valid;
latest sample ID. SysTick/ECAP are MCU clocks, not host time or automatically
absolute QEMU timestamps. Counter reset/freeze epochs must be accounted for
before source-event alignment. Commit is sequence-last under a short interrupt
mask. Extra observer instructions/latency make this a separate experimental ELF.
Default builds compile out the observer. No motor/hardware qualification follows.

The virtual calibration profile consumes native VBUS samples using an explicit
3.3V ADC/DAC reference, VBUS ratio 11/160 and phase ratio 11/80. This is a
SIM_ONLY fixture network, not a measured PCB. It derives each code through the
existing checked conversion, never an initial synthetic DACH code. VBUS fixture
input is static; this does not qualify dynamic bus response or real calibration.
