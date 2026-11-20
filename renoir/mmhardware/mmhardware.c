// SPDX-License-Identifier: GPL-2.0-only
/* Based on Xiaomi's downstream multimedia hardware detection driver. */

#include <linux/atomic.h>
#include <linux/init.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>

#include "../../audio-kernel/include/soc/mmhardware.h"

struct mmhardware_attr {
	struct kobj_attribute attr;
	enum mmhardware_id id;
};

static struct kobject *mmhardware_kobj;
static atomic_t registered = ATOMIC_INIT(0);
static atomic_t calibrated = ATOMIC_INIT(0);

static ssize_t mmhardware_show(struct kobject *kobj,
			       struct kobj_attribute *attr, char *buf)
{
	const struct mmhardware_attr *hw =
		container_of(attr, struct mmhardware_attr, attr);
	bool present = atomic_read(&registered) & hw->id;

	if (hw->id < MM_HW_PA_1)
		return sysfs_emit(buf, "%d\n", present);

	return sysfs_emit(buf, "%d\n%d\n", present,
			  !!(atomic_read(&calibrated) & hw->id));
}

static ssize_t mmhardware_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf, size_t count)
{
	/* Preserve the legacy write behavior. */
	return count;
}

#define MM_ATTR(_name, _id)					\
	static struct mmhardware_attr mm_##_name = {		\
		.attr = __ATTR(_name, 0644, mmhardware_show,	\
			       mmhardware_store),			\
		.id = _id,					\
	}

MM_ATTR(adsp, MM_HW_ADSP);
MM_ATTR(codec, MM_HW_CODEC);
MM_ATTR(pa1, MM_HW_PA_1);
MM_ATTR(pa2, MM_HW_PA_2);
MM_ATTR(pa3, MM_HW_PA_3);
MM_ATTR(pa4, MM_HW_PA_4);
MM_ATTR(pa5, MM_HW_PA_5);
MM_ATTR(pa6, MM_HW_PA_6);
MM_ATTR(pa7, MM_HW_PA_7);
MM_ATTR(pa8, MM_HW_PA_8);

static struct attribute *mmhardware_attrs[] = {
	&mm_adsp.attr.attr, &mm_codec.attr.attr,
	&mm_pa1.attr.attr, &mm_pa2.attr.attr, &mm_pa3.attr.attr,
	&mm_pa4.attr.attr, &mm_pa5.attr.attr, &mm_pa6.attr.attr,
	&mm_pa7.attr.attr, &mm_pa8.attr.attr,
	NULL,
};
ATTRIBUTE_GROUPS(mmhardware);

static bool mmhardware_valid_id(enum mmhardware_id id)
{
	return id >= MM_HW_ADSP && id <= MM_HW_PA_8 && !(id & (id - 1));
}

int register_kobj_under_mmsysfs(enum mmhardware_id id, const char *name)
{
	if (!name || !mmhardware_valid_id(id))
		return -EINVAL;

	if (atomic_fetch_or(id, &registered) & id)
		return -EALREADY;

	return 0;
}
EXPORT_SYMBOL_GPL(register_kobj_under_mmsysfs);

int on_calibration_under_mmsysfs(enum mmhardware_id id, const char *name)
{
	if (!name || !mmhardware_valid_id(id))
		return -EINVAL;

	if (id >= MM_HW_PA_1)
		atomic_or(id, &calibrated);

	return 0;
}
EXPORT_SYMBOL_GPL(on_calibration_under_mmsysfs);

static int __init mmhardware_init(void)
{
	int ret;

	mmhardware_kobj = kobject_create_and_add("mm_hardware", NULL);
	if (!mmhardware_kobj)
		return -ENOMEM;

	ret = sysfs_create_groups(mmhardware_kobj, mmhardware_groups);
	if (ret)
		kobject_put(mmhardware_kobj);

	return ret;
}

static void __exit mmhardware_exit(void)
{
	sysfs_remove_groups(mmhardware_kobj, mmhardware_groups);
	kobject_put(mmhardware_kobj);
}

module_init(mmhardware_init);
module_exit(mmhardware_exit);
MODULE_AUTHOR("Xiaomi multimedia");
MODULE_DESCRIPTION("Xiaomi legacy multimedia hardware diagnostics");
MODULE_LICENSE("GPL");
