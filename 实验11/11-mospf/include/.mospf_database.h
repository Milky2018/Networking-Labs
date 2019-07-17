#ifndef __MOSPF_DATABASE_H__
#define __MOSPF_DATABASE_H__

#include "base.h"
#include "list.h"

#include "mospf_proto.h"
#include <stdbool.h>

struct list_head mospf_db;

typedef struct {
	struct list_head list;
	u32	rid;
	u16	seq;
	int nadv;
	int timer;
	struct mospf_lsa *array;
} mospf_db_entry_t;

void init_mospf_db();
mospf_db_entry_t *mospf_db_query(u32 rid);
bool mospf_db_insert(mospf_db_entry_t *entry);
void mospf_db_update(mospf_db_entry_t *entry);
bool mospf_db_remove(u32 rid);
void mospf_db_display();

#define mospf_db_foreach(pos) \
	list_for_each_entry(pos, &mospf_db, list)

#endif
