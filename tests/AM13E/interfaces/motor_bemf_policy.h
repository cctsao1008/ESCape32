#pragma once
typedef struct { int interval_us,delay_us,fast; } AM13E_BemfPolicyPlan;
/* 1 = accepted and filled output, 0 = early (state unchanged),
 * -1 = invalid arithmetic/input. Only accepted edges arm TIMG12.
 */
int am13e_bemf_policy_plan(int ival,int ertm,int captured,int timing,
                          AM13E_BemfPolicyPlan *out);
