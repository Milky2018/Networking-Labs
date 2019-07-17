#include "tcp.h"
#include "tcp_timer.h"
#include "tcp_sock.h"
#include "tcp_retrans.h"

#include <stdio.h>
#include <unistd.h>

static struct list_head timer_list;

// scan the timer_list, find the tcp sock which stays for at 2*MSL, release it
void tcp_scan_timer_list()
{
	// fprintf(stdout, "TODO: implement %s please.\n", __FUNCTION__);
	struct tcp_timer *t, *q;
	list_for_each_entry_safe(t, q, &timer_list, list) {
		t->timeout -= TCP_TIMER_SCAN_INTERVAL;
		if (t->timeout <= 0) {
			if (t->type == TIME_WAIT) {
				list_delete_entry(&t->list);

				struct tcp_sock *tsk = timewait_to_tcp_sock(t);
				if (!tsk->parent) {
					tcp_bind_unhash(tsk);
				}
				tcp_set_state(tsk, TCP_CLOSED);
				free_tcp_sock(tsk);
			} else if (t->type == RETRANS) {
				struct tcp_sock *tsk = retranstimer_to_tcp_sock(t);
				if (!list_empty(&tsk->send_buf)) {
					struct send_buf_packet *sbp = 
					    list_entry(tsk->send_buf.next, struct send_buf_packet, list);
					send_buf_retrans(sbp);
				}
				t->timeout = 2 * TCP_RETRANS_INTERVAL_INITIAL; // temp deal
			}
		}
	}
}

// set the timewait timer of a tcp sock, by adding the timer into timer_list
void tcp_set_timewait_timer(struct tcp_sock *tsk)
{
	// fprintf(stdout, "TODO: implement %s please.\n", __FUNCTION__);
	struct tcp_timer *timer = &tsk->timewait;

	timer->type = TIME_WAIT;
	timer->enable = 1;
	timer->timeout = TCP_TIMEWAIT_TIMEOUT;
	list_add_tail(&timer->list, &timer_list);
}

void tcp_set_retrans_timer(struct tcp_sock *tsk)
{
	struct tcp_timer *timer = &tsk->retrans_timer;

	timer->type = RETRANS;
	timer->enable = 1;
	timer->timeout = TCP_RETRANS_INTERVAL_INITIAL;
	list_add_tail(&timer->list, &timer_list);
}

void update_retrans_timer(struct tcp_sock *tsk)
{
	tsk->retrans_timer.timeout = TCP_RETRANS_INTERVAL_INITIAL;
}

void tcp_unset_retrans_timer(struct tcp_sock *tsk)
{
	struct tcp_timer *timer = &tsk->retrans_timer;

	timer->enable = 0;
	list_delete_entry(&timer->list);
}

// scan the timer_list periodically by calling tcp_scan_timer_list
void *tcp_timer_thread(void *arg)
{
	init_list_head(&timer_list);
	while (1) {
		usleep(TCP_TIMER_SCAN_INTERVAL);
		tcp_scan_timer_list();
	}

	return NULL;
}
