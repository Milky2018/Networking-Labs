#include "icmp.h"
#include "ip.h"
#include "rtable.h"
#include "arp.h"
#include "base.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"

// send icmp packet
void icmp_send_packet(const char *in_pkt, int len, u8 type, u8 code)
{
	struct iphdr *ip = packet_to_ip_hdr(in_pkt);
	char *packet = malloc(ETHER_HDR_SIZE + ntohs(ip->tot_len));
	memcpy(packet, in_pkt, len);

	struct icmphdr *icmp = (struct icmphdr *)(packet + ETHER_HDR_SIZE + IP_HDR_SIZE(ip));
	icmp->code = code;
	icmp->type = type;
	int size = 0;
	if (type != ICMP_ECHOREPLY) {
		memset((char *)icmp + 4, 0, 4);
		size = IP_HDR_SIZE(ip) + 8;
	} else {
		size = IP_HDR_SIZE(ip) + 4;
	}
	icmp->checksum = icmp_checksum(icmp, ntohs(ip->tot_len) - IP_HDR_SIZE(ip));
	
	ip_send_packet(packet, len);
	// free(packet);
	// log(DEBUG, "");
}
