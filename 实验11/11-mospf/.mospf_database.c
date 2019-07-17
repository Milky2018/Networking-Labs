#include "mospf_database.h"
#include "ip.h"

#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>

struct list_head mospf_db;

void init_mospf_db()
{
	init_list_head(&mospf_db);
}

mospf_db_entry_t *mospf_db_query(u32 rid)
{
	mospf_db_entry_t *entry = NULL;
	list_for_each_entry(entry, &mospf_db, list) {
		if (rid == entry->rid) {
			entry->timer = 0;
			return entry;
		}
	}

	return NULL;
}

bool mospf_db_insert(mospf_db_entry_t *entry)
{
	mospf_db_entry_t *pos = NULL;
	entry->timer = 0;
	list_for_each_entry(pos, &mospf_db, list) {
		if (pos->rid == entry->rid) {
			return false;
		}
	}
	list_add_tail(&entry->list, &mospf_db);
	return true;
}

void mospf_db_update(mospf_db_entry_t *entry)
{
	mospf_db_entry_t *pos = NULL;
	list_for_each_entry(pos, &mospf_db, list) {
		if (pos->rid == entry->rid) {
			mospf_db_remove(pos->rid);
			mospf_db_insert(entry);
			return;
		}
	}
	mospf_db_insert(entry);
}

bool mospf_db_remove(u32 rid)
{
	mospf_db_entry_t *p = NULL, *q = NULL;
	list_for_each_entry_safe(p, q, &mospf_db, list) {
		if (p->rid == rid) {
			list_delete_entry(&p->list);
			free(p->array);
			free(p);
			return true;
		}
	}
	return false;
}

void mospf_db_display() 
{
	mospf_db_entry_t *entry = NULL;
	mospf_db_foreach(entry) {
		fprintf(stdout, "rid: " IP_FMT "\n", HOST_IP_FMT_STR(entry->rid));
		fprintf(stdout, "\tnadv: %d\n", entry->nadv);
		for (int i = 0; i < entry->nadv; i++) {
			fprintf(stdout, "\t" IP_FMT "\t" IP_FMT "\t" IP_FMT "\n", 
				HOST_IP_FMT_STR(entry->array[i].subnet), 
				HOST_IP_FMT_STR(entry->array[i].mask), 
				HOST_IP_FMT_STR(entry->array[i].rid));
		}
	}
}