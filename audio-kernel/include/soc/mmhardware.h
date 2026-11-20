/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _SOC_MMHARDWARE_H
#define _SOC_MMHARDWARE_H

#include <linux/module.h>

enum mmhardware_id {
	MM_HW_ADSP = 0x2,
	MM_HW_CODEC = 0x4,
	MM_HW_PA_1 = 0x8,
	MM_HW_PA_2 = 0x10,
	MM_HW_PA_3 = 0x20,
	MM_HW_PA_4 = 0x40,
	MM_HW_PA_5 = 0x80,
	MM_HW_PA_6 = 0x100,
	MM_HW_PA_7 = 0x200,
	MM_HW_PA_8 = 0x400,
};

int register_kobj_under_mmsysfs(enum mmhardware_id id, const char *name);
int on_calibration_under_mmsysfs(enum mmhardware_id id, const char *name);

static inline void mmhardware_report(enum mmhardware_id id, const char *name)
{
	typeof(&register_kobj_under_mmsysfs) report;

	report = symbol_get(register_kobj_under_mmsysfs);
	if (report) {
		report(id, name);
		symbol_put(register_kobj_under_mmsysfs);
	}
}

#endif
