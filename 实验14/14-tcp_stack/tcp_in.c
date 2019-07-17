#include "tcp.h"
#include "tcp_sock.h"
#include "tcp_timer.h"

#include "log.h"
#include "ring_buffer.h"

#include <stdlib.h>
// update the snd_wnd of tcp_sock
//
// if the snd_wnd before updating is zero, notify tcp_sock_send (wait_send)
static inline void tcp_update_window(struct tcp_sock *tsk, struct tcp_cb *cb)
{
	u16 old_snd_wnd = tsk->snd_wnd;
	tsk->snd_wnd = cb->rwnd;
	if (old_snd_wnd == 0)
		wake_up(tsk->wait_send);
}

// update the snd_wnd safely: cb->ack should be between snd_una and snd_nxt
static inline void tcp_update_window_safe(struct tcp_sock *tsk, struct tcp_cb *cb)
{
	if (less_or_equal_32b(tsk->snd_una, cb->ack) && less_or_equal_32b(cb->ack, tsk->snd_nxt))
		tcp_update_window(tsk, cb);
}

#ifndef max
#	define max(x,y) ((x)>(y) ? (x) : (y))
#endif

// check whether the sequence number of the incoming packet is in the receiving
// window
static inline int is_tcp_seq_valid(struct tcp_sock *tsk, struct tcp_cb *cb)
{
	u32 rcv_end = tsk->rcv_nxt + max(tsk->rcv_wnd, 1);
	if (less_than_32b(cb->seq, rcv_end) && less_or_equal_32b(tsk->rcv_nxt, cb->seq_end)) {
		return 1;
	}
	else {
		log(ERROR, "received packet with invalid seq, drop it.");
		return 0;
	}
}

// Process the incoming packet according to TCP state machine. 
void tcp_process(struct tcp_sock *tsk, struct tcp_cb *cb, char *packet)
{
	// fprintf(stdout, "TODO: implement %s please.\n", __FUNCTION__);
	u8 flags = cb->flags;

	// if (!is_tcp_seq_valid(tsk, cb)) {
	// 	return;
	// }

	if (flags & TCP_RST) {
		tcp_sock_close(tsk);
		free_tcp_sock(tsk);
		return;
	}

	log(DEBUG, IP_FMT ":%hu current state is %s.", \
			HOST_IP_FMT_STR(tsk->sk_sip), tsk->sk_sport, \
			tcp_state_str[tsk->state]);

	switch (tsk->state) {
		case TCP_CLOSED:
			tcp_send_reset(cb);
			return;
		case TCP_LISTEN:
			if (flags & TCP_SYN) {
				struct tcp_sock *new = alloc_tcp_sock();
				new->parent = tsk;
				new->sk_dip = cb->saddr;
				new->sk_dport = cb->sport;
				new->sk_sip = cb->daddr;
				new->sk_sport = cb->dport;

				new->iss = tcp_new_iss();
				new->snd_una = new->iss;
				new->snd_nxt = new->iss;
				new->rcv_nxt = cb->seq + 1;

				tcp_set_state(new, TCP_SYN_RECV);

				tcp_hash(new);
				list_add_head(&new->list, &tsk->listen_queue);

				tcp_send_control_packet(new, TCP_ACK | TCP_SYN);
			} else {
				log(ERROR, "invalid tcp packet while in state LISTEN");
			}
			return;
		case TCP_SYN_RECV:
			if (flags & TCP_ACK) {
				list_delete_entry(&tsk->list);
				tcp_sock_accept_enqueue(tsk);
				wake_up(tsk->parent->wait_accept);
				tcp_set_state(tsk, TCP_ESTABLISHED);
			} else {
				log(ERROR, "invalid tcp packet while in state SYN_RECV");
			}
			return;
		case TCP_SYN_SENT:
			if (flags & (TCP_SYN | TCP_ACK)) {
				tsk->rcv_nxt = cb->seq + 1;
				tsk->snd_nxt = cb->ack;

				tcp_send_control_packet(tsk, TCP_ACK);
				tcp_set_state(tsk, TCP_ESTABLISHED);
				wake_up(tsk->wait_connect);
			} else if (flags & TCP_SYN) {
				tcp_send_control_packet(tsk, TCP_SYN | TCP_ACK);
				tcp_set_state(tsk, TCP_SYN_RECV);
			} else {
				log(ERROR, "invalid tcp packet while in state SYN_SENT");
			}
			return;
		case TCP_ESTABLISHED:
			if (flags & TCP_FIN) {
				tsk->rcv_nxt = cb->seq_end;
				tcp_send_control_packet(tsk, TCP_ACK);
				tcp_set_state(tsk, TCP_CLOSE_WAIT);
			} else {
				log(ERROR, "invalid tcp packet while in state ESTABLISHED");
			}
			return;
		case TCP_CLOSE_WAIT:
			log(ERROR, "invalid tcp packet while in state CLOSE_WAIT");
			return;
		case TCP_LAST_ACK:
			if (flags & TCP_ACK) {
				tcp_set_state(tsk, TCP_CLOSED);
				tcp_unhash(tsk);
			} else {
				log(ERROR, "invalid tcp packet while in state LAST_ACK");
			}
			return;
		case TCP_FIN_WAIT_1:
			if (flags & TCP_ACK) {
				tcp_set_state(tsk, TCP_FIN_WAIT_2);
			} else if (flags & TCP_FIN) {
				tcp_send_control_packet(tsk, TCP_ACK);
				tcp_set_state(tsk, TCP_CLOSING);
			} else {
				log(ERROR, "invalid tcp packet while in state FIN_WAIT_1");
			}
			return;
		case TCP_FIN_WAIT_2:
			if (flags & TCP_FIN) {
				tsk->rcv_nxt = cb->seq_end;
				tcp_send_control_packet(tsk, TCP_ACK);
				tcp_set_state(tsk, TCP_TIME_WAIT);
				tcp_set_timewait_timer(tsk);
			} else {
				log(ERROR, "invalid tcp packet while in state FIN_WAIT_2");
			}
		case TCP_CLOSING:
			if (flags & TCP_ACK) {
				tcp_set_state(tsk, TCP_TIME_WAIT);
				tcp_set_timewait_timer(tsk);
			} else {
				log(ERROR, "invalid tcp packet while in state FIN_WAIT_2");
			}
			return;
		case TCP_TIME_WAIT:
			log(ERROR, "invalid tcp packet while in state TIME_WAIT");
			return;
		default:
			break;
		}
}
