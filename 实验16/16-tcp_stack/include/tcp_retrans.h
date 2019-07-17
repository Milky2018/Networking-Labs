#ifndef __TCP_RETRANS_H__
#define __TCP_RETRANS_H__

#include "types.h"
#include "list.h"
#include "tcp.h"
#include "tcp_timer.h"
#include "ring_buffer.h"

#include "synch_wait.h"

enum buf_kind { DATA, SYN, FIN };
struct send_buf_packet {
    int len;
    char *packet; // the whole tcp packet
    struct list_head list;
    u32 seq_end;
    enum buf_kind kind;
};

struct recv_ofo_buf_packet {
    int len;
    char *packet; // only data part
    struct list_head list;
    u32 seq;
    u32 seq_end;
};

void send_buf_add(struct tcp_sock *tsk, char *packet, int len, enum buf_kind kind, u32 seq_end);
void send_buf_remove(struct send_buf_packet *sbp);
void send_buf_retrans(struct send_buf_packet *sbp);
void send_buf_ack(struct tcp_sock *tsk, struct tcp_cb *cb);
void recv_ofo_buf_add(struct tcp_sock *tsk, char *packet, int len, u32 seq, u32 seq_end);
void recv_ofo_buf_check(struct tcp_sock *tsk, u32 seq);
void recv_ofo_buf_reload(struct tcp_sock *tsk, struct recv_ofo_buf_packet *robp);
void insert_to_ring(struct tcp_sock *tsk, char *buf, int len);
void recv_ofo_buf_remove(struct recv_ofo_buf_packet *robp);

#endif