#include <linux/build-salt.h>
#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

BUILD_SALT;

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
	{ 0xfc6cb152, "module_layout" },
	{ 0xb8b31ec6, "rt6_add_dflt_router" },
	{ 0x259f0724, "addrconf_prefix_rcv" },
	{ 0x779a18af, "kstrtoll" },
	{ 0x91715312, "sprintf" },
	{ 0x82a87675, "nf_unregister_net_hook" },
	{ 0x151f4898, "schedule_timeout_uninterruptible" },
	{ 0xa18fffba, "skb_put" },
	{ 0x3b24d791, "nf_register_net_hook" },
	{ 0x79aa04a2, "get_random_bytes" },
	{ 0x50e13afd, "kthread_stop" },
	{ 0x17633d9f, "wake_up_process" },
	{ 0xf33790ef, "kthread_create_on_node" },
	{ 0x64689e31, "dev_change_flags" },
	{ 0xda07057a, "dev_queue_xmit" },
	{ 0x15ba50a6, "jiffies" },
	{ 0x952664c5, "do_exit" },
	{ 0xb3f7646e, "kthread_should_stop" },
	{ 0xb47cca30, "csum_ipv6_magic" },
	{ 0xe113bbbc, "csum_partial" },
	{ 0xd804ee14, "ipv6_dev_get_saddr" },
	{ 0xebef2d58, "skb_push" },
	{ 0xd5258e83, "__alloc_skb" },
	{ 0x37befc70, "jiffies_to_msecs" },
	{ 0x20c55ae0, "sscanf" },
	{ 0x6118b3b9, "current_task" },
	{ 0xaafdc258, "strcasecmp" },
	{ 0xf9a482f9, "msleep" },
	{ 0xa33c79cb, "init_net" },
	{ 0xe2d5255a, "strcmp" },
	{ 0xf6ebc03b, "net_ratelimit" },
	{ 0x301fa007, "_raw_spin_unlock" },
	{ 0xd9a6d477, "__pskb_pull_tail" },
	{ 0x449ad0a7, "memcmp" },
	{ 0xdbf17652, "_raw_spin_lock" },
	{ 0xdb7305a1, "__stack_chk_fail" },
	{ 0x6a6137d3, "kfree_skb" },
	{ 0xba38ede8, "consume_skb" },
	{ 0x785f4473, "skb_clone" },
	{ 0x7c32d0f0, "printk" },
	{ 0x490e4980, "kmem_cache_alloc_trace" },
	{ 0x6fbd61f1, "kmalloc_caches" },
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


MODULE_INFO(srcversion, "463EA6ADC03C1929BA5FD44");
