# AM13E Source-file Naming Contract

AM13E firmware filenames describe **software responsibilities**, rather than specific board pins or physical peripheral instances.

Examples:
- `fault_input.[ch]`: application fault input supervision
- `command_capture.[ch]`: motor command pulse capture
- `command_decode.[ch]`: protocol timing decode
- `command_reply.[ch]`: bidirectional command response
- `analog_runtime.[ch]`, `analog_sampling_backend.[ch]`: analog sampling
- `motor_output_backend.[ch]`, `motor_output_route_plan.[ch]`: motor output routing
- `clock_source_reference.c`: reference clock-source implementation

Do not encode GPIO, ADC, PB14/PA6, XTAL25 or pad names in C/H filenames.

Board-specific providers use `board_*_reference` names and preserve exact
Reference hardware assignments and compile-time restrictions. Generic MCU
backend modules accept explicit routing; they must never silently choose
reference-board wiring.

Register identifiers, peripheral names and physical wiring **remain** in
implementation and hardware evidence where precise and necessary.
The renaming does not change public Rel17 APIs, electrical settings,
Boot/Image ABI, Motor/Audio MCPWM0 ownership, fault interlocks or behavior.

CI runs `tools/check_source_filenames.py` for all AM13E C/H files,
including native host tests.
