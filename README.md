# KD6-DHCPv6-PD-DANIR
Lite Kernel DHCPv6 Prefix Delegation &amp; Neighbour Discovery implementation

The kernel in the IoT Router issues a DHCPv6 PD request on its egress interface and obtains a /56. Further it splits multiple /64s out of it and sends RAs with /64 on the ingress interfaces. After clients receive the RA the default route is configured (link-local) address of IoT Requesting Router and /64 prefix is used to configure interface IP address.
Finally this implementation doesn't include cellular part.

# DEMO and IETF docs:
It is an implementation of draft-shytyi-v6ops-danir-03.txt: https://tools.ietf.org/html/draft-shytyi-v6ops-danir-03
Demo of this implementation: https://www.youtube.com/watch?v=DymVQY7bCUM
The DEMO recorded the virtual machines output on the IoT Router, the Client device behind it, and the DHCPv6-PD server. 

# Compiled with kernel:
Linux ferby 4.19.4pachedkernel-v2 #7 SMP PREEMPT Fri Aug 23 04:58:09 CEST 2019 x86_64 GNU/Linux

# Suggestions:
Do not forget to enable ipv6 forwarding on the IoT Router.

# Requirements:
Patched kernel. 

# Problems and solutions:
Problem:
	insmod: ERROR: could not insert module danir.ko: Unknown symbol in module
Errors in the journalctl -f during insmod:
	Sep 16 23:48:17 ferby kernel: danir: Unknown symbol rt6_add_dflt_router (err 0)
	Sep 16 23:48:17 ferby kernel: danir: Unknown symbol addrconf_prefix_rcv (err 0)
Solution:
	Stock kernel doesn't expose some functions. Thus such errors appear.
	need to add few lines of the code to fix the errors.
	Example: EXPORT_SYMBOL(rt2_add_dflt_router);
	Example: EXPORT_SYMBOL(addrconf_refix_rcv);


