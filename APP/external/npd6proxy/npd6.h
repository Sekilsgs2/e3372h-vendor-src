/*
 *   This software is Copyright 2011 by Sean Groarke <sgroarke@gmail.com>
 *   All rights reserved.
 *
 *   This file is part of npd6.
 *
 *   npd6 is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   npd6 is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with npd6.  If not, see <http://www.gnu.org/licenses/>.
 */

/* $Id: npd6.h 84 2012-02-29 09:46:20Z npd6Project $
 * $HeadURL: https://npd6.googlecode.com/svn/trunk/npd6.h $
 
   <问题单号>        <作  者>    <修改时间>   <版本> <修改描述>
     4032401612  y00248130   2014/03/24          优化代码结构
 */

#ifndef NPD6_H

#define NPD6_H
#include "includes.h"
#include "netprotoapi_public.h"

#ifndef NPD6_CONF
#define NPD6_CONF "/etc/npd6.conf"
#endif

#ifndef NPD6_LOG
#define NPD6_LOG (ROUTER_VARPATH_PREFIX "/log/npd6.log")
#endif

#ifndef NULL
#define NULL 0
#endif

#ifndef TRUE
#define TRUE    1
#endif

// Misc bits and bobs
#define HWADDR_MAX          16
#define MAX_PKT_BUFF        1500
#define MAX_MSG_SIZE        2048
#define LOGTIMEFORMAT       "%b %d %H:%M:%S"
#define INTERFACE_STRLEN    12
#define NULLSTR             "null"
#define min(a,b)            (((a) < (b)) ? (a) : (b))
#define MAXMAXHOPS          255
#define HWADDR_MAX          16
#define DISPATCH_TIMEOUT    300000          // milliseconds 300000 = 5 mins
#define flog(pri, ...)      npd6log(__FUNCTION__, pri, __VA_ARGS__)
#define LOG_DEBUG2          8
#define USE_FILE            1
#define USE_SYSLOG          2
#define USE_STD             3
#define MAXTARGETS          1000000         // Ultimate sane limit

//*****************************************************************************
// Globals
//
char            *pname;
char            *paramName;
int             sockicmp;
int             sockpkt;
int             lansockpkt;
int             debug;
int             daemonize;
FILE            *logFileFD;
int             logging;
char            configfile[FILENAME_MAX];
FILE            *configFileFD;
int             initialIFFlags;

// Interface we're interested in
char            interfacestr[INTERFACE_STRLEN];
unsigned int    interfaceIdx;
unsigned int    laninterfaceIdx;
unsigned char   linkAddr[8];

// Regarding the target prefix being matched
char            prefixaddrstr[INET6_ADDRSTRLEN];
struct          in6_addr prefixaddr;
int             prefixaddrlen;

// Key behaviour
int             naLinkOptFlag;      // From config file NPD6OPTFLAG
int             nsIgnoreLocal;      // From config file NPD6LOCALIG
int             naRouter;           // From config file NPD6ROUTERNA
int             maxHops;            // From config file NPD6MAXHOPS
int             collectTargets;     // From config file NPD6TARGETS

// Target tree data structures etc
void            *tRoot;
int             tCompare(const void *, const void *);
void            tDump(const void *, const VISIT , const int);
int             tEntries;

// Black/whitelisting data
void            *lRoot;
int             listType;
#define         NOLIST      0
#define         BLACKLIST   1
#define         WHITELIST   2



//*****************************************************************************
// Prototypes
//
// main.c
void    dispatcher(void);
void    showUsage(void);

// config.c
int     readConfig(char *);

// util.c
int     npd6log(const char *, int , char *, ...);
void    usersignal(int );
void    print_addr(struct in6_addr *, char *);
void    print_addr16(const struct in6_addr * , char * );
int     build_addr(char *, struct in6_addr *);
int     prefixset(char []);
void    stripwhitespace(char *);
void    dumpHex(unsigned char *, unsigned int);
int     getLinkaddress( char *, unsigned char *);
void    showVersion(void);
int     openLog(char *);
void    dropdead(void);
void    dumpAddressData(void);
void    storeTarget( struct in6_addr *);
int     tCompare(const void *, const void *);
void    tDump(const void *, const VISIT, const int);
void    storeListEntry(struct in6_addr *);



// icmp6.c
int     open_packet_socket(void);
int     open_icmpv6_socket(void);
int     get_rx(unsigned char *msg, int sockfd, int timeout);
void    if_allmulti(char *, unsigned int);
int     init_sockets(void);
int     npd6getwan(char *);

// ip6.c
void    processNS( unsigned char *, unsigned int);
int     addr6match( struct in6_addr *, struct in6_addr *, int);
int     addr6matchex( struct in6_addr *a1);
int     processLocalNS(struct in6_addr *targetaddr,  unsigned int  multicastNS,
                                struct in6_addr srcLinkAddr);

#endif

