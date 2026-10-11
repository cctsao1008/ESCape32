#pragma once
void boot_am13e_release_io(void);
/* Called only after marker/vector validation, with IRQs masked. */
void boot_am13e_quiesce(void);
