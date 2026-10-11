#pragma once
/* Host libc lacks newlib's itoa(). Formatting is a libc boundary. */
char *itoa(int value,char *out,int radix);
