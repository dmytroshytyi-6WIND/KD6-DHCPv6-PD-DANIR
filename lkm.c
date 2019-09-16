/**
 * @file    lkm.c
 * @author  Dmytro Shytyi
 * @date    14 Octobre 2018
 * @date    23 September 2019
 * @version 0.7
 * @brief This is a Lite Linux Kernel Implementation of DHCPv6PD and NDP(DANIR).
 * 	  Details in the IETF draft https://datatracker.ietf.org/doc/draft-shytyi-v6ops-danir
 * @see https://dmytro.shytyi.net/ for a full description.
 * @mail contact.dmytro@shytyi.net
 * @credits Martin Mares, Gero Kuhlmann et al. Inspired by linux/net/ipv4/ipconfig.c
 */




#include <linux/init.h>             // Macros used to mark up functions e.g., __init __exit
#include <linux/module.h>           // Core header for loading LKMs into the kernel
#include <linux/kernel.h>           // Contains types, macros, functions for the kernel

#include <linux/types.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/fcntl.h>
#include <linux/socket.h>
#include <linux/in.h>
#include <linux/inet.h>
#include <linux/skbuff.h>
#include <linux/if_arp.h>
#include <linux/netdevice.h> 
#include <linux/etherdevice.h>
#include <linux/netdevice.h> 
#include <linux/etherdevice.h>
#include <linux/string.h>
#include <linux/ip.h> 
#include <linux/udp.h>


#include <linux/jiffies.h>
#include <linux/random.h>
#include <linux/utsname.h>
#include <linux/if.h>
#include <linux/inetdevice.h>
#include <linux/route.h>

#include <linux/root_dev.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/export.h>
#include <linux/inetdevice.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv6.h>

#include <net/net_namespace.h>
#include <net/dsa.h>
#include <net/ip.h>
#include <net/ipconfig.h>
#include <net/route.h>

#include <linux/uaccess.h>
#include <net/checksum.h>
#include <asm/processor.h>

#include <linux/ipv6.h>
#include <linux/if_ether.h>
#include <net/udp.h>
#include <net/ip6_checksum.h>

#include <linux/utsname.h>
#include <linux/time.h>


#include <net/addrconf.h>
#include <net/ip6_route.h>
#include <net/ip6_fib.h>

#include <linux/kthread.h>

MODULE_LICENSE("GLPv3.0");              ///< The license type -- this affects runtime behavior
MODULE_AUTHOR("Dmytro Shytyi");      ///< The author -- visible when you use modinfo
MODULE_DESCRIPTION("[KD6] DANIR Kernel DHCPv6 Prefix Delegation"); 
MODULE_VERSION("1.1");              ///< The version of the module



/* 
 * DHCPv6 message types, defined in section 5.3 of RFC 3315 
 */
#define KD6_SOLICIT              1
#define KD6_ADVERTISE            2
#define KD6_REQUEST              3
#define KD6_CONFIRM              4
#define KD6_RENEW                5
#define KD6_REBIND               6
#define KD6_REPLY                7
#define KD6_RELEASE              8
#define KD6_DECLINE              9
#define KD6_RECONFIGURE         10
#define KD6_INFORMATION_REQUEST 11
#define KD6_RELAY_FORW          12
#define KD6_RELAY_REPL          13
#define KD6_LEASEQUERY          14   /* RFC5007 */
#define KD6_LEASEQUERY_REPLY    15   /* RFC5007 */
#define KD6_LEASEQUERY_DONE     16   /* RFC5460 */
#define KD6_LEASEQUERY_DATA     17   /* RFC5460 */
#define KD6_RECONFIGURE_REQUEST 18   /* RFC6977 */
#define KD6_RECONFIGURE_REPLY   19   /* RFC6977 */
#define KD6_DHCPV4_QUERY        20   /* RFC7341 */
#define KD6_DHCPV4_RESPONSE     21   /* RFC7341 */



/* Define the timeout for waiting for a DHCPv6PD reply */
#define KD6_SEND_RETRIES  3 /* Send six requests per open */
#define KD6_BASE_TIMEOUT (HZ*2) /* Initial timeout: 2 seconds */
#define KD6_TIMEOUT_RANDOM (HZ) /* Maximum amount of randomization */
#define KD6_TIMEOUT_MAX (HZ*30) /* Maximum allowed timeout */
#define KD6_TIMEOUT_MULT *7/4 /* Rate of timeout growth */
#define KD6_CARRIER_TIMEOUT 120000 /* Wait for carrier timeout */
#define KD6_POST_OPEN  10 /* After opening: 10 msecs */
#define KD6_OPEN_RETRIES  1 /* (Re)open devices twice */
#define KD6_DEVICE_WAIT_MAX  12 /* 12 seconds */

static int kd6_msgtype = NULL ; /* DHCP msg type received */
static struct kd6_device *kd6_first_dev; /* List of opened devices */
static struct task_struct *thread1_NDP;
static volatile int kd6_got_reply ;    /* Proto(s) that replied */
static struct in6_addr kd6_servaddr; /* Boot server IP address */
static struct in6_addr kd6_gateway; /* Gateway IP address */
static struct in6_addr dhcp6_myaddr;  /* My IP address */
static int kd6_proto_have_if ;
static char kd6_user_dev_name[IFNAMSIZ] ;
static DEFINE_SPINLOCK(kd6_recv_lock);
static u8 kd6_servaddr_hw[6];
struct in6_addr KD6_LINK_LOCAL_MULTICAST = {{{ 0xff,02,0,0,0,0,0,0,0,0,0,0,0,1,0,2 }}};
struct in6_addr KD6_LINK_LOCAL = {{{ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 }}};
struct in6_addr KD6_LINK_NULL = {{{ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 }}};



struct dhcpv6_server_id{
	u16 option_client_id;
	u16 option_len;
	u16 duid_type;
	u16 hw_type;
	u32 duid_time;
	u8 my_hw_addr[6];
}__attribute__((packed)) kd6_global_server_id;



struct dhcpv6_client_id{
	u16 option_client_id;
	u16 option_len;
	u16 duid_type;
	u16 hw_type;
	u32 duid_time;
	u8 my_hw_addr[6];
}__attribute__((packed));


struct dhcpv6_time{
	u16 option_time;
	u16 option_len;
	u16 value;
};

struct dhcpv6_oro{
	u16 option;
	u16 option_len;
	u8 value[4];
};

struct dhcpv6_ia_prefix{
	u16 option_prefix;
	u16 option_len;
	u32 prefered_lifetime;
	u32 valid_lifetime;
	u8 prefix_len;
	u8 prefix_addr[16];

}__attribute__((packed)) kd6_global_ia_prefix;


struct dhcpv6_ia_pd{
	u16 option_ia_pd;
	u16 option_len;
	u8 iaid[4];
	u8 t1[4];
	u8 t2[4];
	//	struct dhcpv6_ia_prefix ia_prefix
};

struct dhcpv6_packet_sol {
	u8 msg_type;
	u8 transaction_id[3];
	struct dhcpv6_client_id my_client_id;
	struct dhcpv6_oro oro;
	struct dhcpv6_time time;
	struct dhcpv6_ia_pd ia_pd;        
};
struct dhcpv6_packet_req {
	u8 msg_type;
	u8 transaction_id[3];
	struct dhcpv6_client_id my_client_id;
	struct dhcpv6_client_id my_server_id;
	struct dhcpv6_oro oro;
	struct dhcpv6_time time;
	struct dhcpv6_ia_pd ia_pd;
	struct dhcpv6_ia_prefix ia_prefix;	
};

struct dhcpv6_packet{
	struct dhcpv6_packet_sol *kd6_sol;
	struct dhcpv6_packet_req *kd6_req;
};


/*
 * Network devices
 */
struct kd6_device{
	struct kd6_device *next;
	struct net_device *dev;
	short able;
	u8 xid[3];
};



static struct kd6_device *kd6_first_dev ; /* List of open device */
static struct kd6_device *kd6_dev ;  /* Selected device */
static unsigned int kd6_rcv_pkt (
		void *priv,
		struct sk_buff *skb,
		const struct nf_hook_state *state
		);

static struct nf_hook_ops my_hook = {
	.hook = kd6_rcv_pkt,
	//.pf = NFPROTO_IPV6,
	.pf = PF_INET6,
	.hooknum = (1 << NF_INET_PRE_ROUTING),
	.priority = NF_IP6_PRI_FIRST,
};



static bool  kd6_is_init_dev(struct net_device *dev)
{
	if (dev->flags & IFF_LOOPBACK)
		return false;
	return kd6_user_dev_name[0] ? !strcmp(dev->name, kd6_user_dev_name) :
		(!(dev->flags & IFF_LOOPBACK) &&
		 (dev->flags & (IFF_POINTOPOINT|IFF_BROADCAST)) &&
		 strncmp(dev->name, "dummy", 5));
}



static int kd6_parse_received(u8 *kd6_packet, u8 len){
	u8 pointer=0;
	u16 kd6_option;
	u16 dns_size;
	u16 domain_list;

	struct dhcpv6_client_id *kd6_server_id;
	struct dhcpv6_client_id *kd6_client_id;
	struct dhcpv6_ia_pd *kd6_ia_pd;
	struct dhcpv6_ia_prefix *kd6_ia_prefix;
	kd6_ia_pd = kmalloc (sizeof (struct dhcpv6_ia_pd),GFP_KERNEL);
	kd6_ia_prefix = kmalloc (sizeof (struct dhcpv6_ia_prefix),GFP_KERNEL);
	kd6_client_id = kmalloc (sizeof (struct dhcpv6_client_id),GFP_KERNEL);
	kd6_server_id = kmalloc (sizeof (struct dhcpv6_client_id),GFP_KERNEL);

	kd6_packet+=4; //first option
	while (pointer<len){
		memcpy(&kd6_option,kd6_packet+pointer, sizeof(u16));
		pr_debug("KD6: kd6_op_ntohs %d",ntohs(kd6_option));
		switch (ntohs(kd6_option)){
			case 1:	// Client identifier
				memcpy(kd6_client_id, kd6_packet+pointer, sizeof (struct dhcpv6_client_id));
				pointer+=sizeof(struct dhcpv6_client_id);
				break;
			case 2:	// Server identifier
				memcpy(kd6_server_id, kd6_packet+pointer, sizeof (struct dhcpv6_client_id));
				pointer+=sizeof(struct dhcpv6_client_id);
				break;
			case 23:	// DNS recursive nameserver
				memcpy(&dns_size,kd6_packet+pointer+2,sizeof (u16));
				pointer+=ntohs(dns_size)+4;
				break;
			case 24:	// Domain search list
				memcpy(&domain_list,kd6_packet+pointer+2,sizeof (u16));
				pointer+=ntohs(domain_list)+4;
				break;
			case 25:	// IA PD
				memcpy(kd6_ia_pd,kd6_packet+pointer,sizeof(struct dhcpv6_ia_pd));
				pointer+=sizeof(struct dhcpv6_ia_pd);
				memcpy(kd6_ia_prefix,kd6_packet+pointer,sizeof(struct dhcpv6_ia_prefix));
				pointer+=sizeof(struct dhcpv6_ia_prefix);
				break;
			default:
				pr_err("KD6: ERROR");
				pr_info("KD6: DEFAULT: %x",kd6_option);
				pr_info("KD: pointer: %d", pointer);
				goto ex;
		}
	}

	memcpy(&kd6_global_server_id,kd6_server_id,sizeof(struct dhcpv6_server_id));
	memcpy(&kd6_global_ia_prefix,kd6_ia_prefix,sizeof(struct dhcpv6_ia_prefix));
	memcpy(kd6_servaddr_hw,kd6_server_id->my_hw_addr,sizeof(kd6_servaddr_hw)); 
ex:
	return 0;
}

/*
 *  Receive DHCPv6 reply.
 */

static unsigned int kd6_rcv_pkt (
		void *priv,
		struct sk_buff *skb,
		const struct nf_hook_state *state
		)
{

	pr_debug("KD6: Received DHCP packet");

	struct ipv6hdr *h;
	struct kd6_device *d;
	int len, ext_len;

	struct udphdr *udph;
	struct ipv6hdr *ipv6h;
	struct ethhdr *llh;


	//if (!net_eq(dev_net(dev), &init_net))
	//goto drop;

	if (skb->pkt_type == PACKET_OTHERHOST){
		goto drop;
	}

	skb = skb_share_check(skb, GFP_ATOMIC);
	if (!skb)
		return NET_RX_DROP;


	pr_info ();
	if (!pskb_may_pull(skb,
				sizeof(struct ipv6hdr) +
				sizeof(struct udphdr))){
		goto drop;
	}

	ipv6h = (struct ipv6hdr*) skb_network_header(skb);
	udph = (struct udphdr*) skb_transport_header(skb);

	if (udph->source != htons(547) || udph->dest != htons(546)){
		goto drop;
	}
	// Ok the front looks good, make sure we can get at the rest.  
	if (!pskb_may_pull(skb, skb->len))
		goto drop;



	// One reply at a time, please. 
	spin_lock(&kd6_recv_lock);
	// If we already have a reply, just drop the packet 
	if (kd6_got_reply){
		pr_info("DROP: already_get_reply");
		goto drop_unlock;
	}
	// Find the kd6_device that the packet arrived on 
	d = kd6_first_dev;

	u8 dhcpv6_size = 
		skb->len -
		(sizeof(struct ipv6hdr)+
		 sizeof(struct udphdr)+
		 4);//message type + transaction id

	long dh6_offset = sizeof (struct udphdr);
	u8 *dhp;
	dhp = (u8 *) udph + dh6_offset;
	dhp+=1; //Transaction ID pointer

	u8 rx_xid[3];
	memcpy(rx_xid,dhp,3*sizeof(u8));
       if (memcmp(rx_xid, d->xid,3) != 0){                                                                                         
	       net_err_ratelimited("KD6: Reply not for us on %s, ,rx_xid[%x%x%x],internal_xid [%x%x%x]\n",    
                                  d->dev->name, rx_xid[0],rx_xid[1],rx_xid[2],d->xid[0],d->xid[1],d->xid[2]);
                  goto drop_unlock;   
          }              

	// pr_info("KD6: Got message type %x%d (%s)\n", message_t[0],message_t[1], d->dev->name);
	u8 msg_type;
	memcpy(&msg_type,dhp-=1,sizeof(u8));

	switch (msg_type) {
		case KD6_ADVERTISE:
			//if (memcmp(&kd6_global_ia_prefix.prefix_addr,&LINK_NULL,sizeof(dhcp6_myaddr)))
			// goto drop_unlock;

			kd6_parse_received(dhp,dhcpv6_size);
			break;

		case KD6_REPLY:
			kd6_parse_received(dhp, dhcpv6_size);

			//if (memcmp(dev->dev_addr, kd6_servaddr_hw, dev->addr_len) != 0)
			// goto drop_unlock;
			pr_info("KD6: IPv6 GUNPs offered  %pI64, by server %pI64\n",
					&(kd6_global_ia_prefix.prefix_addr), ipv6h->saddr.in6_u.u6_addr16);

			memcpy (&kd6_servaddr,&ipv6h->saddr,sizeof(kd6_servaddr));
			kd6_got_reply = 1;
			//kd6_dev = d->dev;
			break;

		default:
			//  Forget it/
			dhcp6_myaddr=KD6_LINK_NULL;
			memset (&kd6_servaddr,0,sizeof(kd6_servaddr));
			goto drop_unlock;
	}

	kd6_msgtype = msg_type;
	kd6_dev = d;

drop_unlock:
	/* Show's over.  Nothing to see here.  */
	spin_unlock(&kd6_recv_lock);

drop:
	/* Throw the packet out. */

	// kfree_skb(skb);
	return 0;
}

int GetMon (const char *str){
	const char * month_names[] = {
		"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug",
		"Sep", "Oct", "Nov", "Dec", NULL };
	int i = 0;
	while (i < 12) {
		if (strcasecmp(month_names[i],str) == 0)
			break;
		++i;
	}
	if (i == 12) {
		return 0;
	} else {
		return i + 1; 
	}
}



u32 convertTimeDateToSeconds(const struct tm date){
	u32 y;
	u32 m;
	u32 d;
	u32 t;

	//Year
	y = date.tm_year-2000;
	//Month of year
	m = date.tm_mon-1;
	//Day of month
	d = date.tm_mday-2;

	//January and February are counted as months 13 and 14 of the previous year
	if(m <= 2)
	{
		m += 12;
		y -= 1;
	}
	//Convert years to days
	t = (365 * y) + (y / 4) - (y / 100) + (y / 400);
	//Convert months to days
	t += (30 * m) + (3 * (m + 1) / 5) + d;
	//Convert days to seconds
	t *= 86400;
	//Add hours, minutes and seconds
	t += (3600 * (date.tm_hour-1)) + (60 * date.tm_min) + date.tm_sec;

	//Return Unix time
	return t;
}

static void kd6_options_send_if(u8 msg_type,struct dhcpv6_packet dhp, struct kd6_device *d,unsigned long jiffies_diff){
	struct tm dh6_ktime = {0}; 
	char ktime_month[3];
	char * ver;
	long kernelCompilationTimeStartingFrom2000;
	u32 duid_time;
	u8  val_time[4] = {0x00,0x17,0x00,0x18};
	u8 t1[4]={0x00,0x00,0x0e,0x10};
	u8 t2[4]={0x00,0x00,0x15,0x18};
	u16 msecs_htons;

	switch (msg_type){
		case KD6_SOLICIT:
			memset (dhp.kd6_sol, 0, sizeof(dhp.kd6_sol)); 
			dhp.kd6_sol->msg_type = KD6_SOLICIT; 
			//get_random_bytes ( xid, sizeof(3) );
			//memcpy(dhp->transaction_id,&d->xid,sizeof(3));
			memcpy(dhp.kd6_sol->transaction_id,d->xid,sizeof(d->xid));

			//client id option
			dhp.kd6_sol->my_client_id.option_client_id = htons(1);
			dhp.kd6_sol->my_client_id.option_len = htons(14);
			dhp.kd6_sol->my_client_id.duid_type = htons(1);
			dhp.kd6_sol->my_client_id.hw_type = htons(1);
			//DUID time convert kernel vertsion to compatible option value 
			ver = utsname()->version;
			sscanf(ver, "%*s %*s %*s %*s %s %d %d:%d:%d %*s %d",ktime_month, &dh6_ktime.tm_mday, &dh6_ktime.tm_hour, &dh6_ktime.tm_min, &dh6_ktime.tm_sec, &dh6_ktime.tm_year);
			dh6_ktime.tm_mon=GetMon(ktime_month);
			//pr_info( "EP KTIME: %d,%d,%d,%d,%d,%d", dh6_ktime.tm_mon, dh6_ktime.tm_mday, dh6_ktime.tm_hour, dh6_ktime.tm_min, dh6_ktime.tm_sec, dh6_ktime.tm_year);
			kernelCompilationTimeStartingFrom2000 = convertTimeDateToSeconds(dh6_ktime);
			duid_time=htonl( kernelCompilationTimeStartingFrom2000 & 0xffffffff);
			memcpy (&(dhp.kd6_sol->my_client_id.duid_time),&duid_time,sizeof(duid_time));
			memcpy (dhp.kd6_sol->my_client_id.my_hw_addr, d->dev->dev_addr, 48);


			//struct timeval ktime;
			//do_gettimeofday(&ktime);
			//u32 sec = time.tv_sec;
			//sec-=946684800;
			//sec= sec & 0xffffffff;
			//pr_info ("FULL_TIME: %d", sec);
			//sec = htonl(sec); 
			//memcpy (&(dhp->client_id.duid_time),&sec,sizeof(sec));



			//oro option
			dhp.kd6_sol->oro.option = htons(6);
			dhp.kd6_sol->oro.option_len = htons(4);
			memcpy(dhp.kd6_sol->oro.value,val_time,sizeof(val_time));


			//time option
			dhp.kd6_sol->time.option_time = htons(8);
			dhp.kd6_sol->time.option_len = htons(2);
			msecs_htons = htons(jiffies_to_msecs(jiffies_diff));
			memcpy(&(dhp.kd6_sol->time.value),&msecs_htons,sizeof(msecs_htons));


			//ia pd option
			dhp.kd6_sol->ia_pd.option_ia_pd = htons(0x19);
			dhp.kd6_sol->ia_pd.option_len = htons(0x0c);
			dhp.kd6_sol->ia_pd.iaid[1]=d->dev->dev_addr[3];
			dhp.kd6_sol->ia_pd.iaid[2]=d->dev->dev_addr[4];
			dhp.kd6_sol->ia_pd.iaid[3]=d->dev->dev_addr[5];
			dhp.kd6_sol->ia_pd.iaid[4]=d->dev->dev_addr[6];
			memcpy(dhp.kd6_sol->ia_pd.t1,t1,sizeof(t1));
			memcpy(dhp.kd6_sol->ia_pd.t2,t2,sizeof(t2));
			/*
			   dhp->ia_pd.ia_prefix.option_prefix = htons(0x1a);
			// dhp->ia_pd.ia_prefix.option_prefix = htons(26);
			inehp->ia_pd.ia_prefix.option_len = htons(0x19);
			dhp->ia_pd.ia_prefix.prefered_lifetime=htons(0x00001c20);
			dhp->ia_pd.ia_prefix.valid_lifetime=htons(0x00001d4c);
			dhp->ia_pd.ia_prefix.prefix_len=56;
			*/
			break;
		case KD6_REQUEST:

			dhp.kd6_req->msg_type = KD6_REQUEST; 
			memcpy(dhp.kd6_req->transaction_id,d->xid,sizeof(d->xid));

			//client id option
			dhp.kd6_req->my_client_id.option_client_id = htons(1);
			dhp.kd6_req->my_client_id.option_len = htons(14);
			dhp.kd6_req->my_client_id.duid_type = htons(1);
			dhp.kd6_req->my_client_id.hw_type = htons(1);
			//DUID time convert kernel vertsion to compatible option value 
			ver = utsname()->version;
			sscanf(ver, "%*s %*s %*s %*s %s %d %d:%d:%d %*s %d",ktime_month, &dh6_ktime.tm_mday, &dh6_ktime.tm_hour, &dh6_ktime.tm_min, &dh6_ktime.tm_sec, &dh6_ktime.tm_year);
			dh6_ktime.tm_mon=GetMon(ktime_month);
			kernelCompilationTimeStartingFrom2000 = convertTimeDateToSeconds(dh6_ktime);
			duid_time=htonl( kernelCompilationTimeStartingFrom2000 & 0xffffffff);
			memcpy (&(dhp.kd6_req->my_client_id.duid_time),&duid_time,sizeof(duid_time));
			memcpy (dhp.kd6_req->my_client_id.my_hw_addr, d->dev->dev_addr, 48);

			//server id option
			memcpy (&(dhp.kd6_req->my_server_id),&kd6_global_server_id ,sizeof(dhp.kd6_req->my_server_id));

			//oro option
			dhp.kd6_req->oro.option = htons(6);
			dhp.kd6_req->oro.option_len = htons(4);
			memcpy(dhp.kd6_req->oro.value,val_time,sizeof(val_time));


			//time option
			dhp.kd6_req->time.option_time = htons(8);
			dhp.kd6_req->time.option_len = htons(2);
			msecs_htons = htons(jiffies_to_msecs(jiffies_diff));
			memcpy(&(dhp.kd6_req->time.value),&msecs_htons,sizeof(msecs_htons));


			//ia pd option
			dhp.kd6_req->ia_pd.option_ia_pd = htons(0x19);
			dhp.kd6_req->ia_pd.option_len = htons(0x0c);
			dhp.kd6_req->ia_pd.iaid[1]=d->dev->dev_addr[3];
			dhp.kd6_req->ia_pd.iaid[2]=d->dev->dev_addr[4];
			dhp.kd6_req->ia_pd.iaid[3]=d->dev->dev_addr[5];
			dhp.kd6_req->ia_pd.iaid[4]=d->dev->dev_addr[6];
			memcpy(dhp.kd6_req->ia_pd.t1,t1,sizeof(t1));
			memcpy(dhp.kd6_req->ia_pd.t2,t2,sizeof(t2));

			//ia pd prefix
			memcpy (&(dhp.kd6_req->ia_prefix),&kd6_global_ia_prefix ,sizeof(dhp.kd6_req->ia_prefix));

			break;
	}
}

static void kd6_send_if(struct kd6_device *d, unsigned long jiffies_diff)
{
	struct net_device *dev = d->dev;
	struct sk_buff *skb;
	struct ipv6hdr *ipv6h;
	struct ethhdr *ethh;
	struct udphdr *udph;
	//struct dhcpv6_packet_sol *kd6_pkt_sol;
	//struct dhcpv6_packet_req *kd6_pkt_req;
	struct dhcpv6_packet kd6_pkt_func;
	int hlen = LL_RESERVED_SPACE(dev);
	int tlen = dev->needed_tailroom;

	memset (&kd6_pkt_func, 0, sizeof(kd6_pkt_func)); 
	kd6_pkt_func.kd6_sol = kmalloc (sizeof (struct dhcpv6_packet_sol),GFP_KERNEL); 
	kd6_pkt_func.kd6_req = kmalloc (sizeof (struct dhcpv6_packet_req),GFP_KERNEL); 

	/* Allocate packet */
	if (kd6_msgtype == NULL)
		skb = alloc_skb(sizeof(struct ethhdr) + 
				sizeof(struct udphdr) + 
				sizeof(struct ipv6hdr) + 
				sizeof(struct dhcpv6_packet_sol), GFP_KERNEL);
	else if (kd6_msgtype == KD6_ADVERTISE)
		skb = alloc_skb(sizeof(struct ethhdr) + 
				sizeof(struct udphdr) + 
				sizeof(struct ipv6hdr) + 
				sizeof(struct dhcpv6_packet_req), GFP_KERNEL);
	else 
		pr_err("KD6:Error-unsupported msgtype");
	if (!skb)
		return;
	memset (skb, 0, sizeof(skb));

	skb->dev = dev;
	skb->pkt_type = PACKET_OUTGOING;
	skb->protocol = htons(ETH_P_IPV6);
	skb->no_fcs = 1;  

	skb_reserve(skb, sizeof (struct ethhdr) + sizeof(struct udphdr)+ sizeof(struct ipv6hdr));

	if (kd6_msgtype == NULL){
		//dhcpv6
		kd6_pkt_func.kd6_sol = (struct dhcpv6_packet_sol *) skb_put (skb, sizeof (struct dhcpv6_packet_sol));
		kd6_options_send_if(KD6_SOLICIT,kd6_pkt_func,d,jiffies_diff);
		//udp
		ipv6_dev_get_saddr(&init_net, d->dev, &KD6_LINK_LOCAL_MULTICAST, 0, &KD6_LINK_LOCAL); 
		udph = (struct udphdr *) skb_push (skb, sizeof (struct udphdr));
		udph->source = htons(546);
		udph->dest = htons(547);
		udph->len = htons(sizeof(struct udphdr)+sizeof(struct dhcpv6_packet_sol));
		udph->check = 0;
		__wsum csum = csum_partial((char *) udph, sizeof(struct udphdr)+sizeof(struct dhcpv6_packet_sol),0);
		udph->check = csum_ipv6_magic(&KD6_LINK_LOCAL, &KD6_LINK_LOCAL_MULTICAST, sizeof(struct udphdr)+sizeof(struct dhcpv6_packet_sol),IPPROTO_UDP,csum); 
		//ipv6
		ipv6h = (struct ipv6hdr *) skb_push (skb,sizeof (struct ipv6hdr));
		ipv6h->version = 6;
		ipv6h->nexthdr = IPPROTO_UDP;
		ipv6h->payload_len = htons(sizeof(struct udphdr)+sizeof(struct dhcpv6_packet_sol));
		ipv6h->daddr = KD6_LINK_LOCAL_MULTICAST; 
		ipv6h->saddr = KD6_LINK_LOCAL;
		ipv6h->hop_limit = 255;
	}else if (kd6_msgtype == KD6_ADVERTISE){
		//dhcpv6
		kd6_pkt_func.kd6_req = (struct dhcpv6_packet_req *) skb_put (skb, sizeof (struct dhcpv6_packet_req));
		memset (kd6_pkt_func.kd6_req, 0, sizeof(struct dhcpv6_packet_req)); 
		kd6_options_send_if(KD6_REQUEST,kd6_pkt_func,d,jiffies_diff);
		//udp
		ipv6_dev_get_saddr(&init_net, d->dev, &KD6_LINK_LOCAL_MULTICAST, 0, &KD6_LINK_LOCAL); 
		udph = (struct udphdr *) skb_push (skb, sizeof (struct udphdr));
		udph->source = htons(546);
		udph->dest = htons(547);
		udph->len = htons(sizeof(struct udphdr)+sizeof(struct dhcpv6_packet_req));
		udph->check = 0;
		__wsum csum = csum_partial((char *) udph, sizeof(struct udphdr)+sizeof(struct dhcpv6_packet_req),0);
		udph->check = csum_ipv6_magic(&KD6_LINK_LOCAL, &KD6_LINK_LOCAL_MULTICAST, sizeof(struct udphdr)+sizeof(struct dhcpv6_packet_req),IPPROTO_UDP,csum); 
		//ipv6
		ipv6h = (struct ipv6hdr *) skb_push (skb,sizeof (struct ipv6hdr));
		ipv6h->version = 6;
		ipv6h->nexthdr = IPPROTO_UDP;
		ipv6h->payload_len = htons(sizeof(struct udphdr)+sizeof(struct dhcpv6_packet_req));
		ipv6h->daddr = KD6_LINK_LOCAL_MULTICAST; 
		ipv6h->saddr = KD6_LINK_LOCAL;
		ipv6h->hop_limit = 255;
	}else 
		pr_err("KD6:ERROR-msgtype is not supported");


	ethh = (struct ethhdr *) skb_push (skb, sizeof(struct ethhdr)); 
	ethh->h_proto = htons(ETH_P_IPV6); 
	memcpy(ethh->h_source, d->dev->dev_addr, ETH_ALEN);
	u8 ipv6_my_multicast[6]={0x33,0x33,0x00,0x01,0x00,0x02};
	memcpy (ethh->h_dest, ipv6_my_multicast, sizeof(ipv6_my_multicast));



	//skb->csum=skb_checksum_complete(skb);

	if (dev_queue_xmit(skb) < 0)
		printk("KD6: Error-dev_queue_xmit failed");
}

static inline void  kd6_dhcpv6PD_init(void)
{
	nf_register_net_hook (&init_net,&my_hook);
	return 0;
}


/*
 *  DHCPv6PD cleanup.

 */
static inline void  kd6_dhcpv6PD_cleanup(void)
{
	nf_unregister_net_hook(&init_net, &my_hook);
}





static int  kd6_dhcpv6PD_snd_rcv_sequence (void)
{
	int retries;
	struct kd6_device *d;
	unsigned long start_jiffies, timeout, jiff;


	if ((!kd6_proto_have_if))
		pr_err("KD6: No suitable device found\n");

	if (!kd6_proto_have_if)
		/* Error message already printed */
		return -1;
	kd6_dhcpv6PD_init();
	/*
	 * Setup protocols
	 */
	/*
	 * Send requests and wait, until we get an answer. This loop
	 * seems to be a terrible waste of CPU time, but actually there is
	 * only one process running at all, so we don't need to use any
	 * scheduler functions.
	 * [Actually we could now, but the nothing else running note still
	 *  applies.. - AC]
	 */
	pr_notice("Sending DHCPv6_PD requests .");
	start_jiffies = jiffies;
	d = kd6_first_dev;
	retries = KD6_SEND_RETRIES;
	get_random_bytes(&timeout, sizeof(timeout));
	timeout = KD6_BASE_TIMEOUT + (timeout % (unsigned int) KD6_TIMEOUT_RANDOM);

	for (;;) {
		if ((d->able ))
			pr_debug ("KD6: send dhcpv6 on %s", d->dev);

		kd6_send_if(d, jiffies - start_jiffies);

		if (!d->next) {
			jiff = jiffies + timeout;
			while (time_before(jiffies, jiff) && !kd6_got_reply)
				schedule_timeout_uninterruptible(1);
		}
		/* DHCP isn't done until we get a DHCPACK. */
		if ((kd6_got_reply) &&	kd6_msgtype != KD6_REPLY) {
			kd6_got_reply = 0;
			/* continue on device that got the reply */
			d = kd6_dev;
			pr_cont(",");
			continue;
		}

		if (kd6_got_reply) {
			pr_cont(" OK\n");
			break;
		}

		if ((d = d->next))
			continue;

		if (!(--retries)) {
			pr_cont(" timed out!\n");
			break;
		}

		d = kd6_first_dev;

		timeout = timeout  KD6_TIMEOUT_MULT;
		if (timeout > KD6_TIMEOUT_MAX)
			timeout = KD6_TIMEOUT_MAX;

		pr_cont(".");
	}

	kd6_dhcpv6PD_cleanup();

	if (!kd6_got_reply) {
		dhcp6_myaddr = KD6_LINK_NULL;
		return -1;
	}

	pr_info("KD6: Got DHCPv6 REPLY from %pI64, the IPv6 GUNPs offered: %pI64\n",
			&(kd6_servaddr), &(kd6_global_ia_prefix.prefix_addr) );

	return 0;
}




static int  kd6_wait_for_devices(void)
{
	int i;

	for (i = 0; i < KD6_DEVICE_WAIT_MAX; i++) {
		struct net_device *dev;
		int found = 0;

		rtnl_lock();
		for_each_netdev(&init_net, dev) {
			if (kd6_is_init_dev(dev)) {
				found = 1;
				break;
			}
		}
		rtnl_unlock();
		if (found)
			return 0;
		ssleep(1);
	}
	return -ENODEV;
}



static int  kd6_open_devs(void)
{
	struct kd6_device *d, **last;
	struct net_device *dev;
	unsigned short oflags;
	unsigned long start, next_msg;

	last = &kd6_first_dev;
	rtnl_lock();

	/* bring loopback and DSA master network devices up first */
	for_each_netdev(&init_net, dev) {
		pr_info ("KD6: Inspecting dev %s", dev);
		if (!(dev->flags & IFF_LOOPBACK) && !netdev_uses_dsa(dev))
			continue;
		if (dev_change_flags(dev, dev->flags | IFF_UP) < 0)
			pr_err("KD6: Failed to open %s\n", dev->name);
	}

	for_each_netdev(&init_net, dev) {
		if (kd6_is_init_dev(dev)) {

			int able = 0;
			if (dev->mtu >= 364){
				able = 1;
				pr_info("KD6: Device %s is suitable to send", dev);
			}
			else
				pr_warn("KD6: Ignoring device %s, MTU %d too small\n",
						dev->name, dev->mtu);

			if (!able){
				pr_warn(" KD6: dev %s is not able", dev);
				continue;
			}


			oflags = dev->flags;
			if (dev_change_flags(dev, oflags | IFF_UP) < 0) {
				pr_err("KD6: Failed to open %s\n",
						dev->name);
				continue;
			}
			if (!(d = kmalloc(sizeof(struct kd6_device), GFP_KERNEL))) {
				rtnl_unlock();
				return -ENOMEM;
			}
			d->dev = dev;
			*last = d;
			last = &d->next;
			//d->flags = oflags;
			//   d->able = able;
			if (able )
				//get_random_bytes(&d->xid, 4*sizeof(u8));
				get_random_bytes(d->xid, sizeof(d->xid));
			kd6_proto_have_if |= able;
			//pr_debug("KD6: %s UP (able=%d, xid=%08x)\n",
			//  dev->name, able, d->xid);
		}
	}

	/* no point in waiting if we could not bring up at least one device */
	if (!kd6_first_dev)
		goto have_carrier;

	/* wait for a carrier on at least one device */
	start = jiffies;
	next_msg = start + msecs_to_jiffies(KD6_CARRIER_TIMEOUT/12);
	while (time_before(jiffies, start +
				msecs_to_jiffies(KD6_CARRIER_TIMEOUT))) {
		int wait, elapsed;

		for_each_netdev(&init_net, dev)
			if (kd6_is_init_dev(dev) && netif_carrier_ok(dev))
				goto have_carrier;

		if (time_before(jiffies, next_msg))
			continue;

		elapsed = jiffies_to_msecs(jiffies - start);
		wait = (KD6_CARRIER_TIMEOUT - elapsed + 500)/1000;
		pr_info("Waiting up to %d more seconds for network.\n", wait);
		next_msg = jiffies + msecs_to_jiffies(KD6_CARRIER_TIMEOUT/12);
	}
have_carrier:
	rtnl_unlock();

	*last = NULL;

	if (!kd6_first_dev) {
		if (kd6_user_dev_name[0])
			pr_err("KD6: Device `%s' not found\n",
					kd6_user_dev_name);
		else
			pr_err("KD6: No network devices available\n");
		return -ENODEV;
	}
	return 0;
}




static void kd6_close_devs(void)
{
	struct kd6_device *d, *next;
	struct net_device *dev;

	rtnl_lock();
	next = kd6_first_dev;
	while ((d = next)) {
		next = d->next;
		dev = d->dev;
		if (d != kd6_dev && !netdev_uses_dsa(dev)) {
			pr_debug("KD6: Downing %s\n", dev->name);
			//dev_change_flags(dev, d->flags);
		}
		kfree(d);
	}
	rtnl_unlock();
}



static int kd6_setup_def_route(void){
	struct fib6_info *rt = NULL;
	//need to setup default route
	rt = rt6_add_dflt_router(&init_net, &kd6_servaddr, kd6_dev->dev, ICMPV6_ROUTER_PREF_MEDIUM);
	if (!rt) {
		pr_info("KD6: failed to add default route\n");
		return 0;
	}
	if (rt)
		//set infinite timeout
		fib6_clean_expires(rt);
	//set finite timeout
	//fib6_set_expires(rt, jiffies + msecs_to_jiffies(ntohl(kd6_global_ia_prefix.valid_lifetime)*1000));
}
kd6_setup_routes(struct net_device *dev,struct prefix_info *mypinfo){
	/*
	 *This code is attemp to use different fucntion to configure the route. May be usefull as a tip.
	 */

	/*
	   unsigned int pref = ICMPV6_ROUTER_PREF_MEDIUM;
	   int max_len_rt_info = 999;
	   struct route_info rt_info;
	   rt_info.length = 3;
	   rt_info.prefix_len = mypinfo->prefix_len;
	   memcpy(&(rt_info.prefix), &(kd6_global_ia_prefix.prefix_addr),sizeof(rt_info.prefix));
	   */
	/*
	   rt6_add_route_info( &init_net,
	   mypinfo->prefix, 
	   mypinfo->prefix_len,
	//const struct in6_addr *gwaddr,
	NULL,
	dev,
	pref);
	*/
	//rt6_route_rcv(dev, (u8 *)&rt_info, max_len_rt_info, NULL);
	//u32 flags
	/*
	   u32 metric = 1024;
	   unsigned long expires = 4000;
	   int plen = mypinfo->prefix_len;
	   struct in6_addr pfx;

	   memcpy (&pfx, &(kd6_global_ia_prefix.prefix_addr),sizeof (struct in6_addr));

	   struct fib6_config cfg = {
	   .fc_table = l3mdev_fib_table(dev) ? : RT6_TABLE_PREFIX,
	   .fc_metric = metric ? : IP6_RT_PRIO_ADDRCONF,
	   .fc_ifindex = dev->ifindex,
	   .fc_expires = expires,
	   .fc_dst_len = plen,
	   .fc_flags = RTF_UP,
	   .fc_nlinfo.nl_net = dev_net(dev),
	   .fc_protocol = RTPROT_KERNEL,
	   .fc_type = RTN_UNICAST,
	   };

	   cfg.fc_dst = pfx;

	// ip6_route_add(&cfg, GFP_KERNEL, NULL);
	*/
}


static int kd6_setup_if(void){
	struct kd6_device *d, *next;
	struct net_device *dev;

	struct prefix_info *pinfo;
	struct inet6_dev *in6_dev;
	struct in6_addr *kd6_setupif_addr;

	int addr_type = 1; //NET_ADDR_IP6 
	u32 addr_flags;
	bool sllao;
	bool tokenized;
	int i=0;
	u8 *eui;	 
	long subprefix_dec;
	char *subprefix_dec_s;
	long subprefix_hex;
	char *subprefix_hex_s;
	subprefix_dec_s=kmalloc (2*sizeof (char),GFP_KERNEL);
	subprefix_hex_s=kmalloc (2*sizeof (char),GFP_KERNEL);
	eui = kmalloc (sizeof (u8),GFP_KERNEL);
	pinfo = kmalloc (sizeof (struct prefix_info),GFP_KERNEL);
	kd6_setupif_addr = kmalloc (sizeof (struct in6_addr),GFP_KERNEL);


	pinfo->prefix_len = 64;//kd6_global_ia_prefix.prefix_len;
	pinfo->valid = (kd6_global_ia_prefix.valid_lifetime);
	//pr_info ("valid_lifetime %d \n",pinfo->valid); 
	pinfo->prefered = (kd6_global_ia_prefix.prefered_lifetime);
	//pr_info("preffered_lifetime %d \n",pinfo->prefered);
	pinfo->onlink = 1;
	pinfo->autoconf = 1; 

	memcpy(&(pinfo->prefix.in6_u.u6_addr8),&(kd6_global_ia_prefix.prefix_addr),sizeof(pinfo->prefix.in6_u.u6_addr8));
	memcpy(&(pinfo->prefix.in6_u.u6_addr16),&(kd6_global_ia_prefix.prefix_addr),sizeof(pinfo->prefix.in6_u.u6_addr16));
	memcpy(&(pinfo->prefix.in6_u.u6_addr32),&(kd6_global_ia_prefix.prefix_addr),sizeof(pinfo->prefix.in6_u.u6_addr32));

	rtnl_lock();
	next = kd6_first_dev;
	while ((d = next)) {
		next = d->next;
		dev = d->dev;




		//This implemetation supports at least 15 ports as Proof of the Concept. 
		//Also the motivation for 8 bits: because most of the DHCP_PD servers will advertise
		//the pool compatible with current code.
		//
		//
		//Here we take the last 8 bits of the prefix and 
		//assign subprefixes {1,2,3,..} to the interfaces
		//
		subprefix_hex=pinfo->prefix.in6_u.u6_addr8[7];
		//pr_info ("subprefix is: %x",subprefix_hex);
		sprintf(subprefix_dec_s,"%d",subprefix_hex);
		//pr_info("subprefix_dec str %s", subprefix_dec_s);
		kstrtol(subprefix_dec_s,10,&subprefix_dec);
		subprefix_dec=subprefix_dec+1;
		//pr_info("subprefix_dec %d", subprefix_dec);
		sprintf(subprefix_hex_s,"%x",subprefix_dec);
		kstrtol(subprefix_hex_s,16,&subprefix_hex);
		//pr_info("subprefix_hex %x",subprefix_hex);
		pinfo->prefix.in6_u.u6_addr8[7]=subprefix_hex;

		pr_info("assigning to dev %s prefix %pI64 \n",dev,pinfo->prefix.in6_u.u6_addr32);  


		addrconf_prefix_rcv(dev, (u8 *)pinfo, jiffies + msecs_to_jiffies(999*1000), sllao); 

		//kd6_setup_routes(dev, pinfo);
		//in6_dev = in6_dev_get(dev);
		/*
		   memcpy(&(kd6_setupif_addr->in6_u.u6_addr8),&(kd6_global_ia_prefix.prefix_addr),sizeof(kd6_setupif_addr->in6_u.u6_addr8));
		   memcpy(&(kd6_setupif_addr->in6_u.u6_addr16),&(kd6_global_ia_prefix.prefix_addr),sizeof(kd6_setupif_addr->in6_u.u6_addr8));
		   memcpy(&(kd6_setupif_addr->in6_u.u6_addr32),&(kd6_global_ia_prefix.prefix_addr),sizeof(kd6_setupif_addr->in6_u.u6_addr8));
		   */

		//  ipv6_inherit_eui64(eui,in6_dev);
		/*
		   addrconf_prefix_rcv_add_addr(&init_net, dev, pinfo, in6_dev,
		   kd6_setupif_addr, addr_type, addr_flags,
		   sllao, tokenized, valid_lft,      prefered_lft);
		   */


	}
	kd6_setup_def_route();

	rtnl_unlock();

}

static int kd6_auto_config(void)
{
	__be32 addr;
	int retries = KD6_OPEN_RETRIES;
	int err;
	unsigned int i;

	pr_info ("KD6: Kernel DHCPv6 Lite initiated");
	for (;;){
		/* Wait for devices to appear */
		err = kd6_wait_for_devices();
		if (err)
			return err;

		/* Setup all network devices */
		err = kd6_open_devs();
		if (err)
			return err;

		/* Give drivers a chance to settle */
		msleep(KD6_POST_OPEN);

		if (memcmp(dhcp6_myaddr.in6_u.u6_addr16, KD6_LINK_NULL.in6_u.u6_addr16,16) ||
				kd6_first_dev->next) {
			if (kd6_dhcpv6PD_snd_rcv_sequence() < 0) {
				kd6_close_devs();

				if (!(--retries)) {
					/* Oh, well.  At least we tried. */
					pr_err("KD6: Auto-configuration of network failed\n");
					return -1;
				}
				pr_err("KD6: Reopening network devices...\n");
			}else
				break;
		} else {
			/* Device selected manually or only one device -> use it */
			kd6_dev = kd6_first_dev;
			break;
		}
	}
	/*
	 * Use defaults wherever applicable.
	 */

	pr_info("KD6: Complete:\n");
	kd6_setup_if();
	kd6_close_devs();

	return err;
}



static int  kd6_nd_open_devs(void)
{
	struct kd6_device *d, **last;
	struct net_device *dev;
	unsigned short oflags;
	unsigned long start, next_msg;

	last = &kd6_first_dev;
	rtnl_lock();

	/* bring loopback and DSA master network devices up first */
	for_each_netdev(&init_net, dev) {
		pr_info ("KD6_ND: Inspecting dev %s", dev);
		if (!(dev->flags & IFF_LOOPBACK) && !netdev_uses_dsa(dev))
			continue;
		if (dev_change_flags(dev, dev->flags | IFF_UP) < 0)
			pr_err("KD6_ND: Failed to open %s\n", dev->name);
	}

	for_each_netdev(&init_net, dev) {
		if (kd6_is_init_dev(dev)) {

			int able = 0;
			if (dev->mtu >= 364){
				able = 1;
			}
			else
				pr_warn("KD6_ND: Ignoring device %s, MTU %d too small\n",
						dev->name, dev->mtu);

			if (!able){
				pr_warn ("dev %s is not able",dev);
				continue;
			}

			if (!(d = kmalloc(sizeof(struct kd6_device), GFP_KERNEL))) {
				rtnl_unlock();
				return -ENOMEM;
			}
			d->dev = dev;
			*last = d;
			last = &d->next;
			pr_debug("KD6_ND: is UP \n");
		}
	}

	/* no point in waiting if we could not bring up at least one device */
	if (!kd6_first_dev)
		goto have_carrier;

	/* wait for a carrier on at least one device */
	start = jiffies;
	next_msg = start + msecs_to_jiffies(KD6_CARRIER_TIMEOUT/12);
	while (time_before(jiffies, start +
				msecs_to_jiffies(KD6_CARRIER_TIMEOUT))) {
		int wait, elapsed;

		for_each_netdev(&init_net, dev)
			if (kd6_is_init_dev(dev) && netif_carrier_ok(dev))
				goto have_carrier;

		if (time_before(jiffies, next_msg))
			continue;

		elapsed = jiffies_to_msecs(jiffies - start);
		wait = (KD6_CARRIER_TIMEOUT - elapsed + 500)/1000;
		pr_info("Waiting up to %d more seconds for network.\n", wait);
		next_msg = jiffies + msecs_to_jiffies(KD6_CARRIER_TIMEOUT/12);
	}
have_carrier:
	rtnl_unlock();

	*last = NULL;

	if (!kd6_first_dev) {
		if (kd6_user_dev_name[0])
			pr_err("KD6_ND: Device `%s' not found\n",
					kd6_user_dev_name);
		else
			pr_err("KD6_ND: No network devices available\n");
		return -ENODEV;
	}
	return 0;
}




static void  kd6_nd_close_devs(void)
{
	struct kd6_device *d, *next;
	struct net_device *dev;

	rtnl_lock();
	next = kd6_first_dev;
	while ((d = next)) {
		next = d->next;
		dev = d->dev;
		if (d != kd6_dev && !netdev_uses_dsa(dev)) {
			pr_debug("KD6: Downing %s\n", dev->name);
		}
		kfree(d);
	}
	rtnl_unlock();
}


static int kd6_nd_network_prefix_prepare_if(void){
	__be32 addr;
	int retries = KD6_OPEN_RETRIES;
	int err;
	unsigned int i;

	pr_debug("KD6_ND_Config: Entered.\n");
	//for (;;){
	/* Wait for devices to appear */
	err = kd6_wait_for_devices();
	if (err)
		return err;

	/* Setup all network devices */
	err = kd6_nd_open_devs();
	if (err)
		return err;

	/* Give drivers a chance to settle */
	msleep(KD6_POST_OPEN);

	return err;
}

struct sk_buff* kd6_nd_network_prefix_generate_payload(struct net_device *dev){
	struct sk_buff *skb;	
	struct ipv6hdr* ipv6h;
	struct ethhdr *ethh;
	struct dhcpv6_packet kd6_pkt_func;
	int hlen = LL_RESERVED_SPACE(dev);
	int tlen = dev->needed_tailroom;
	struct in6_addr LINK_LOCAL_ALL_NODES_MULTICAST = {{{ 0xff,02,0,0,0,0,0,0,0,0,0,0,0,0,0,1 }}};
	struct in6_addr LINK_GLOBAL_UNICAST = {{{ 0x20,0x01,0,0,0,0,0,0,0,0,0,0,0,0,0,0}}};
	struct in6_addr kd6_if_addr_global = {{{ 0, }}};
	struct in6_addr kd6_if_addr_ll = {{{ 0, }}};
	struct icmp6sup_hdr{
		//base icmpv6
		struct icmp6hdr __attribute__((packed)) icmp6h_base;
		u32 reachable_time;
		u32 retransmit_timer;
		//prefix opt
		u8 type_prefix;
		u8 len_prefix_opt;
		u8 prefix_len;
		u8 flag;
		u32 valid_lifetime;
		u32 prefered_lifetime;
		u32 reserved;
		u8 prefix[16];
		//slla opt
		u8 type_slla;
		u8 len; 
		u8 eth_addr[6];
	};
	struct icmp6sup_hdr *icmp6h;

	skb = alloc_skb(sizeof(struct ethhdr) + 
			sizeof(struct ipv6hdr)+
			sizeof(struct icmp6hdr), GFP_KERNEL);
	if (!skb)
		return;
	memset (skb, 0, sizeof(skb));

	skb->dev = dev;
	skb->pkt_type = PACKET_OUTGOING;
	skb->protocol = htons(ETH_P_IPV6);
	skb->no_fcs = 1;  

	skb_reserve(skb, sizeof (struct ethhdr) + sizeof(struct icmp6sup_hdr)+ sizeof(struct ipv6hdr));
	//	ipv6_dev_get_saddr(&init_net, dev, &LINK_LOCAL_ALL_NODES_MULTICAST, 0, &LINK_LOCAL); 


	//icmpv6 base
	icmp6h = (struct icmp6sup_hdr*) skb_push (skb, sizeof (struct icmp6sup_hdr));
	icmp6h->icmp6h_base.icmp6_type = 134; //icmp ra type
	icmp6h->icmp6h_base.icmp6_code = 0;
	icmp6h->icmp6h_base.icmp6_dataun.u_nd_ra.router_pref = 3;
	icmp6h->icmp6h_base.icmp6_dataun.u_nd_ra.hop_limit = 64;
	icmp6h->icmp6h_base.icmp6_dataun.u_nd_ra.rt_lifetime = 86400;
	icmp6h->reachable_time = 0;
	icmp6h->retransmit_timer = 0;


	//icmpv6 slla opt

	icmp6h->type_slla = 1;
	icmp6h->len = 1;
	u8 lladdr[6]; 
	lladdr[0]=0x08;
	lladdr[1]=0x00;
	lladdr[2]=0x27;	
	lladdr[3]=0x5e;
	lladdr[4]=0x28;
	lladdr[5]=0x56;

	memcpy(&(icmp6h->eth_addr),lladdr,sizeof(icmp6h->eth_addr));

	//	memcpy(&(icmp6h->eth_addr),&(dev->dev_addr),sizeof(icmp6h->eth_addr));

	//icmpv6 prefix opt
	icmp6h->type_prefix		= 3;
	icmp6h->len_prefix_opt		= 4;	
	icmp6h->prefix_len 		= 64;
	icmp6h->flag 			= 0xe0;
	icmp6h->valid_lifetime		= htonl(86400);
	icmp6h->prefered_lifetime	= htonl(14400);

	ipv6_dev_get_saddr(&init_net, dev, &LINK_GLOBAL_UNICAST, 0, &kd6_if_addr_global);
	ipv6_dev_get_saddr(&init_net, dev, &LINK_LOCAL_ALL_NODES_MULTICAST, 0, &kd6_if_addr_ll);

	memset(icmp6h->prefix, 0, sizeof(icmp6h->prefix));		
	memcpy(icmp6h->prefix,&kd6_if_addr_global.in6_u.u6_addr16,(sizeof (icmp6h->prefix))/2);
	//        pr_info("FOUND IP ADDRESS ON IF %s :%pI64",dev, kd6_if_addr_global.in6_u.u6_addr16);



	//icmpv6 base
	icmp6h->icmp6h_base.icmp6_cksum = 0;
	__wsum csum = csum_partial((char *) icmp6h, sizeof(struct icmp6sup_hdr),0);
	icmp6h->icmp6h_base.icmp6_cksum = csum_ipv6_magic(&kd6_if_addr_ll, &LINK_LOCAL_ALL_NODES_MULTICAST, 
			sizeof(struct icmp6sup_hdr),IPPROTO_ICMPV6,csum); 

	//ipv6
	ipv6h = (struct ipv6hdr *) skb_push (skb,sizeof (struct ipv6hdr));
	ipv6h->version = 6;
	ipv6h->nexthdr = IPPROTO_ICMPV6;
	ipv6h->payload_len = htons(sizeof(struct icmp6sup_hdr));
	ipv6h->daddr = LINK_LOCAL_ALL_NODES_MULTICAST; 
	ipv6h->saddr= kd6_if_addr_ll;
	ipv6h->hop_limit = 255;

	//eth
	ethh = (struct ethhdr *) skb_push (skb, sizeof(struct ethhdr)); 
	ethh->h_proto = htons(ETH_P_IPV6); 
	memcpy(ethh->h_source, dev->dev_addr, ETH_ALEN);
	u8 ipv6_my_multicast[6]={0x33,0x33,0x00,0x00,0x00,0x01};
	memcpy (ethh->h_dest, ipv6_my_multicast, sizeof(ipv6_my_multicast));
	return skb;
}

static int kd6_nd_network_prefix_send(void){
	struct kd6_device *kd6_dev;
	kd6_dev = kd6_first_dev;
	struct sk_buff *skb;
	for (;;){
		if(kthread_should_stop()) {
			do_exit(0);
		}
		if (kd6_dev->able){

			struct kd6_device *d, *next;
			struct net_device *dev;
			next = kd6_first_dev;
			while ((d = next)) {
				next = d->next;
				dev = d->dev;
				if (dev != kd6_dev->dev){	
					pr_info ("KD6_ND: send RA on %s", dev);
					rtnl_lock();
					skb = kd6_nd_network_prefix_generate_payload(dev);
					if (dev_queue_xmit(skb) < 0)
						pr_err("KD6: Error-dev_queue_xmit failed");
					rtnl_unlock();
				}
			}
			ssleep (30);
		}
	}
	return 0;
}

static int kd6_nd_network_prefix_close_if(void){
	pr_info("KD6_ND: Complete.\n");
	kd6_nd_close_devs();
	return 0;
}


int kd6_nd_advertise_on_link(void* unused){
	kd6_nd_network_prefix_prepare_if();
	kd6_nd_network_prefix_send();	
	return 0;
}

int kd6_NDP_thread_init (void) {
	char  our_thread[11]="thread1_NDP";
	pr_info("KD6_ND: Neigbour Discovery Lite proto: thread init");
	thread1_NDP = kthread_create(kd6_nd_advertise_on_link,NULL,our_thread);
	if((thread1_NDP)){
		printk(KERN_INFO "KD6_ND: thread started");
		wake_up_process(thread1_NDP);
	}
	return 0;
}



void thread_cleanup(void) {
	int ret;
	ret = kthread_stop(thread1_NDP);
	kd6_nd_network_prefix_close_if();
	if(!ret)
		pr_info("KD6_ND: thread stopped");
	else
		pr_err ("KD6_ND: error during stopping the thread, %d",ret);
}


static int  KD6_LKM_init(void){
	printk(KERN_INFO "KernelDhcpv6[KD6] DANIR LKM is started!\n" );
	kd6_auto_config();
	kd6_NDP_thread_init();
	return 0;
}

static void __exit KD6_LKM_exit(void){
	thread_cleanup();
	printk(KERN_INFO "Goodbye from KernelDhcpv6[KD6] DANIR LKM!\n");
}

module_init(KD6_LKM_init);
module_exit(KD6_LKM_exit);
