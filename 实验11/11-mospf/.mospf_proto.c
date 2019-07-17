#include "mospf_proto.h"
#include "base.h"
#include <arpa/inet.h>
#include <string.h>

extern ustack_t *instance;

void mospf_init_hdr(struct mospf_hdr *mospf, u8 type, u16 len, u32 rid, u32 aid)
{
	mospf->version = MOSPF_VERSION;
	mospf->type = type;
	mospf->len = htons(len);
	mospf->rid = htonl(rid);
	mospf->aid = htonl(aid);
	mospf->padding = 0;
}

void mospf_init_hello(struct mospf_hello *hello, u32 mask)
{
	hello->mask = htonl(mask);
	hello->helloint = htons(MOSPF_DEFAULT_HELLOINT);
	hello->padding = 0;
}

void mospf_init_lsu(struct mospf_lsu *lsu, u32 nadv)
{
	lsu->seq = htons(instance->sequence_num);
	lsu->unused = 0;
	lsu->ttl = MOSPF_MAX_LSU_TTL;
	lsu->nadv = htonl(nadv);
}

void mospf_init_hdr_hello(struct mospf_hdr_hello *mospf, u32 rid, u32 mask) 
{
	mospf_init_hdr(&mospf->hdr, MOSPF_TYPE_HELLO, MOSPF_HDR_HELLO_SIZE, rid, 0);
	mospf_init_hello(&mospf->body, mask);
	mospf->hdr.checksum = mospf_checksum(&mospf->hdr);
}

void mospf_init_hdr_lsu(struct mospf_hdr_lsu *mospf, u32 rid, u32 nadv, struct mospf_lsa *array)
{
	u16 length = MOSPF_HDR_LSU_SIZE + nadv * MOSPF_LSA_SIZE;
	mospf_init_hdr(&mospf->hdr, MOSPF_TYPE_LSU, length, rid, 0);
	mospf_init_lsu(&mospf->body, nadv);
	struct mospf_lsa *lsa = get_mospf_lsa(mospf);
	for (u32 i = 0; i < nadv; i++) {
		lsa[i].subnet = htonl(array[i].subnet);
		lsa[i].mask = htonl(array[i].mask);
		lsa[i].rid = htonl(array[i].rid);
	}
	mospf->hdr.checksum = mospf_checksum(&mospf->hdr);
}

// struct mospf_hdr_hello *new_mospf_hdr_hello(u32 rid, u32 mask)
// {
// 	struct mospf_hdr_hello *new = malloc(MOSPF_HDR_HELLO_SIZE);
// 	mospf_init_hdr(&new->hdr, MOSPF_TYPE_HELLO, MOSPF_HDR_HELLO_SIZE, rid, 0);
// 	mospf_init_hello(&new->body, mask);
// 	return new;
// }

// struct mospf_hdr_lsu *new_mospf_hdr_lsu(u32 rid, u32 nadv, struct mospf_lsa *array)
// {
// 	u32 length = MOSPF_HDR_LSU_SIZE + nadv * MOSPF_LSA_SIZE;
// 	struct mospf_hdr_lsu *new = malloc(length);
// 	mospf_init_hdr(&new->hdr, MOSPF_TYPE_LSU, length, rid, 0);
// 	mospf_init_lsu(&new->body, nadv);
// 	struct mospf_lsa *lsa = get_mospf_lsa(new);
// 	for (u32 i = 0; i < nadv; i++) {
// 		lsa[i].subnet = htonl(array[i].subnet);
// 		lsa[i].mask = htonl(array[i].mask);
// 		lsa[i].rid = htonl(array[i].rid);
// 	}
// 	return new;
// }