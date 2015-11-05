#include "endpoint.h"

#include <stdint.h>
#include <netinet/in.h>

#include <string.h>

void translate_endpoint(endpoint_t *ep) {
    struct sockaddr *sa;
    struct sockaddr_in *sin;
    struct sockaddr_in6 *sin6;
    uint32_t addr;

    sin = sin6 = sa = (void *)&ep->addr;
    switch (sa->sa_family) {
        case AF_INET:
            ep->ep_class = EPC_IP4;
            ep->ep.ip4.port = ntohs(sin->sin_port);
            addr = ntohl(sin->sin_addr.s_addr);
            ep->ep.ip4.addr[0] = addr >> 0x18;
            ep->ep.ip4.addr[1] = (addr >> 0x10) & 0xff;
            ep->ep.ip4.addr[2] = (addr >> 0x08) & 0xff;
            ep->ep.ip4.addr[3] = addr & 0xff;
            break;
        case AF_INET6:
            ep->ep_class = EPC_IP6;
            ep->ep.ip6.port = ntohs(sin6->sin6_port);
            memcpy(ep->ep.ip6.addr, &sin6->sin6_addr.s6_addr, sizeof(ep->ep.ip6.addr));
            break;
    }
}
