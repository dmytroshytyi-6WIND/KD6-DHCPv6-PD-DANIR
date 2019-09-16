#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

#ifdef RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0xe8aad16f, "module_layout" },
	{ 0x779a18af, "kstrtoll" },
	{ 0x91715312, "sprintf" },
	{ 0x5097b0a5, "nf_unregister_net_hook" },
	{ 0x151f4898, "schedule_timeout_uninterruptible" },
	{ 0xa0be19c0, "skb_put" },
	{ 0x70efb612, "nf_register_net_hook" },
	{ 0x79aa04a2, "get_random_bytes" },
	{ 0xf2c30e8c, "kthread_stop" },
	{ 0xa71f4f0b, "wake_up_process" },
	{ 0xbc73131c, "kthread_create_on_node" },
	{ 0x467583ec, "dev_change_flags" },
	{ 0xd83683cd, "dev_queue_xmit" },
	{ 0x15ba50a6, "jiffies" },
	{ 0x952664c5, "do_exit" },
	{ 0xb3f7646e, "kthread_should_stop" },
	{ 0xb47cca30, "csum_ipv6_magic" },
	{ 0xe113bbbc, "csum_partial" },
	{ 0xe26d48db, "ipv6_dev_get_saddr" },
	{ 0x5faf8e24, "skb_push" },
	{ 0x91824b90, "__alloc_skb" },
	{ 0x37befc70, "jiffies_to_msecs" },
	{ 0x20c55ae0, "sscanf" },
	{ 0xb18fc00, "current_task" },
	{ 0xaafdc258, "strcasecmp" },
	{ 0xf9a482f9, "msleep" },
	{ 0x3fba1fa8, "init_net" },
	{ 0xe2d5255a, "strcmp" },
	{ 0xf6ebc03b, "net_ratelimit" },
	{ 0x301fa007, "_raw_spin_unlock" },
	{ 0x769574a3, "__pskb_pull_tail" },
	{ 0x449ad0a7, "memcmp" },
	{ 0xdbf17652, "_raw_spin_lock" },
	{ 0xdb7305a1, "__stack_chk_fail" },
	{ 0x82a15c00, "kfree_skb" },
	{ 0xc8868991, "consume_skb" },
	{ 0xf490b91d, "skb_clone" },
	{ 0x27e1a049, "printk" },
	{ 0x8c7d81de, "kmem_cache_alloc_trace" },
	{ 0xbc5cde86, "kmalloc_caches" },
	{ 0x6e720ff2, "rtnl_unlock" },
	{ 0x37a0cba, "kfree" },
	{ 0x9b7fe4d4, "__dynamic_pr_debug" },
	{ 0xc7a4fbed, "rtnl_lock" },
	{ 0xbdfb6dbb, "__fentry__" },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";


MODULE_INFO(srcversion, "24B240B94AEDCDB16B3A329");
