#include "mospf_daemon.h"
#include "mospf_proto.h"
#include "mospf_nbr.h"
#include "mospf_database.h"

#include "ip.h"

#include "list.h"
#include "log.h"
#include "packet.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>

extern ustack_t *instance;

pthread_mutex_t mospf_lock;

void mospf_init()
{
	pthread_mutex_init(&mospf_lock, NULL);

	instance->area_id = 0;
	// get the ip address of the first interface
	iface_info_t *iface = list_entry(instance->iface_list.next, iface_info_t, list);
	instance->router_id = iface->ip;
	instance->sequence_num = 0;
	instance->lsuint = MOSPF_DEFAULT_LSUINT;

	iface = NULL;
	list_for_each_entry(iface, &instance->iface_list, list) {
		iface->helloint = MOSPF_DEFAULT_HELLOINT;
		init_list_head(&iface->nbr_list);
	}

	init_mospf_db();
}

void *sending_mospf_hello_thread(void *param);
void *sending_mospf_lsu_thread(void *param);
void *checking_nbr_thread(void *param);
void *checking_database_thread(void *param);
void *debug_thread(void *param);

static void sending_mospf_lsu();
// void generate_rtable();

void mospf_run()
{
	pthread_t hello, lsu, nbr, db, debug;
	pthread_create(&hello, NULL, sending_mospf_hello_thread, NULL);
	pthread_create(&lsu, NULL, sending_mospf_lsu_thread, NULL);
	pthread_create(&nbr, NULL, checking_nbr_thread, NULL);
	pthread_create(&db, NULL, checking_database_thread, NULL);
	pthread_create(&debug, NULL, debug_thread, NULL);
}

void *debug_thread(void *param)
{
	while (1) {
		sleep(5);
		mospf_db_display();
	}
}

void *sending_mospf_hello_thread(void *param)
{
	// fprintf(stdout, "TODO: send mOSPF Hello message periodically.\n");
	while (1) {
		pthread_mutex_lock(&mospf_lock);
		iface_info_t *iface = NULL;
		list_for_each_entry(iface, &instance->iface_list, list) {
			// struct mospf_hdr_hello *packet = new_mospf_hdr_hello(instance->router_id, iface->mask);
			u16 len = ETHER_HDR_SIZE + IP_BASE_HDR_SIZE + MOSPF_HDR_HELLO_SIZE;
			char *packet = malloc(len);

			struct ether_header *ether_header = (struct ether_header *)packet;
			memcpy(ether_header->ether_dhost, &(int []){0x01, 0x00, 0x5e, 0x00, 0x00, 0x05}, ETH_ALEN);
			memcpy(ether_header->ether_shost, iface->mac, ETH_ALEN);
			ether_header->ether_type = htons(ETH_P_IP);

			struct iphdr *ip_hdr = (struct iphdr *)(packet + ETHER_HDR_SIZE);
			ip_init_hdr(ip_hdr, iface->ip, MOSPF_ALLSPFRouters, IP_BASE_HDR_SIZE + MOSPF_HDR_HELLO_SIZE, 90);

			struct mospf_hdr_hello *hdr_hello = (struct mospf_hdr_hello *)(packet + ETHER_HDR_SIZE + IP_BASE_HDR_SIZE);
			mospf_init_hdr_hello(hdr_hello, instance->router_id, iface->mask);

			ip_hdr->checksum = ip_checksum(ip_hdr);

			iface_send_packet(iface, packet, len);

			free(packet);
		}
		pthread_mutex_unlock(&mospf_lock);
		sleep(MOSPF_DEFAULT_HELLOINT);
	}
	
	return NULL;
}

void *checking_nbr_thread(void *param)
{
	// fprintf(stdout, "TODO: neighbor list timeout operation.\n");
	while (1) {
		iface_info_t *iface = NULL;
		pthread_mutex_lock(&mospf_lock);
		list_for_each_entry(iface, &instance->iface_list, list) {
			mospf_nbr_t *nbr = NULL, *q;
			list_for_each_entry_safe(nbr, q, &iface->nbr_list, list) {
				nbr->alive ++;
				if (nbr->alive >= 3 * MOSPF_DEFAULT_HELLOINT) {
					list_delete_entry(&nbr->list);
					free(nbr);
				}
			}
		}
		pthread_mutex_unlock(&mospf_lock);
		sleep(1);
	}
	return NULL;
}

void *checking_database_thread(void *param)
{
	// fprintf(stdout, "TODO: link state database timeout operation.\n");
	while (1) {
		mospf_db_entry_t *entry = NULL;
		mospf_db_foreach(entry) {
			entry->timer++;
			if (entry->timer > MOSPF_DATABASE_TIMEOUT) {
				mospf_db_remove(entry->rid);
			}
		}
		sleep(1);
	}

	return NULL;
}

void handle_mospf_hello(iface_info_t *iface, char *packet, int len)
{
	// fprintf(stdout, "TODO: handle mOSPF Hello message.\n");
	struct iphdr *ip_hdr = packet_to_ip_hdr(packet);
	struct mospf_hdr_hello *mospf = (struct mospf_hdr_hello *)(packet + ETHER_HDR_SIZE + IP_BASE_HDR_SIZE);

	mospf_nbr_t *pos = NULL;
	bool found = false;
	pthread_mutex_lock(&mospf_lock);
	list_for_each_entry(pos, &iface->nbr_list, list) {
		if (pos->nbr_id == ntohl(mospf->hdr.rid)) {
			found = true;
			pos->alive = 0;
			break;
		}
	}

	if (!found) {
		mospf_nbr_t *nbr = malloc(sizeof(mospf_nbr_t));
		nbr->alive = 0;
		nbr->nbr_id = ntohl(mospf->hdr.rid);
		nbr->nbr_ip = ntohl(ip_hdr->saddr);
		nbr->nbr_mask = ntohl(mospf->body.mask);
		list_add_tail(&nbr->list, &iface->nbr_list);
		iface->num_nbr++;
	}
	pthread_mutex_unlock(&mospf_lock);

	if (!found) {
		sending_mospf_lsu();
	}

	// free(packet);
}

static void sending_mospf_lsu()
{
	iface_info_t *iface = NULL;
	u32 nadv = 0;

	pthread_mutex_lock(&mospf_lock);
	list_for_each_entry(iface, &instance->iface_list, list) {
		if (iface->num_nbr == 0) {
			nadv = nadv + 1;
		} else {
			nadv = nadv + iface->num_nbr;
		}
	}
	u16 len = ETHER_HDR_SIZE + IP_BASE_HDR_SIZE + MOSPF_HDR_LSU_SIZE + nadv * MOSPF_LSA_SIZE;
	char *packet = malloc(len);
	struct mospf_lsa lsa[nadv];
	nadv = 0;
	list_for_each_entry(iface, &instance->iface_list, list) {
		if (iface->num_nbr == 0) {
			lsa[nadv].mask = iface->mask;
			lsa[nadv].rid = 0;
			lsa[nadv].subnet = iface->mask & iface->ip;
			nadv = nadv + 1;
		} else {
			mospf_nbr_t *nbr = NULL;
			list_for_each_entry(nbr, &iface->nbr_list, list) {
				lsa[nadv].mask = nbr->nbr_mask;
				lsa[nadv].rid = nbr->nbr_id;
				lsa[nadv].subnet = nbr->nbr_ip & nbr->nbr_mask;
				nadv = nadv + 1;
			}
		}
	}
	list_for_each_entry(iface, &instance->iface_list, list) {
		struct ether_header *eth_hdr = (struct ether_header *)packet;
		memcpy(eth_hdr->ether_shost, iface->mac, ETH_ALEN);
		eth_hdr->ether_type = htons(ETH_P_IP);

		struct mospf_hdr_lsu *lsu = (struct mospf_hdr_lsu *)(packet + ETHER_HDR_SIZE + IP_BASE_HDR_SIZE);
		mospf_init_hdr_lsu(lsu, instance->router_id, nadv, lsa);

		mospf_nbr_t *nbr = NULL;
		list_for_each_entry(nbr, &iface->nbr_list, list) {
			struct iphdr *ip_hdr = packet_to_ip_hdr(packet);
			ip_init_hdr(ip_hdr, iface->ip, nbr->nbr_ip, len - ETHER_HDR_SIZE, IPPROTO_MOSPF);

			ip_hdr->checksum = ip_checksum(ip_hdr);

			ip_send_packet(packet, len);
		}
	}
	instance->sequence_num++;
	pthread_mutex_unlock(&mospf_lock);
	free(packet);
}

void *sending_mospf_lsu_thread(void *param)
{
	// fprintf(stdout, "TODO: send mOSPF LSU message periodically.\n");
	while (1) {
		sending_mospf_lsu();
		sleep(MOSPF_DEFAULT_LSUINT);
	}

	return NULL;
}

void handle_mospf_lsu(iface_info_t *iface, char *packet, int len)
{
	// fprintf(stdout, "TODO: handle mOSPF LSU message.\n");
	struct iphdr *ip_hdr = packet_to_ip_hdr(packet);
	struct mospf_hdr_lsu *lsu = (struct mospf_hdr_lsu *)(packet + ETHER_HDR_SIZE + IP_BASE_HDR_SIZE);
	struct mospf_lsa *lsa = get_mospf_lsa(lsu);

	u32 rid = ntohl(lsu->hdr.rid);
	u16 seq = ntohs(lsu->body.seq);
	u32 nadv = ntohl(lsu->body.nadv);

	pthread_mutex_lock(&mospf_lock);
	mospf_db_entry_t *entry = mospf_db_query(rid);
	if (entry == NULL || seq > entry->seq) {
		mospf_db_entry_t *new_entry = malloc(sizeof(mospf_db_entry_t));
		new_entry->seq = seq;
		new_entry->rid = rid;
		new_entry->nadv = nadv;
		new_entry->array = malloc(nadv * MOSPF_LSA_SIZE);
		for (int i = 0; i < nadv; i++) {
			new_entry->array[i].mask = ntohl(lsa[i].mask);
			new_entry->array[i].rid = ntohl(lsa[i].rid);
			new_entry->array[i].subnet = ntohl(lsa[i].subnet);
		}
		mospf_db_update(new_entry);
	}
	pthread_mutex_unlock(&mospf_lock);

	lsu->body.ttl--;
	if (lsu->body.ttl > 0) {
		iface_info_t *pos = NULL;
		list_for_each_entry(pos, &instance->iface_list, list) {
			if (pos != iface) {
				struct ether_header *eth_hdr = (struct ether_header *)packet;
				memcpy(eth_hdr->ether_shost, pos->mac, ETH_ALEN);
				mospf_nbr_t *nbr = NULL;
				list_for_each_entry(nbr, &pos->nbr_list, list) {
					ip_init_hdr(ip_hdr, pos->ip, nbr->nbr_ip, len - ETHER_HDR_SIZE, IPPROTO_MOSPF);
					ip_hdr->checksum = ip_checksum(ip_hdr);
					ip_send_packet(packet, len);
				}
			}
		}
	}

	// free(packet);
}

void handle_mospf_packet(iface_info_t *iface, char *packet, int len)
{
	struct iphdr *ip = (struct iphdr *)(packet + ETHER_HDR_SIZE);
	struct mospf_hdr *mospf = (struct mospf_hdr *)((char *)ip + IP_HDR_SIZE(ip));

	if (mospf->version != MOSPF_VERSION) {
		log(ERROR, "received mospf packet with incorrect version (%d)", mospf->version);
		return ;
	}
	if (mospf->checksum != mospf_checksum(mospf)) {
		log(ERROR, "received mospf packet with incorrect checksum");
		return ;
	}
	if (ntohl(mospf->aid) != instance->area_id) {
		log(ERROR, "received mospf packet with incorrect area id");
		return ;
	}

	// log(DEBUG, "received mospf packet, type: %d", mospf->type);

	switch (mospf->type) {
		case MOSPF_TYPE_HELLO:
			handle_mospf_hello(iface, packet, len);
			break;
		case MOSPF_TYPE_LSU:
			handle_mospf_lsu(iface, packet, len);
			break;
		default:
			log(ERROR, "received mospf packet with unknown type (%d).", mospf->type);
			break;
	}
}
