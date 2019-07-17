#include "tcp.h"
#include "tcp_sock.h"
#include "ip.h"
#include "ether.h"
#include "tcp_retrans.h"
#include "log.h"

#include "list.h"

#include <stdlib.h>
#include <string.h>

void send_buf_add(struct tcp_sock *tsk, char *packet, int len, enum buf_kind kind, u32 seq_end)
{
    struct send_buf_packet *new_packet = malloc(sizeof(struct send_buf_packet));
    new_packet->packet = malloc(len);
    new_packet->len = len;
    new_packet->kind = kind;
    new_packet->seq_end = seq_end;
    memcpy(new_packet->packet, packet, len);
    list_add_tail(&new_packet->list, &tsk->send_buf);
    if (tsk->retrans_timer.enable == 0) {
        tcp_set_retrans_timer(tsk);
    }
    log(DEBUG, "send buffer add: %d, type: %d", seq_end, kind);
}

void send_buf_remove(struct send_buf_packet *sbp)
{
    log(DEBUG, "send buffer delete: %d, type: %d", sbp->seq_end, sbp->kind);
    list_delete_entry(&sbp->list);
    free(sbp->packet);
    free(sbp);
}

void send_buf_retrans(struct send_buf_packet *sbp)
{
    log(DEBUG, "send buffer retrans: %d, type: %d", sbp->seq_end, sbp->kind);
    char *packet = malloc(sbp->len);
    memcpy(packet, sbp->packet, sbp->len);
    ip_send_packet(packet, sbp->len);
}

void send_buf_ack(struct tcp_sock *tsk, struct tcp_cb *cb) 
{
    struct send_buf_packet *sbp, *q;
    list_for_each_entry_safe(sbp, q, &tsk->send_buf, list) {
        if (less_or_equal_32b(sbp->seq_end, cb->ack)) {
            send_buf_remove(sbp);
            update_retrans_timer(tsk);
        }
    }
}

void recv_ofo_buf_add(struct tcp_sock *tsk, char *packet, int len, u32 seq, u32 seq_end)
{
    struct recv_ofo_buf_packet *robp = malloc(sizeof(struct recv_ofo_buf_packet));
    robp->len = len;
    robp->seq = seq;
    robp->seq_end = seq_end;
    robp->packet = malloc(len);
    memcpy(robp->packet, packet, len);
    list_add_tail(&robp->list, &tsk->rcv_ofo_buf);
}

void recv_ofo_buf_check(struct tcp_sock *tsk, u32 seq) 
{
    struct recv_ofo_buf_packet *robp, *q;
    list_for_each_entry_safe(robp, q, &tsk->rcv_ofo_buf, list) {
        if (robp->seq == seq) {
            log(DEBUG, "seq: %d now is reloaded", seq);
            recv_ofo_buf_reload(tsk, robp);
            recv_ofo_buf_check(tsk, robp->seq_end);
            return;
        } else if (less_than_32b(robp->seq, seq)) {
            recv_ofo_buf_remove(robp);
        }
    }
}

void recv_ofo_buf_reload(struct tcp_sock *tsk, struct recv_ofo_buf_packet *robp)
{
    insert_to_ring(tsk, robp->packet, robp->len);
    recv_ofo_buf_remove(robp);
}

void recv_ofo_buf_remove(struct recv_ofo_buf_packet *robp)
{
    list_delete_entry(&robp->list);
    free(robp->packet);
    free(robp);
}