/* vi: set sw=4 ts=4: */
/*
 * arp.c - Manipulate the system ARP cache
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 *
 * Author: Fred N. van Kempen, <waltje at uwalt.nl.mugnet.org>
 * Busybox port: Paul van Gool <pvangool at mimotech.com>
 *
 * modified for getopt32 by Arne Bernin <arne [at] alamut.de>
 */

#include "libbb.h"
#include "inet_common.h"

#include <arpa/inet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <netinet/ether.h>
#include <netpacket/packet.h>

#define DEBUG 0

#define DFLT_AF "inet"
#define DFLT_HW "ether"

#define	ARP_OPT_A (0x1)
#define	ARP_OPT_p (0x2)
#define	ARP_OPT_H (0x4)
#define	ARP_OPT_t (0x8)
#define	ARP_OPT_i (0x10)
#define	ARP_OPT_a (0x20)
#define	ARP_OPT_d (0x40)
#define	ARP_OPT_n (0x80)	/* do not resolve addresses     */
#define	ARP_OPT_D (0x100)	/* HW-address is devicename     */
#define	ARP_OPT_s (0x200)
#define	ARP_OPT_v (0x400 * DEBUG)	/* debugging output flag        */


/*start of atp add code for arp send mac by ourself by c00126165 2008-7-15*/

#define MAC_TARGET_ADDR	"\x00\x00\x00\x00\x00\x00"
#define MAC_BCAST_ADDR  "\xff\xff\xff\xff\xff\xff"

#define ATP_ARP_MAC_LEN 6
#define ATP_ARP_IP_LEN 4
#define ATP_ARP_PAD_LEN 18
#define ATP_ARP_ARP_DEFAULT_TIME 2
#define ATP_ARP_MAC_SETOFFE 32
#define ATP_ARP_HADDR_SETOFFE 38
#define ATP_ARP_PACKET_BIG 60

struct arpMsg {
	struct ethhdr ethhdr;	 		/* Ethernet header */
	u_short usHtype;				/* hardware type (must be ARPHRD_ETHER) */
	u_short usPtype;				/* protocol type (must be ETH_P_IP) */
	u_char  cHlen;				/* hardware address length (must be 6) */
	u_char  clen;				/* protocol address length (must be 4) */
	u_short usOperation;			/* ARP opcode */
	u_char  ucSHaddr[ATP_ARP_MAC_LEN];			/* sender's hardware address */
	u_char  ucSInaddr[ATP_ARP_IP_LEN];			/* sender's IP address */
	u_char  ucTHaddr[ATP_ARP_MAC_LEN];			/* target's hardware address */
	u_char  ucTInaddr[ATP_ARP_IP_LEN];			/* target's IP address */
	u_char  ucPad[ATP_ARP_PAD_LEN];			/* pad for min. Ethernet payload (60 bytes) */
};

static int atp_arp_init_packet(u_int32_t ulYiaddr,u_int32_t ulIp,char *pszArp,struct arpMsg *pstMsg)
{
    int lPtval = 1;
    int lSock;

   	memset(pstMsg,0,sizeof(*pstMsg));
   	memcpy(pstMsg->ethhdr.h_dest,MAC_BCAST_ADDR,ATP_ARP_MAC_LEN);   /* MAC DA */
	memcpy(pstMsg->ethhdr.h_source,pszArp,ATP_ARP_MAC_LEN);	/* MAC SA */
	pstMsg->ethhdr.h_proto = htons(ETH_P_ARP);	/* protocol type (Ethernet) */
	pstMsg->usHtype = htons(ARPHRD_ETHER);		/* hardware type */
	pstMsg->usPtype = htons(ETH_P_IP);			/* protocol type (ARP message) */
	pstMsg->cHlen = ATP_ARP_MAC_LEN;							/* hardware address length */
	pstMsg->clen = 4;							/* protocol address length */
	pstMsg->usOperation = htons(ARPOP_REQUEST);			/* ARP op code */
	*((u_int *)pstMsg->ucSInaddr) = ulIp;		/* source IP address */
	memcpy( pstMsg->ucSHaddr,pszArp,ATP_ARP_MAC_LEN);			/* source hardware address */
	*((u_int *)pstMsg->ucTInaddr) = ulYiaddr;		/* target IP address */

	if((lSock = socket (AF_INET, SOCK_PACKET, htons (ETH_P_ARP))) == -1) 
	{
		bb_error_msg("Could not open raw socket");
		return -1;
	}

	if(setsockopt (lSock, SOL_SOCKET, SO_BROADCAST, &lPtval, sizeof (lPtval)) == -1) 
	{
		bb_error_msg("Could not setsocketopt on raw socket");
		close(lSock);
		return -1;
	}
	
	return lSock;
}


static unsigned int atp_arp_get_ifc_index(char *pszInterface,int *lIfIndex)
{
	int lSockFd;
	struct ifreq ifr;

	memset(&ifr, 0, sizeof(struct ifreq));
	if((lSockFd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) >= 0) 
	{
		ifr.ifr_addr.sa_family = AF_INET;
		strcpy(ifr.ifr_name, pszInterface);
		
		if (ioctl(lSockFd, SIOCGIFINDEX, &ifr) == 0) 
		{
			*lIfIndex = ifr.ifr_ifindex;
		}
		else 
		{
			bb_error_msg("SIOCGIFINDEX failed\n");
			close(lSockFd);
			return 1;
		}
		
	} 
	else 
	{
		bb_error_msg("socket failed\n");
		return 1;
	}
	
	close(lSockFd);
	return 0;
}


static unsigned int atp_arp_get_ip_and_mac(char *pszInterface,unsigned int *pulAddr,unsigned char* pucArp)
{
	int lSockFd;
	struct ifreq ifr;
	struct sockaddr_in *pstOurIp;

	memset(&ifr, 0, sizeof(struct ifreq));
	if((lSockFd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) >= 0) 
	{
		ifr.ifr_addr.sa_family = AF_INET;
		strcpy(ifr.ifr_name, pszInterface);
		
		if (ioctl(lSockFd, SIOCGIFADDR, &ifr) == 0) 
		{
			pstOurIp = (struct sockaddr_in *) &ifr.ifr_addr;
			*pulAddr = pstOurIp->sin_addr.s_addr;
		}
		else 
	    {
			bb_error_msg("atp,arp ,SIOCGIFADDR failed\n");
			close(lSockFd);
			return 1;
		}
				
		if (ioctl(lSockFd, SIOCGIFHWADDR, &ifr) == 0) 
		{
			memcpy(pucArp, ifr.ifr_hwaddr.sa_data, 6);
		}
		else 
	    {
			bb_error_msg("atp,arp ,SIOCGIFHWADDR failed\n");
			close(lSockFd);
			return 1;
		}
	} 
	else 
	{
		bb_error_msg("atp,arp ,socket failed\n");
		return 1;
	}	
	
	close(lSockFd);
	return 0;
}
static unsigned int atp_arp_send_packet_main(char *pacSrcDev,char *pacDstDev)
{
    int lFlag;
    int lOpt;
    int lArpNum = 0;

    struct arpMsg	stArp;
    char acBuff[61];

    int lSock;
    struct ifreq szInterface;

    unsigned int ulSrcIpAddr;
    unsigned char uacSrcMacAddr[ATP_ARP_MAC_LEN];

    int lDstIfcIndex;

    struct sockaddr stAddr;
    
    if (NULL == pacSrcDev || NULL == pacDstDev)
    {
        return 0;
    }

	if (option_mask32 & ARP_OPT_p) 
	{
	    if (atp_arp_get_ip_and_mac(pacSrcDev,&ulSrcIpAddr,uacSrcMacAddr) != 0)
        {
            return 0;
        }
    }
	else
    {
        return 0;
    }

    if (option_mask32 & ARP_OPT_i) 
	{
	    if (atp_arp_get_ifc_index(pacDstDev,&lDstIfcIndex) != 0)
        {
            return 0;
        }
    }
	else
    {
        return 0;
    }

    	
    lSock = atp_arp_init_packet(ulSrcIpAddr,ulSrcIpAddr,uacSrcMacAddr,&stArp);

    if(lSock == -1)
	{
	    bb_error_msg("Could not open raw socket");
	    return 0;
	}
    
	memcpy(acBuff,&stArp,ATP_ARP_PACKET_BIG);
	memcpy(acBuff + ATP_ARP_MAC_SETOFFE,MAC_TARGET_ADDR,ATP_ARP_MAC_LEN);
	memcpy(acBuff + ATP_ARP_HADDR_SETOFFE,&ulSrcIpAddr,4);

	strncpy(szInterface.ifr_ifrn.ifrn_name, pacDstDev, IFNAMSIZ);
	if (setsockopt(lSock, SOL_SOCKET, SO_BINDTODEVICE,(char *)&szInterface, sizeof(szInterface)) < 0)
	{
	    bb_error_msg("Could not setsocketopt on raw socket<pacDstDev>");
		close(lSock);
		return 0;
	}

    if ((lFlag = fcntl(lSock, F_GETFL, 0)) >= 0)
    {
        fcntl(lSock, F_SETFL, lFlag | O_NONBLOCK);
    }

	memset(&stAddr,0,sizeof(stAddr));
	strcpy(stAddr.sa_data, pacDstDev);

    while(lArpNum < 3)
    {
        sendto(lSock, &acBuff, sizeof(stArp), 0, &stAddr, sizeof(stAddr));

        lArpNum ++;
    }
    
    close(lSock);
    
    return 1;
}
/*end of atp add code for arp send mac by ourself by c00126165 2008-7-15*/
static const struct aftype *ap; /* current address family       */
static const struct hwtype *hw; /* current hardware type        */
static int sockfd;              /* active socket descriptor     */
static smallint hw_set;         /* flag if hw-type was set (-H) */
static const char *device = ""; /* current device               */

static const char options[] ALIGN1 =
	"pub\0"
	"priv\0"
	"temp\0"
	"trail\0"
	"dontpub\0"
	"auto\0"
	"dev\0"
	"netmask\0";

/* Delete an entry from the ARP cache. */
/* Called only from main, once */
static int arp_del(char **args)
{
	char *host;
	struct arpreq req;
	struct sockaddr sa;
	int flags = 0;
	int err;

	memset(&req, 0, sizeof(req));

	/* Resolve the host name. */
	host = *args;
	if (ap->input(host, &sa) < 0) {
		bb_herror_msg_and_die("%s", host);
	}

	/* If a host has more than one address, use the correct one! */
	memcpy(&req.arp_pa, &sa, sizeof(struct sockaddr));

	if (hw_set)
		req.arp_ha.sa_family = hw->type;

	req.arp_flags = ATF_PERM;
	args++;
	while (*args != NULL) {
		switch (index_in_strings(options, *args)) {
		case 0: /* "pub" */
			flags |= 1;
			args++;
			break;
		case 1: /* "priv" */
			flags |= 2;
			args++;
			break;
		case 2: /* "temp" */
			req.arp_flags &= ~ATF_PERM;
			args++;
			break;
		case 3: /* "trail" */
			req.arp_flags |= ATF_USETRAILERS;
			args++;
			break;
		case 4: /* "dontpub" */
#ifdef HAVE_ATF_DONTPUB
			req.arp_flags |= ATF_DONTPUB;
#else
			bb_error_msg("feature ATF_DONTPUB is not supported");
#endif
			args++;
			break;
		case 5: /* "auto" */
#ifdef HAVE_ATF_MAGIC
			req.arp_flags |= ATF_MAGIC;
#else
			bb_error_msg("feature ATF_MAGIC is not supported");
#endif
			args++;
			break;
		case 6: /* "dev" */
			if (*++args == NULL)
				bb_show_usage();
			device = *args;
			args++;
			break;
		case 7: /* "netmask" */
			if (*++args == NULL)
				bb_show_usage();
			if (strcmp(*args, "255.255.255.255") != 0) {
				host = *args;
				if (ap->input(host, &sa) < 0) {
					bb_herror_msg_and_die("%s", host);
				}
				memcpy(&req.arp_netmask, &sa, sizeof(struct sockaddr));
				req.arp_flags |= ATF_NETMASK;
			}
			args++;
			break;
		default:
			bb_show_usage();
			break;
		}
	}
	if (flags == 0)
		flags = 3;

	strncpy(req.arp_dev, device, sizeof(req.arp_dev));

	err = -1;

	/* Call the kernel. */
	if (flags & 2) {
		if (option_mask32 & ARP_OPT_v)
			bb_error_msg("SIOCDARP(nopub)");
		err = ioctl(sockfd, SIOCDARP, &req);
		if (err < 0) {
			if (errno == ENXIO) {
				if (flags & 1)
					goto nopub;
				printf("No ARP entry for %s\n", host);
				return -1;
			}
			bb_perror_msg_and_die("SIOCDARP(priv)");
		}
	}
	if ((flags & 1) && err) {
 nopub:
		req.arp_flags |= ATF_PUBL;
		if (option_mask32 & ARP_OPT_v)
			bb_error_msg("SIOCDARP(pub)");
		if (ioctl(sockfd, SIOCDARP, &req) < 0) {
			if (errno == ENXIO) {
				printf("No ARP entry for %s\n", host);
				return -1;
			}
			bb_perror_msg_and_die("SIOCDARP(pub)");
		}
	}
	return 0;
}

/* Get the hardware address to a specified interface name */
static void arp_getdevhw(char *ifname, struct sockaddr *sa,
						 const struct hwtype *hwt)
{
	struct ifreq ifr;
	const struct hwtype *xhw;

	strcpy(ifr.ifr_name, ifname);
	ioctl_or_perror_and_die(sockfd, SIOCGIFHWADDR, &ifr,
					"cant get HW-Address for '%s'", ifname);
	if (hwt && (ifr.ifr_hwaddr.sa_family != hw->type)) {
		bb_error_msg_and_die("protocol type mismatch");
	}
	memcpy(sa, &(ifr.ifr_hwaddr), sizeof(struct sockaddr));

	if (option_mask32 & ARP_OPT_v) {
		xhw = get_hwntype(ifr.ifr_hwaddr.sa_family);
		if (!xhw || !xhw->print) {
			xhw = get_hwntype(-1);
		}
		bb_error_msg("device '%s' has HW address %s '%s'",
					 ifname, xhw->name,
					 xhw->print((char *) &ifr.ifr_hwaddr.sa_data));
	}
}

/* Set an entry in the ARP cache. */
/* Called only from main, once */
static int arp_set(char **args)
{
	char *host;
	struct arpreq req;
	struct sockaddr sa;
	int flags;

	memset(&req, 0, sizeof(req));

	host = *args++;
	if (ap->input(host, &sa) < 0) {
		bb_herror_msg_and_die("%s", host);
	}
	/* If a host has more than one address, use the correct one! */
	memcpy(&req.arp_pa, &sa, sizeof(struct sockaddr));

	/* Fetch the hardware address. */
	if (*args == NULL) {
		bb_error_msg_and_die("need hardware address");
	}
	if (option_mask32 & ARP_OPT_D) {
		arp_getdevhw(*args++, &req.arp_ha, hw_set ? hw : NULL);
	} else {
		if (hw->input(*args++, &req.arp_ha) < 0) {
			bb_error_msg_and_die("invalid hardware address");
		}
	}

	/* Check out any modifiers. */
	flags = ATF_PERM | ATF_COM;
	while (*args != NULL) {
		switch (index_in_strings(options, *args)) {
		case 0: /* "pub" */
			flags |= ATF_PUBL;
			args++;
			break;
		case 1: /* "priv" */
			flags &= ~ATF_PUBL;
			args++;
			break;
		case 2: /* "temp" */
			flags &= ~ATF_PERM;
			args++;
			break;
		case 3: /* "trail" */
			flags |= ATF_USETRAILERS;
			args++;
			break;
		case 4: /* "dontpub" */
#ifdef HAVE_ATF_DONTPUB
			flags |= ATF_DONTPUB;
#else
			bb_error_msg("feature ATF_DONTPUB is not supported");
#endif
			args++;
			break;
		case 5: /* "auto" */
#ifdef HAVE_ATF_MAGIC
			flags |= ATF_MAGIC;
#else
			bb_error_msg("feature ATF_MAGIC is not supported");
#endif
			args++;
			break;
		case 6: /* "dev" */
			if (*++args == NULL)
				bb_show_usage();
			device = *args;
			args++;
			break;
		case 7: /* "netmask" */
			if (*++args == NULL)
				bb_show_usage();
			if (strcmp(*args, "255.255.255.255") != 0) {
				host = *args;
				if (ap->input(host, &sa) < 0) {
					bb_herror_msg_and_die("%s", host);
				}
				memcpy(&req.arp_netmask, &sa, sizeof(struct sockaddr));
				flags |= ATF_NETMASK;
			}
			args++;
			break;
		default:
			bb_show_usage();
			break;
		}
	}

	/* Fill in the remainder of the request. */
	req.arp_flags = flags;

	strncpy(req.arp_dev, device, sizeof(req.arp_dev));

	/* Call the kernel. */
	if (option_mask32 & ARP_OPT_v)
		bb_error_msg("SIOCSARP()");
	xioctl(sockfd, SIOCSARP, &req);
	return 0;
}


/* Print the contents of an ARP request block. */
static void
arp_disp(const char *name, char *ip, int type, int arp_flags,
		 char *hwa, char *mask, char *dev)
{
	const struct hwtype *xhw;

	xhw = get_hwntype(type);
	if (xhw == NULL)
		xhw = get_hwtype(DFLT_HW);

	printf("%s (%s) at ", name, ip);

	if (!(arp_flags & ATF_COM)) {
		if (arp_flags & ATF_PUBL)
			printf("* ");
		else
			printf("<incomplete> ");
	} else {
		printf("%s [%s] ", hwa, xhw->name);
	}

	if (arp_flags & ATF_NETMASK)
		printf("netmask %s ", mask);

	if (arp_flags & ATF_PERM)
		printf("PERM ");
	if (arp_flags & ATF_PUBL)
		printf("PUP ");
#ifdef HAVE_ATF_MAGIC
	if (arp_flags & ATF_MAGIC)
		printf("AUTO ");
#endif
#ifdef HAVE_ATF_DONTPUB
	if (arp_flags & ATF_DONTPUB)
		printf("DONTPUB ");
#endif
	if (arp_flags & ATF_USETRAILERS)
		printf("TRAIL ");

	printf("on %s\n", dev);
}

/* Display the contents of the ARP cache in the kernel. */
/* Called only from main, once */
static int arp_show(char *name)
{
	const char *host;
	const char *hostname;
	FILE *fp;
	struct sockaddr sa;
	int type, flags;
	int num;
	unsigned entries = 0, shown = 0;
	char ip[128];
	char hwa[128];
	char mask[128];
	char line[128];
	char dev[128];

	host = NULL;
	if (name != NULL) {
		/* Resolve the host name. */
		if (ap->input(name, &sa) < 0) {
			bb_herror_msg_and_die("%s", name);
		}
		host = xstrdup(ap->sprint(&sa, 1));
	}
	fp = xfopen("/proc/net/arp", "r");
	/* Bypass header -- read one line */
	fgets(line, sizeof(line), fp);

	/* Read the ARP cache entries. */
	while (fgets(line, sizeof(line), fp)) {

		mask[0] = '-'; mask[1] = '\0';
		dev[0] = '-'; dev[1] = '\0';
		/* All these strings can't overflow
		 * because fgets above reads limited amount of data */
		num = sscanf(line, "%s 0x%x 0x%x %s %s %s\n",
					 ip, &type, &flags, hwa, mask, dev);
		if (num < 4)
			break;

		entries++;
		/* if the user specified hw-type differs, skip it */
		if (hw_set && (type != hw->type))
			continue;

		/* if the user specified address differs, skip it */
		if (host && strcmp(ip, host) != 0)
			continue;

		/* if the user specified device differs, skip it */
		if (device[0] && strcmp(dev, device) != 0)
			continue;

		shown++;
		/* This IS ugly but it works -be */
		hostname = "?";
		if (!(option_mask32 & ARP_OPT_n)) {
			if (ap->input(ip, &sa) < 0)
				hostname = ip;
			else
				hostname = ap->sprint(&sa, (option_mask32 & ARP_OPT_n) | 0x8000);
			if (strcmp(hostname, ip) == 0)
				hostname = "?";
		}

		arp_disp(hostname, ip, type, flags, hwa, mask, dev);
	}
	if (option_mask32 & ARP_OPT_v)
		printf("Entries: %d\tSkipped: %d\tFound: %d\n",
			   entries, entries - shown, shown);

	if (!shown) {
		if (hw_set || host || device[0])
			printf("No match found in %d entries\n", entries);
	}
	if (ENABLE_FEATURE_CLEAN_UP) {
		free((char*)host);
		fclose(fp);
	}
	return 0;
}

int arp_main(int argc, char **argv);
int arp_main(int argc, char **argv)
{
	char *hw_type;
	char *protocol;

	unsigned int ulRet = 0;

	/* Initialize variables... */
	ap = get_aftype(DFLT_AF);
	if (!ap)
		bb_error_msg_and_die("%s: %s not supported", DFLT_AF, "address family");

	getopt32(argv, "A:c:H:t:i:adnDsv", &protocol, &protocol,
				 &hw_type, &hw_type, &device);
	argv += optind;
	
/*start of atp add code for arp send mac by ourself by c00126165 2008-7-15*/	

    ulRet = atp_arp_send_packet_main(protocol,device);

	if (ulRet == 1)
    {
        return 0;
    }
/*end of atp add code for arp send mac by ourself by c00126165 2008-7-15*/
	
	if (option_mask32 & ARP_OPT_A || option_mask32 & ARP_OPT_p) {
		ap = get_aftype(protocol);
		if (ap == NULL)
			bb_error_msg_and_die("%s: unknown %s", protocol, "address family");
	}
	if (option_mask32 & ARP_OPT_A || option_mask32 & ARP_OPT_p) {
		hw = get_hwtype(hw_type);
		if (hw == NULL)
			bb_error_msg_and_die("%s: unknown %s", hw_type, "hardware type");
		hw_set = 1;
	}
	//if (option_mask32 & ARP_OPT_i)... -i

	if (ap->af != AF_INET) {
		bb_error_msg_and_die("%s: kernel only supports 'inet'", ap->name);
	}

	/* If no hw type specified get default */
	if (!hw) {
		hw = get_hwtype(DFLT_HW);
		if (!hw)
			bb_error_msg_and_die("%s: %s not supported", DFLT_HW, "hardware type");
	}

	if (hw->alen <= 0) {
		bb_error_msg_and_die("%s: %s without ARP support",
							 hw->name, "hardware type");
	}
	sockfd = xsocket(AF_INET, SOCK_DGRAM, 0);

	/* Now see what we have to do here... */
	if (option_mask32 & (ARP_OPT_d|ARP_OPT_s)) {
		if (argv[0] == NULL)
			bb_error_msg_and_die("need host name");
		if (option_mask32 & ARP_OPT_s)
			return arp_set(argv);
		return arp_del(argv);
	}
	//if (option_mask32 & ARP_OPT_a) - default
	return arp_show(argv[0]);
}
