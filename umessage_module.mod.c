#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/elfnote-lto.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

BUILD_SALT;
BUILD_LTO_INFO;

MODULE_INFO(vermagic, VERMAGIC_STRING);
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

#ifdef CONFIG_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif

static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0xf704969, "module_layout" },
	{ 0x6bc3fbc0, "__unregister_chrdev" },
	{ 0xeb233a45, "__kmalloc" },
	{ 0xd9b85ef6, "lockref_get" },
	{ 0x289828cc, "__register_chrdev" },
	{ 0x755a0fd9, "init_user_ns" },
	{ 0xd81a3247, "mount_bdev" },
	{ 0x3d0a55bc, "d_add" },
	{ 0x4c55f5ef, "pv_ops" },
	{ 0xe2d5255a, "strcmp" },
	{ 0x6b10bee1, "_copy_to_user" },
	{ 0x5b8239ca, "__x86_return_thunk" },
	{ 0x18554f24, "current_task" },
	{ 0xab4169d3, "__bread_gfp" },
	{ 0x9ec6ca96, "ktime_get_real_ts64" },
	{ 0x6f15f55a, "blkdev_get_by_path" },
	{ 0xf71ee839, "set_nlink" },
	{ 0x2ce06643, "__brelse" },
	{ 0x87a21cb3, "__ubsan_handle_out_of_bounds" },
	{ 0xd0da656b, "__stack_chk_fail" },
	{ 0x92997ed8, "_printk" },
	{ 0x2173540a, "unlock_new_inode" },
	{ 0xec41023e, "kill_block_super" },
	{ 0x65487097, "__x86_indirect_thunk_rax" },
	{ 0xbdfb6dbb, "__fentry__" },
	{ 0x12e21383, "register_filesystem" },
	{ 0x348484ad, "iput" },
	{ 0xaa2af4ae, "d_make_root" },
	{ 0x4dea0566, "unregister_filesystem" },
	{ 0x13c49cc2, "_copy_from_user" },
	{ 0xc86187a, "param_ops_ulong" },
	{ 0x88db9f48, "__check_object_size" },
	{ 0xa16c77dc, "iget_locked" },
	{ 0x5d40dc60, "inode_init_owner" },
};

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "EBA782C4F1E9763438DEDC1");
