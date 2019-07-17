#include "tcp.h"
#include "tcp_sock.h"
#include "tcp_timer.h"

#include "log.h"
#include "ring_buffer.h"
#include "tcp_retrans.h"
#include <stdbool.h>

#include <stdlib.h>

// update the snd_wnd of tcp_sock
//
// if the snd_wnd before updating is zero, notify tcp_sock_send (wait_send)
#define OFO 1
bool out_of_order;
static inline void tcp_update_window(struct tcp_sock *tsk, struct tcp_cb *cb)
{
	// u16 old_snd_wnd = tsk->snd_wnd;
	tsk->snd_wnd = cb->rwnd;
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
		log(ERROR, "received packet with invalid seq, drop it. \n\
			seq: %d, seq_end: %d, rcv_end: %d, rcv_nxt: %d", cb->seq, cb->seq_end, rcv_end, tsk->rcv_nxt);
		return 0;
	}
}

void tcp_closed_process(struct tcp_sock *tsk, struct tcp_cb *cb, char *packet)
{
	tcp_send_reset(cb);
	return;
}

void tcp_listen_process(struct tcp_sock *tsk, struct tcp_cb *cb, char *packet)
{
	if (cb->flags == TCP_SYN) {
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
}

void tcp_syn_recv_process(struct tcp_sock *tsk, struct tcp_cb *cb, char *packet)
{
	void tcp_established_process(struct tcp_sock *tsk, struct tcp_cb *cb, char *packet);
	if (cb->flags == TCP_ACK) {
		tsk->snd_wnd = cb->rwnd;
		list_delete_entry(&tsk->list);
		tcp_sock_accept_enqueue(tsk);
		wake_up(tsk->parent->wait_accept);
		tcp_set_state(tsk, TCP_ESTABLISHED);
	} else if (cb->flags == (TCP_ACK | TCP_PSH)) {
		list_delete_entry(&tsk->list);
		tcp_sock_accept_enqueue(tsk);
		wake_up(tsk->parent->wait_accept);
		tcp_set_state(tsk, TCP_ESTABLISHED);
		tcp_established_process(tsk, cb, packet);
	} else {
		log(ERROR, "invalid tcp packet while in state SYN_RECV");
	}
	return;
}

void tcp_syn_sent_process(struct tcp_sock *tsk, struct tcp_cb *cb, char *packet)
{
	if (cb->flags == (TCP_SYN | TCP_ACK)) {
		tsk->snd_una = cb->ack;
		tsk->rcv_nxt = cb->seq_end;
		tsk->snd_wnd = cb->rwnd;

		tcp_send_control_packet(tsk, TCP_ACK);
		tcp_set_state(tsk, TCP_ESTABLISHED);
		wake_up(tsk->wait_connect);
	} else if (cb->flags == TCP_SYN) {
		tsk->snd_una = cb->ack;
		tsk->rcv_nxt = cb->seq_end;
		tcp_send_control_packet(tsk, TCP_SYN | TCP_ACK);
		tcp_set_state(tsk, TCP_SYN_RECV);
	} else {
		log(ERROR, "invalid tcp packet while in state SYN_SENT");
	}
	return;
}

void insert_to_ring(struct tcp_sock *tsk, char *buf, int len)
{
    if (len > 0) {
        wake_up(tsk->wait_recv);
        while (less_than_32b(ring_buffer_free(tsk->rcv_buf), len)) {
            sleep_on(tsk->wait_write);
        }
        write_ring_buffer(tsk->rcv_buf, buf, len);
    }
}

void tcp_established_process(struct tcp_sock *tsk, struct tcp_cb *cb, char *packet)
{
	if (!is_tcp_seq_valid(tsk, cb)) {
		return;
	}
	if (cb->flags == TCP_FIN) {
		tsk->snd_una = cb->ack;
		tsk->rcv_nxt = cb->seq_end;
		tcp_send_control_packet(tsk, TCP_ACK);
		tcp_set_state(tsk, TCP_CLOSE_WAIT);
		wake_up(tsk->wait_recv);
	} else if (cb->flags == (TCP_PSH | TCP_ACK)) {
		tcp_update_window_safe(tsk, cb);
		if (out_of_order) {
			// recv_ofo_buf_add(tsk, packet, cb->pl_len, cb->seq, cb->seq_end);
			tcp_send_control_packet(tsk, TCP_ACK);
		} else {
			tsk->snd_una = cb->ack;
			tsk->rcv_nxt = cb->seq_end;
			insert_to_ring(tsk, cb->payload, cb->pl_len);
			tcp_send_control_packet(tsk, TCP_ACK);
			// recv_ofo_buf_check(tsk, cb->seq_end);
		}
	} else if (cb->flags == TCP_ACK) {
		tcp_update_window_safe(tsk, cb);
	} else {
		log(ERROR, "invalid tcp packet while in state ESTABLISHED");
	}
	return;
}

void tcp_close_wait_process(struct tcp_sock *tsk, struct tcp_cb *cb, char *packet)
{
	log(ERROR, "invalid tcp packet while in state CLOSE_WAIT");
	return;
}

void tcp_last_ack_process(struct tcp_sock *tsk, struct tcp_cb *cb, char *packet)
{
	if (cb->flags == TCP_ACK) {
		tcp_update_window_safe(tsk, cb);
		tcp_set_state(tsk, TCP_CLOSED);
		tcp_unhash(tsk);
	} else {
		log(ERROR, "invalid tcp packet while in state LAST_ACK");
	}
	return;
}

void tcp_fin_wait_1_process(struct tcp_sock *tsk, struct tcp_cb *cb, char *packet)
{
	if (cb->flags == TCP_ACK) {
		tcp_update_window_safe(tsk, cb);
		tcp_set_state(tsk, TCP_FIN_WAIT_2);
	} else if (cb->flags == TCP_FIN) {
		tsk->snd_una = cb->ack;
		tsk->rcv_nxt = cb->seq_end;
		tcp_send_control_packet(tsk, TCP_ACK);
		tcp_set_state(tsk, TCP_CLOSING);
	} else {
		log(ERROR, "invalid tcp packet while in state FIN_WAIT_1");
	}
	return;
}

void tcp_fin_wait_2_process(struct tcp_sock *tsk, struct tcp_cb *cb, char *packet)
{
	if (cb->flags == TCP_FIN) {
		tsk->snd_una = cb->ack;
		tsk->rcv_nxt = cb->seq_end;
		tcp_send_control_packet(tsk, TCP_ACK);
		tcp_set_state(tsk, TCP_TIME_WAIT);
		tcp_set_timewait_timer(tsk);
	} else {
		log(ERROR, "invalid tcp packet while in state FIN_WAIT_2");
	}
}

void tcp_closing_process(struct tcp_sock *tsk, struct tcp_cb *cb, char *packet)
{
	if (cb->flags == TCP_ACK) {
		tcp_update_window_safe(tsk, cb);
		tcp_set_state(tsk, TCP_TIME_WAIT);
		tcp_set_timewait_timer(tsk);
	} else {
		log(ERROR, "invalid tcp packet while in state FIN_WAIT_2");
	}
	return;
}

void tcp_time_wait_process(struct tcp_sock *tsk, struct tcp_cb *cb, char *packet)
{
	log(ERROR, "invalid tcp packet while in state TIME_WAIT");
	return;
}

void (*tcp_state_process[])(struct tcp_sock *tsk, struct tcp_cb *cb, char *packet) = {
	tcp_closed_process, tcp_listen_process, tcp_syn_recv_process, tcp_syn_sent_process,
	tcp_established_process, tcp_close_wait_process, tcp_last_ack_process, tcp_fin_wait_1_process,
	tcp_fin_wait_2_process, tcp_closing_process, tcp_time_wait_process
};

// Process the incoming packet according to TCP state machine. 
void tcp_process(struct tcp_sock *tsk, struct tcp_cb *cb, char *packet)
{
	// fprintf(stdout, "TODO: implement %s please.\n", __FUNCTION__);
	if (cb->flags & TCP_RST) {
		tcp_sock_close(tsk);
		free_tcp_sock(tsk);
		return;
	}

#if OFO == 1
	out_of_order = (cb->seq != tsk->rcv_nxt);
#else
	out_of_order = false;
#endif

	if (cb->flags & TCP_ACK) {
		send_buf_ack(tsk, cb);
	}

	tcp_state_process[tsk->state](tsk, cb, packet);
}
