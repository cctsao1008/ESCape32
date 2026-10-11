# AM13E FW1 Reference Board — Observed Implementation State

**NOT a board policy, feature exclusion, generic naming rule or
Rev1.4 architecture requirement.** Rev1.4 authority:
[V14_DESIGN_AUTHORITY.md](V14_DESIGN_AUTHORITY.md).

The currently linked Reference FW1 uses TI MCPWM/CMPSS/ECAP,
Rel17 input/motor algorithms, NTC/VBUS numerical models and the
selected flash configuration. Real pin allocations and polarity
must be established from a qualified board schematic, not inferred
from the generic MCU port.

| Current Reference observation | Remaining Rev1.4 work |
| --- | --- |
| PB13 Gate Enable held inactive and six motor output pads isolated | Physically validate gate polarity, safe-off/trip/deadtime and attach sequence on actual board |
| PB15 nFAULT initialized as GPIO input | Selected original fault behavior requires a qualified route/interrupt/trip implementation |
| Independent OC optional input not assigned | Protect only when real comparator/threshold/pin and board selected |
| Serial TX optional reference pin unassigned/Hi-Z | Original conditional serial input/telemetry remains in port scope, with native AM13E UART backend missing |
| Current-sense optional analog input unassigned | Original source-controlled SENS_MAP/current semantics stay in scope, dependent on physical board channel |

Current ancillary pin values are in `board_configuration.h`.
A selected board must explicitly configure electrical/timing data,
and Reference safe-off must not be mistaken for source-functional
completeness or a permanently disabled original feature.

ARM strict link and native Host tests are **software evidence only**.
This reference board has no claimed power-stage/hardware qualification.
