#include "arp.h"
#include "base.h"
#include "types.h"
#include "packet.h"
#include "ether.h"
#include "arpcache.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"

// send an arp request: encapsulate an arp request packet, send it out through
// iface_send_packet
void arp_send_request(iface_info_t *iface, u32 dst_ip)
{
	struct arp_with_hdr awh = {
		.hdr = {
			.ether_dhost = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
			.ether_type = htons(ETH_P_ARP)
		},
		.arp = {
			.arp_hrd = htons(ARPHRD_ETHER),
			.arp_pro = htons(ETH_P_IP),
			.arp_hln = 6,
			.arp_pln = 4,
			.arp_spa = htonl(iface->ip),
			.arp_tpa = htonl(dst_ip),
			.arp_op = htons(ARPOP_REQUEST)
		}
	};
	memcpy(awh.hdr.ether_shost, iface->mac, ETH_ALEN);
	memcpy(awh.arp.arp_sha, iface->mac, ETH_ALEN);
	memset(awh.arp.arp_tha, 0, ETH_ALEN);
	
	// struct arp_with_hdr *awh2 = malloc(sizeof(awh));
	// memcpy(awh2, &awh, sizeof(awh));
	// iface_send_packet(iface, (char *)awh2, sizeof(awh));

	iface_send_packet(iface, (char *)&awh, sizeof(awh));
	// log(DEBUG, "");
}

// send an arp reply packet: encapsulate an arp reply packet, send it out
// through iface_send_packet
void arp_send_reply(iface_info_t *iface, struct ether_arp *req_hdr)
{
	struct arp_with_hdr awh = {
		.hdr = {
			.ether_type = htons(ETH_P_ARP)
		},
		.arp = {
			.arp_hrd = htons(ARPHRD_ETHER),
			.arp_pro = htons(ETH_P_IP),
			.arp_hln = 6,
			.arp_pln = 4,
			.arp_spa = htonl(iface->ip),
			.arp_tpa = req_hdr->arp_spa,
			.arp_op = htons(ARPOP_REPLY)
		}
	};

	memcpy(&awh.hdr.ether_dhost, req_hdr->arp_sha, ETH_ALEN);
	memcpy(&awh.hdr.ether_shost, iface->mac, ETH_ALEN);

	memcpy(&awh.arp.arp_tha, req_hdr->arp_sha, ETH_ALEN);
	memcpy(&awh.arp.arp_sha, iface->mac, ETH_ALEN);

	iface_send_packet(iface, (char *)&awh, sizeof(awh));
	// log(DEBUG, "");
}

void handle_arp_packet(iface_info_t *iface, char *packet, int len)
{
	struct arp_with_hdr *awh = (struct arp_with_hdr *)packet;
	if (ntohs(awh->arp.arp_op) == ARPOP_REQUEST) {
		if (ntohl(awh->arp.arp_tpa) == iface->ip) {
			arpcache_insert(ntohl(awh->arp.arp_spa), awh->arp.arp_sha);
			arp_send_reply(iface, &awh->arp);
			// log(DEBUG, "");
		}
	} else if (ntohs(awh->arp.arp_op) == ARPOP_REPLY) {
		arpcache_insert(ntohl(awh->arp.arp_spa), awh->arp.arp_sha);
		// log(DEBUG, "");
	}

}

// send (IP) packet through arpcache lookup 
//
// Lookup the mac address of dst_ip in arpcache. If it is found, fill the
// ethernet header and emit the packet by iface_send_packet, otherwise, pending 
// this packet into arpcache, and send arp request.
void iface_send_packet_by_arp(iface_info_t *iface, u32 dst_ip, char *packet, int len)
{
	struct ether_header *eh = (struct ether_header *)packet;
	memcpy(eh->ether_shost, iface->mac, ETH_ALEN);
	eh->ether_type = htons(ETH_P_IP);

	u8 dst_mac[ETH_ALEN];
	int found = arpcache_lookup(dst_ip, dst_mac);
	if (found) {
		// log(DEBUG, "found the mac of %x, send this packet", dst_ip);
		memcpy(eh->ether_dhost, dst_mac, ETH_ALEN);
		iface_send_packet(iface, packet, len);
	}
	else {
		// log(DEBUG, "lookup %x failed, pend this packet", dst_ip);
		arpcache_append_packet(iface, dst_ip, packet, len);
	}
}
