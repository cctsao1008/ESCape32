# Dynamic source handover evidence, v4

SIM_ONLY companion to experimental PR9, stacked on `experiment/e62-sc5-changed-handover`. This branch adds passive request/load/shutdown observations and an explicit virtual analog-network profile; it does not change PR9's control policy. The firmware default remains `am13e-port-v2`.

Source build at e00c2ea produces experimental ELF SHA256 e198f5e9f11fe9afaaddf8a169c8e30582382808c55c5336d60870777b7292d4 with SDK b872469ceef18de5e39a1d3acb0821ec158b44c7 and ARM GCC14.2.1 20241119. This is neither Original05563 nor Production Root Fixes c49a. The affine network is explicitly 1.41V+.02*(VBUS/2+BEMF), VBUS ratio11/160 and ADC/DAC3.3V. At24V it preserves the existing virtual phase-input center/gain. These are declared virtual fixtures, not board calibration measurements.

The companion QEMU branch `experiment/e62-source-vbus-handover` applies external supply changes at source DShot frames1840/1844. Two native acquired samples independently convert to different DAC codes and are linked through actual Guest reads, shadow writes, native ZERO loads and the passive software-ready journal. Capture remains disabled during loading. The retained chains use sample1829, phaseC/rising, and sample1833, phaseB/falling; two native ZERO events precede each ready observation. This proves these specific paths, not analog settling sufficiency or rotor synchronization.

Native invalid-input and conversion-clock-loss tests both shut down with capture/publication/ready cleared and final gate mask0. In the invalid-input trace the previous sample is also expired at fault observation: invalid-only-before-expiry causality is not isolated. The existing register tests cover isolated invalid/stale/phase/epoch/delayed-load failures; they remain distinct from native integration and hardware approval. Clock-loss recovery requires reset in this fixture.

The full changed-handover SDK regression on this branch passes107 groups, including the production motor register configurations:

```sh
source /workspace/setup-escape32/activate.sh
AM13E_SDK_ROOT=/tmp/e62-issues/sdk bash mcu/_support/AM13E/tools/run_sc5_changed_handover_regression.sh
```

The new startup replay remains failed. Its first callback explicitly records capture2782us, original interval10000us and REJECTED_EARLY; the second records capture13728us and accepted delay2733us. A real sampled reversal precedes the first commutation IRQ while fresh matched reference observations bracket it. `rejected_events` is reset at each capture start, so its final value0 is not a lifetime rejection count. Do not replace the persistent decision history with that final counter.

No startup-policy change is proposed by this diagnostic branch. Existing early-acceptance experiments and their failures remain relevant; accepting more captures alone does not establish correct sector/rotor synchronization. No production merge, physical approval or issue closure follows from these results. Exact commands, raw traces, failed-reader snapshots and the source-clock failures are indexed by the companion repository's v4 engineering report and evidence bundle.
