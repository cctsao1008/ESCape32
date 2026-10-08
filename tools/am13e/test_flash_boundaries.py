#!/usr/bin/env python3
"""E62 APP-only 2-KiB erase / 1-KiB protocol-block boundary regression.

Models address decisions visible in boot_port.c and boot_flash.c.
This is NOT a target Flash controller execution test.
"""
from __future__ import annotations

APP = 0x6000
END = 0x80000
BANK = 0x40000
SECTOR = 0x800
BLOCK = 0x400

def erase_addresses(offset: int, size: int) -> list[int]:
    if size <= 0 or offset < 0 or offset >= END - APP or size > END - APP - offset:
        raise ValueError("invalid APP write range")
    address = APP + offset
    last = (address + size - 1) & ~(SECTOR - 1)
    first = (address + SECTOR - 1) & ~(SECTOR - 1)
    return list(range(first, last + 1, SECTOR))

def main() -> None:
    assert APP == 0x6000 and END - APP == 0x7A000
    assert (END - APP) % BLOCK == 0
    assert APP % SECTOR == 0 and BANK % SECTOR == 0

    erased: set[int] = set()
    for offset in range(0, END - APP, BLOCK):
        address = APP + offset
        assert APP <= address < END
        assert address + BLOCK <= END
        assert (address & 15) == 0
        marks = erase_addresses(offset, BLOCK)
        assert len(marks) <= 1
        for sector in marks:
            assert sector >= APP and sector + SECTOR <= END
            assert sector not in erased, f"duplicate erase at {sector:#x}"
            erased.add(sector)
    assert len(erased) == (END - APP) // SECTOR
    assert min(erased) == APP and max(erased) == END - SECTOR
    print("[PASS] 488-KiB sequential APP: every 2-KiB sector erased exactly once")
    print("[PASS] protected Boot / FW1 CFG / FW2 CFG never erased")

    assert erase_addresses(0, BLOCK) == [APP]
    assert erase_addresses(BLOCK, BLOCK) == []
    assert erase_addresses(BLOCK * 2, BLOCK) == [APP + SECTOR]
    assert erase_addresses(BANK - APP - BLOCK, BLOCK) == []
    assert erase_addresses(BANK - APP, BLOCK) == [BANK]
    assert erase_addresses(END - APP - BLOCK, BLOCK) == []
    print("[PASS] APP start / Bank0-Bank1 transition / APP end sector behavior")

    for offset, size in [(-1, 1024), (0, 0), (END - APP, 16),
                         (END - APP - 8, 16), (0, END - APP + 1)]:
        try:
            erase_addresses(offset, size)
        except ValueError:
            pass
        else:
            raise AssertionError(f"accepted out-of-range: {offset=} {size=}")
    print("[PASS] address model rejects empty/out-of-range writes")

    # Important: calling the current erase algorithm on an unaligned,
    # short independent write does not erase its first partially covered
    # sector. Transaction layer must constrain WRITE order and alignment.
    assert erase_addresses(4, 1024) == []
    print("[NOTE] erase policy assumes sequential, sector-aligned transaction from block 0")
    print("[PASS] E62 Flash boundary model")

if __name__ == "__main__":
    main()
