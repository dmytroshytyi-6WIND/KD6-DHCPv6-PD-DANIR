# KD6-DHCPv6-PD-DANIR
Lite Kernel DHCPv6 Prefix Delegation &amp; Neighbour Discovery implementation


# DEMO and IETF docs:
It is an implementation of draft-shytyi-v6ops-danir-03.txt: https://tools.ietf.org/html/draft-shytyi-v6ops-danir-03
Demo of this implementation: https://www.youtube.com/watch?v=DymVQY7bCUM

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
