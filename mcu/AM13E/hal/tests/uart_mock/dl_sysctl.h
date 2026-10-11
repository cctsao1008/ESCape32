#pragma once
enum { DL_SYSCTL_CMP_HP0,DL_SYSCTL_CMP_HP1,DL_SYSCTL_CMP_HN0,DL_SYSCTL_CMP_HN1 };
#define DL_SYSCTL_ERROR_BEHAVIOR_RESET 1
void DL_SYSCTL_setWWDT0ErrorBehavior(int behavior);
int DL_SYSCTL_getWWDT0ErrorBehavior(void);
