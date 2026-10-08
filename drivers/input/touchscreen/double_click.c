#include <linux/double_click.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/init.h>
#include <linux/kernel.h>

static bool doubleclick;

void tp_enable_doubleclick(bool state)
{
	doubleclick = state;
}
EXPORT_SYMBOL_GPL(tp_enable_doubleclick);

bool is_tp_doubleclick_enable(void)
{
	return doubleclick;
}
EXPORT_SYMBOL_GPL(is_tp_doubleclick_enable);

int pox_dt2w_get(void)
{
	return doubleclick ? 1 : 0;
}
EXPORT_SYMBOL(pox_dt2w_get);

int pox_dt2w_set(int enable)
{
	tp_enable_doubleclick(!!enable);
	pr_info("Double-Tap to Wake set to %d\n", !!enable);
	return 0;
}
EXPORT_SYMBOL(pox_dt2w_set);

#include <linux/capability.h>

static ssize_t dt2w_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", pox_dt2w_get());
}

static ssize_t dt2w_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	int val = 0;

	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;

	if (kstrtoint(buf, 10, &val) == 0)
		pox_dt2w_set(val);
	return count;
}

static struct kobj_attribute dt2w_kattr = __ATTR(doubletap2wake, 0644, dt2w_show, dt2w_store);

static int __init dt2w_init(void)
{
	struct kobject *touch_kobj = kobject_create_and_add("android_touch", NULL);
	if (touch_kobj) {
		int ret = sysfs_create_file(touch_kobj, &dt2w_kattr.attr);
		if (ret)
			pr_warn("Failed to create /sys/android_touch/doubletap2wake (ret=%d)\n", ret);
	}
	return 0;
}
late_initcall(dt2w_init);
