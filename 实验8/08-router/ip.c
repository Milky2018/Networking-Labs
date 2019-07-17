#include "ip.h"
#include "icmp.h"
#include "rtable.h"

#include <stdio.h>
#include <stdlib.h>
#include "log.h"

// forward the packet
static void ip_forward_packet(u32 ip_dst, char *packet, int len)
{
    struct iphdr *ip = packet_to_ip_hdr(packet);
    ip->ttl--;
    if (ip->ttl <= 0) {
        icmp_send_packet(packet, len, ICMP_TIME_EXCEEDED, ICMP_EXC_TTL);
        return;
    }

	ip->checksum = ip_checksum(ip);
    rt_entry_t *entry = longest_prefix_match(ip_dst);
    if (entry) {
		ip_send_packet(packet, len);
		return;
	} else {
        icmp_send_packet(packet, len, ICMP_DEST_UNREACH, ICMP_NET_UNREACH);
        return;
    }
}

// handle ip packet
//
// If the packet is ICMP echo request and the destination IP address is equal to
// the IP address of the iface, send ICMP echo reply; otherwise, forward the
// packet.
void handle_ip_packet(iface_info_t *iface, char *packet, int len)
{
	// log(DEBUG, "");
    struct iphdr *ip = packet_to_ip_hdr(packet);
	struct icmphdr *icmp = (struct icmphdr *)(packet + ETHER_HDR_SIZE + IP_HDR_SIZE(ip));
    u32 dst = ntohl(ip->daddr);

	if (icmp->type == ICMP_ECHOREQUEST && dst == iface->ip) {
		icmp_send_packet(packet, len, ICMP_ECHOREPLY, 0);
		return;
	} else {
		ip_forward_packet(dst, packet, len);
		return;
	}
}
