#include <linux/module.h>
#include <linux/export-internal.h>
#include <linux/compiler.h>

MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};



static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0x9dd4105e, "free_irq" },
	{ 0x9f222e1e, "alloc_chrdev_region" },
	{ 0xa61fd7aa, "__check_object_size" },
	{ 0x0e1afaf1, "gpiod_put" },
	{ 0xfd355b93, "config_group_init" },
	{ 0x092a35a2, "_copy_from_user" },
	{ 0xd710adbf, "__kmalloc_noprof" },
	{ 0x4695bf9b, "platform_driver_unregister" },
	{ 0x40a621c5, "snprintf" },
	{ 0xa1dacb42, "class_destroy" },
	{ 0xa68efaaa, "gpiod_to_irq" },
	{ 0x8e142c2e, "kstrtouint" },
	{ 0xcb8b6ec6, "kfree" },
	{ 0x5e505530, "kthread_should_stop" },
	{ 0xd272d446, "__fentry__" },
	{ 0x9b3fd95d, "configfs_unregister_subsystem" },
	{ 0x630dad60, "wake_up_process" },
	{ 0xe8213e80, "_printk" },
	{ 0xbd03ed67, "__ref_stack_chk_guard" },
	{ 0xd272d446, "__stack_chk_fail" },
	{ 0x9479a1e8, "strnlen" },
	{ 0xd70733be, "sized_strscpy" },
	{ 0x8ea73856, "cdev_add" },
	{ 0x60f269c5, "configfs_register_subsystem" },
	{ 0xd09b06f5, "kstrtoint" },
	{ 0x9126ce86, "request_threaded_irq" },
	{ 0xe486c4b7, "device_create" },
	{ 0x23ef80fb, "noop_llseek" },
	{ 0x2bf498f8, "gpiod_direction_input" },
	{ 0x653aa194, "class_create" },
	{ 0xbd03ed67, "random_kmalloc_seed" },
	{ 0xf46d5bf3, "mutex_lock" },
	{ 0x2435d559, "strncmp" },
	{ 0xeb5ade73, "sysfs_create_group" },
	{ 0x0571dc46, "kthread_stop" },
	{ 0x173ec8da, "sscanf" },
	{ 0xc1e6c71e, "__mutex_init" },
	{ 0xe54e0a6b, "__fortify_panic" },
	{ 0xd272d446, "__x86_return_thunk" },
	{ 0x092a35a2, "_copy_to_user" },
	{ 0x888b8f57, "strcmp" },
	{ 0x223cc85c, "__platform_driver_register" },
	{ 0x7f79e79a, "kthread_create_on_node" },
	{ 0x7320379d, "sysfs_remove_group" },
	{ 0xdd6830c7, "sprintf" },
	{ 0x0bc5fb0d, "unregister_chrdev_region" },
	{ 0xa68efaaa, "desc_to_gpio" },
	{ 0xa5c7582d, "strsep" },
	{ 0xf46d5bf3, "mutex_unlock" },
	{ 0x3da34d04, "config_item_init_type_name" },
	{ 0x1595e410, "device_destroy" },
	{ 0xc064623f, "__kmalloc_cache_noprof" },
	{ 0x546c19d9, "validate_usercopy_range" },
	{ 0x9346b6c5, "gpio_to_desc" },
	{ 0x7a6661ca, "ktime_get_seconds" },
	{ 0xe4de56b4, "__ubsan_handle_load_invalid_value" },
	{ 0x43a349ca, "strlen" },
	{ 0x67628f51, "msleep" },
	{ 0xd5f66efd, "cdev_init" },
	{ 0xfaabfe5e, "kmalloc_caches" },
	{ 0x4e54d6ac, "cdev_del" },
	{ 0xbebe66ff, "module_layout" },
};

static const u32 ____version_ext_crcs[]
__used __section("__version_ext_crcs") = {
	0x9dd4105e,
	0x9f222e1e,
	0xa61fd7aa,
	0x0e1afaf1,
	0xfd355b93,
	0x092a35a2,
	0xd710adbf,
	0x4695bf9b,
	0x40a621c5,
	0xa1dacb42,
	0xa68efaaa,
	0x8e142c2e,
	0xcb8b6ec6,
	0x5e505530,
	0xd272d446,
	0x9b3fd95d,
	0x630dad60,
	0xe8213e80,
	0xbd03ed67,
	0xd272d446,
	0x9479a1e8,
	0xd70733be,
	0x8ea73856,
	0x60f269c5,
	0xd09b06f5,
	0x9126ce86,
	0xe486c4b7,
	0x23ef80fb,
	0x2bf498f8,
	0x653aa194,
	0xbd03ed67,
	0xf46d5bf3,
	0x2435d559,
	0xeb5ade73,
	0x0571dc46,
	0x173ec8da,
	0xc1e6c71e,
	0xe54e0a6b,
	0xd272d446,
	0x092a35a2,
	0x888b8f57,
	0x223cc85c,
	0x7f79e79a,
	0x7320379d,
	0xdd6830c7,
	0x0bc5fb0d,
	0xa68efaaa,
	0xa5c7582d,
	0xf46d5bf3,
	0x3da34d04,
	0x1595e410,
	0xc064623f,
	0x546c19d9,
	0x9346b6c5,
	0x7a6661ca,
	0xe4de56b4,
	0x43a349ca,
	0x67628f51,
	0xd5f66efd,
	0xfaabfe5e,
	0x4e54d6ac,
	0xbebe66ff,
};
static const char ____version_ext_names[]
__used __section("__version_ext_names") =
	"free_irq\0"
	"alloc_chrdev_region\0"
	"__check_object_size\0"
	"gpiod_put\0"
	"config_group_init\0"
	"_copy_from_user\0"
	"__kmalloc_noprof\0"
	"platform_driver_unregister\0"
	"snprintf\0"
	"class_destroy\0"
	"gpiod_to_irq\0"
	"kstrtouint\0"
	"kfree\0"
	"kthread_should_stop\0"
	"__fentry__\0"
	"configfs_unregister_subsystem\0"
	"wake_up_process\0"
	"_printk\0"
	"__ref_stack_chk_guard\0"
	"__stack_chk_fail\0"
	"strnlen\0"
	"sized_strscpy\0"
	"cdev_add\0"
	"configfs_register_subsystem\0"
	"kstrtoint\0"
	"request_threaded_irq\0"
	"device_create\0"
	"noop_llseek\0"
	"gpiod_direction_input\0"
	"class_create\0"
	"random_kmalloc_seed\0"
	"mutex_lock\0"
	"strncmp\0"
	"sysfs_create_group\0"
	"kthread_stop\0"
	"sscanf\0"
	"__mutex_init\0"
	"__fortify_panic\0"
	"__x86_return_thunk\0"
	"_copy_to_user\0"
	"strcmp\0"
	"__platform_driver_register\0"
	"kthread_create_on_node\0"
	"sysfs_remove_group\0"
	"sprintf\0"
	"unregister_chrdev_region\0"
	"desc_to_gpio\0"
	"strsep\0"
	"mutex_unlock\0"
	"config_item_init_type_name\0"
	"device_destroy\0"
	"__kmalloc_cache_noprof\0"
	"validate_usercopy_range\0"
	"gpio_to_desc\0"
	"ktime_get_seconds\0"
	"__ubsan_handle_load_invalid_value\0"
	"strlen\0"
	"msleep\0"
	"cdev_init\0"
	"kmalloc_caches\0"
	"cdev_del\0"
	"module_layout\0"
;

MODULE_INFO(depends, "");

MODULE_ALIAS("of:N*T*Cgeneric,anemometer");
MODULE_ALIAS("of:N*T*Cgeneric,anemometerC*");

MODULE_INFO(srcversion, "A29649B891240C596BC7EE1");
