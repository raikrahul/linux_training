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
	{ 0x7a8e92c6, "unregister_kprobe" },
	{ 0x4d40f3a6, "const_pcpu_hot" },
	{ 0x2435d559, "strncmp" },
	{ 0xd272d446, "__fentry__" },
	{ 0xe8213e80, "_printk" },
	{ 0x1c489eb6, "register_kprobe" },
	{ 0xd272d446, "__x86_return_thunk" },
	{ 0xba157484, "module_layout" },
};

static const u32 ____version_ext_crcs[]
__used __section("__version_ext_crcs") = {
	0x7a8e92c6,
	0x4d40f3a6,
	0x2435d559,
	0xd272d446,
	0xe8213e80,
	0x1c489eb6,
	0xd272d446,
	0xba157484,
};
static const char ____version_ext_names[]
__used __section("__version_ext_names") =
	"unregister_kprobe\0"
	"const_pcpu_hot\0"
	"strncmp\0"
	"__fentry__\0"
	"_printk\0"
	"register_kprobe\0"
	"__x86_return_thunk\0"
	"module_layout\0"
;

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "AB0D06A60E645A7EC41A973");
