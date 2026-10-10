# Rev1.4-only Rule Replacement Audit

Source authority: [V14_DESIGN_AUTHORITY.md](V14_DESIGN_AUTHORITY.md).
This audit records deletion of superseded repository rules; it is
**not another architecture baseline**.

| Retired file | Reason |
| --- | --- |
| `AM13E_ESCape32_Porting_Architecture_Rules.md` | Invented additional selectors/limits and rules outside supplied Rev1.4 |
| `mcu/AM13E/SOURCE_NAMING_POLICY.md` | Extra regex filename restrictions absent from Rev1.4 |
| `mcu/AM13E/INTEGRATION_ARCHITECTURE_ALIGNMENT.md` | Superseded Rev1.1 A/C/B interpretation; original Rev1.4 documents now govern |
| `mcu/AM13E/FW1_PB14_ONLY_SCOPE.md` | PB14-only exclusion must not override source-defined conditional modes |
| `mcu/AM13E/BOARD_CONFIGURATION.md` | Five IO-only permanent exclusions replaced by Reference implementation facts |
| `mcu/AM13E/GENERIC_PLATFORM_SCOPE.md` | Mixed earlier proposed generic rules with partial evidence; Rev1.4-only summaries supersede it |
| `mcu/AM13E/tools/check_source_filenames.py` | Removed extra naming-policy CI; Rev1.4 does not require it |

The Reference FW1 remains physically disconnected / fail-closed
until a real board is qualified; deletion of a **rule** does not
claim a new functionality or enable power outputs. Original conditional
source paths, `CMD_UPDATE` and `CMD_SETWRP` still require
native implementation and verification. Host CI PASS is never
full hardware or Rel17 feature-parity PASS.
