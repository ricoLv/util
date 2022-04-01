/*
 * Copyright (c) 1995, 1999
 *	Berkeley Software Design, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * THIS SOFTWARE IS PROVIDED BY Berkeley Software Design, Inc. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Berkeley Software Design, Inc. BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	BSDI ifaddrs.h,v 2.5 2000/02/23 14:51:59 dab Exp
 */

#ifndef	_IFADDRS_H_
#define	_IFADDRS_H_

#include <arpa/inet.h>
#include <cstring>
#include <errno.h>
#include <net/if.h>
#include <netinet/in.h>
#include <new>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>

#include "LocalArray.h"
#include "ScopedFd.h"
// struct ifaddrs {
// 	struct ifaddrs  *ifa_next;
// 	char		*ifa_name;
// 	unsigned int	 ifa_flags;
// 	struct sockaddr	*ifa_addr;
// 	struct sockaddr	*ifa_netmask;
// 	struct sockaddr	*ifa_dstaddr;
// 	void		*ifa_data;
// };
struct ifaddrs {
    // Pointer to next struct in list, or NULL at end.
    ifaddrs* ifa_next;
    // Interface name.
    char* ifa_name;
    // Interface flags.
    unsigned int ifa_flags;
    // Interface network address.
    sockaddr* ifa_addr;
    // Interface netmask.
    sockaddr* ifa_netmask;
    // Unused
    sockaddr* ifa_ifu;
    void *ifa_data;

    ifaddrs(ifaddrs* next)
    : ifa_next(next), ifa_name(NULL), ifa_flags(0), ifa_addr(NULL), ifa_netmask(NULL), ifa_ifu(NULL), ifa_data(NULL)
    {
    }
    ~ifaddrs() {
        delete ifa_next;
        delete[] ifa_name;
        delete ifa_addr;
        delete ifa_netmask;
    }
    // Sadly, we can't keep the interface index for portability with BSD.
    // We'll have to keep the name instead, and re-query the index when
    // we need it later.
    bool setNameAndFlagsByIndex(int interfaceIndex) {
        // Get the name.
        char buf[IFNAMSIZ];
        char* name = if_indextoname(interfaceIndex, buf);
        if (name == NULL) {
            return false;
        }
        ifa_name = new char[strlen(name) + 1];
        strcpy(ifa_name, name);
        // Get the flags.
        ScopedFd fd(socket(AF_INET, SOCK_DGRAM, 0));
        if (fd.get() == -1) {
            return false;
        }
        ifreq ifr;
        memset(&ifr, 0, sizeof(ifr));
        strcpy(ifr.ifr_name, name);
        int rc = ioctl(fd.get(), SIOCGIFFLAGS, &ifr);
        if (rc == -1) {
            return false;
        }
        ifa_flags = ifr.ifr_flags;
        return true;
    }
    // Netlink gives us the address family in the header, and the
    // sockaddr_in or sockaddr_in6 bytes as the payload. We need to
    // stitch the two bits together into the sockaddr that's part of
    // our portable interface.
    void setAddress(int family, void* data, size_t byteCount) {
        // Set the address proper...
        sockaddr_storage* ss = new sockaddr_storage;
        memset(ss, 0, sizeof(*ss));
        ifa_addr = reinterpret_cast<sockaddr*>(ss);
        ss->ss_family = family;
        uint8_t* dst = sockaddrBytes(family, ss);
        memcpy(dst, data, byteCount);
    }
    // Netlink gives us the prefix length as a bit count. We need to turn
    // that into a BSD-compatible netmask represented by a sockaddr*.
    void setNetmask(int family, size_t prefixLength) {
        // ...and work out the netmask from the prefix length.
        sockaddr_storage* ss = new sockaddr_storage;
        memset(ss, 0, sizeof(*ss));
        ifa_netmask = reinterpret_cast<sockaddr*>(ss);
        ss->ss_family = family;
        uint8_t* dst = sockaddrBytes(family, ss);
        memset(dst, 0xff, prefixLength / 8);
        if ((prefixLength % 8) != 0) {
            dst[prefixLength/8] = (0xff << (8 - (prefixLength % 8)));
        }
    }
    // Returns a pointer to the first byte in the address data (which is
    // stored in network byte order).
    uint8_t* sockaddrBytes(int family, sockaddr_storage* ss) {
        if (family == AF_INET) {
            sockaddr_in* ss4 = reinterpret_cast<sockaddr_in*>(ss);
            return reinterpret_cast<uint8_t*>(&ss4->sin_addr);
        } else if (family == AF_INET6) {
            sockaddr_in6* ss6 = reinterpret_cast<sockaddr_in6*>(ss);
            return reinterpret_cast<uint8_t*>(&ss6->sin6_addr);
        }
        return NULL;
    }
private:
    // Disallow copy and assignment.
    ifaddrs(const ifaddrs&);
    void operator=(const ifaddrs&);
};


/*
 * This may have been defined in <net/if.h>.  Note that if <net/if.h> is
 * to be included it must be included before this header file.
 */
#ifndef	ifa_broadaddr
#define	ifa_broadaddr	ifa_dstaddr	/* broadcast address interface */
#endif

#include <sys/cdefs.h>
__BEGIN_DECLS
extern int getifaddrs(struct ifaddrs **ifap);
extern void freeifaddrs(struct ifaddrs *ifa);
__END_DECLS

#endif
