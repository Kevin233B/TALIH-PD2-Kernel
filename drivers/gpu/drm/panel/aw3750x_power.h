/* SPDX-License-Identifier: GPL-2.0 */
/*
 * AW3750x LCD bias helpers used by the HX83121A panel drivers.
 */
#ifndef _AW3750X_POWER_H_
#define _AW3750X_POWER_H_

int tct_aw3750x_volt_outp_set(int code);
int tct_aw3750x_volt_outn_set(int code);
int tct_aw3750x_ldo_current_set(int outp_uA, int outn_uA);

#endif
