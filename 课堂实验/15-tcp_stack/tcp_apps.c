#include "tcp_sock.h"

#include "log.h"

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>

#define TRANS 2

#if TRANS == 1

void *tcp_server(void *arg)
{
	u16 port = *(u16 *)arg;
	struct tcp_sock *tsk = alloc_tcp_sock();

	struct sock_addr addr;
	addr.ip = htonl(0);
	addr.port = port;
	if (tcp_sock_bind(tsk, &addr) < 0) {
		log(ERROR, "tcp_sock bind to port %hu failed", ntohs(port));
		exit(1);
	}

	if (tcp_sock_listen(tsk, 3) < 0) {
		log(ERROR, "tcp_sock listen failed");
		exit(1);
	}

	log(DEBUG, "listen to port %hu.", ntohs(port));

	struct tcp_sock *csk = tcp_sock_accept(tsk);

	log(DEBUG, "accept a connection.");

	char rbuf[1001];
	int rlen = 0;
	fopen("./server-output.dat", "w");
	while (true) {
		rlen = tcp_sock_read(csk, rbuf, 500);
		if (rlen == 0) {
			log(DEBUG, "tcp_sock_read return 0, finish transmission.");
			break;
		} 
		else if (rlen > 0) {
			FILE *file = fopen("./server-output.dat", "a");
			printf("rlen: %d\n", rlen);
			// rbuf[rlen] = '\0'; 
			// puts(rbuf);
			// printf("wlen: %ld\n", fwrite(rbuf, rlen, 1, file));
			fwrite(rbuf, rlen, 1, file);
			fclose(file);
		}
		else {
			log(DEBUG, "tcp_sock_read return negative value, something goes wrong.");
			exit(1);
		}
	}

	log(DEBUG, "close this connection.");

	tcp_sock_close(csk);
	
	return NULL;
}

void *tcp_client(void *arg)
{
	struct sock_addr *skaddr = arg;

	struct tcp_sock *tsk = alloc_tcp_sock();

	if (tcp_sock_connect(tsk, skaddr) < 0) {
		log(ERROR, "tcp_sock connect to server ("IP_FMT":%hu)failed.", \
				NET_IP_FMT_STR(skaddr->ip), ntohs(skaddr->port));
		exit(1);
	}

	FILE *file = fopen("./client-input.dat", "r");

	char rbuf[1001];
	int rlen = fread(rbuf, 1, 1000, file);

	while (rlen > 0) {
		printf("rlen: %d\n", rlen);
		// rbuf[rlen] = '\0';
		// puts(rbuf);
		tcp_sock_write(tsk, rbuf, rlen);
		rlen = fread(rbuf, 1, 1000, file);
	}
	tcp_sock_write(tsk, rbuf, 0);

	fclose(file);

	tcp_sock_close(tsk);

	return NULL;
}

#elif TRANS == 2

typedef struct letter_info {
	int num[26];
}__attribute__((packed)) letter_info_t;

typedef struct letter_conf {
	int start;
	int end;
}__attribute__((packed)) letter_conf_t;

void *tcp_worker(void *arg)
{
	struct tcp_sock *tsk = arg;
	FILE *file = fopen("./war_and_peace.txt", "r");
	int size = sizeof(letter_conf_t);
	char rbuf[1000];
	int rlen = tcp_sock_read(tsk, rbuf, size);
	log(DEBUG, "read %d bytes");
	letter_conf_t *conf = (letter_conf_t *)rbuf;

	fseek(file, conf->start, SEEK_SET);
	
	letter_info_t info;
	for (int i = 0; i < 26; i++) {
		info.num[i] = 0;
	}

	int block = conf->end - conf->start;
	char *book = malloc(block);
	rlen = fread(book, 1, block, file);
	for (int i = 0; i < rlen; i++) {
		char c = book[i];
		if (c >= 'a' && c <= 'z') {
			info.num[c - 'a']++;
		} else if (c >= 'A' && c <= 'Z') {
			info.num[c - 'A']++;
		}
	}

	tcp_sock_write(tsk, (char *)&info, sizeof(info));

	fclose(file);

	tcp_sock_close(tsk);

	return NULL;
}

void *tcp_server(void *arg)
{
	u16 port = *(u16 *)arg;
	struct tcp_sock *tsk = alloc_tcp_sock();
	
	struct sock_addr addr = {
		htonl(0),
		port
	};
	if (tcp_sock_bind(tsk, &addr) < 0) {
		log(ERROR, "tcp_sock bind to port %hu failed", ntohs(port));
		exit(1);
	}

	if (tcp_sock_listen(tsk, 3) < 0) {
		log(ERROR, "tcp_sock listen failed");
		exit(1);
	}

	log(DEBUG, "listen to port %hu.", ntohs(port));

	while (true) {
		struct tcp_sock *csk = tcp_sock_accept(tsk);
		log(DEBUG, "accept a connection.");
		pthread_t thread;
		pthread_create(&thread, NULL, tcp_worker, csk);
	}
}

void *tcp_client(void *arg)
{
	int novel_size = 3227580;
	
	struct sock_addr *skaddr = arg;

	struct sock_addr addr;
	addr.ip = skaddr->ip;

	addr.ip = inet_addr("10.0.0.2");
	// addr.ip = inet_addr("10.0.0.1");
	addr.port = htons(10001);

	struct tcp_sock *tsk = alloc_tcp_sock();
	if (tcp_sock_connect(tsk, &addr) < 0) {
		log(ERROR, "tcp_sock connect to server ("IP_FMT":%hu)failed.", \
				NET_IP_FMT_STR(skaddr->ip), ntohs(skaddr->port));
		exit(1);
	}

	letter_conf_t conf = {
		0, novel_size / 2
	};
	tcp_sock_write(tsk, (char *)&conf, sizeof(conf));
	
	letter_info_t info;
	tcp_sock_read(tsk, (char *)&info, sizeof(info));
	tcp_sock_close(tsk);

	addr.ip = inet_addr("10.0.0.3");
	// addr.ip = inet_addr("10.0.0.1");
	addr.port = htons(10002);

	tsk = alloc_tcp_sock();
	if (tcp_sock_connect(tsk, &addr) < 0) {
		log(ERROR, "tcp_sock connect to server ("IP_FMT":%hu)failed.", \
				NET_IP_FMT_STR(skaddr->ip), ntohs(skaddr->port));
		exit(1);
	}

	conf.start = novel_size / 2;
	conf.end = novel_size;
	tcp_sock_write(tsk, (char *)&conf, sizeof(conf));
	
	letter_info_t info2;
	tcp_sock_read(tsk, (char *)&info2, sizeof(info));
	tcp_sock_close(tsk);

	for (int i = 0; i < 26; i++) {
		printf("A: %d, ", info.num[i] + info2.num[i]);
	}

	return NULL;
}

#else
// tcp server application, listens to port (specified by arg) and serves only one
// connection request
void *tcp_server(void *arg)
{
	u16 port = *(u16 *)arg;
	struct tcp_sock *tsk = alloc_tcp_sock();

	struct sock_addr addr;
	addr.ip = htonl(0);
	addr.port = port;
	if (tcp_sock_bind(tsk, &addr) < 0) {
		log(ERROR, "tcp_sock bind to port %hu failed", ntohs(port));
		exit(1);
	}

	if (tcp_sock_listen(tsk, 3) < 0) {
		log(ERROR, "tcp_sock listen failed");
		exit(1);
	}

	log(DEBUG, "listen to port %hu.", ntohs(port));

	struct tcp_sock *csk = tcp_sock_accept(tsk);

	log(DEBUG, "accept a connection.");

	char rbuf[1001];
	char wbuf[1024];
	int rlen = 0;
	while (1) {
		rlen = tcp_sock_read(csk, rbuf, 1000);
		if (rlen == 0) {
			log(DEBUG, "tcp_sock_read return 0, finish transmission.");
			break;
		} 
		else if (rlen > 0) {
			rbuf[rlen] = '\0';
			sprintf(wbuf, "server echoes: %s", rbuf);
			if (tcp_sock_write(csk, wbuf, strlen(wbuf)) < 0) {
				log(DEBUG, "tcp_sock_write return negative value, something goes wrong.");
				exit(1);
			}
		}
		else {
			log(DEBUG, "tcp_sock_read return negative value, something goes wrong.");
			exit(1);
		}
	}

	log(DEBUG, "close this connection.");

	tcp_sock_close(csk);
	
	return NULL;
}

// tcp client application, connects to server (ip:port specified by arg), each
// time sends one bulk of data and receives one bulk of data 
void *tcp_client(void *arg)
{
	struct sock_addr *skaddr = arg;

	struct tcp_sock *tsk = alloc_tcp_sock();

	if (tcp_sock_connect(tsk, skaddr) < 0) {
		log(ERROR, "tcp_sock connect to server ("IP_FMT":%hu)failed.", \
				NET_IP_FMT_STR(skaddr->ip), ntohs(skaddr->port));
		exit(1);
	}

	char *wbuf = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
	int wlen = strlen(wbuf);
	char rbuf[1001];
	int rlen = 0;

	int n = 2;
	for (int i = 0; i < n; i++) {
		if (tcp_sock_write(tsk, wbuf + i, wlen - n) < 0)
			break;

		rlen = tcp_sock_read(tsk, rbuf, 1000);
		if (rlen == 0) {
			log(DEBUG, "tcp_sock_read return 0, finish transmission.");
			break;
		}
		else if (rlen > 0) {
			rbuf[rlen] = '\0';
			fprintf(stdout, "%s\n", rbuf);
		}
		else {
			log(DEBUG, "tcp_sock_read return negative value, something goes wrong.");
			exit(1);
		}
		sleep(1);
	}

	tcp_sock_close(tsk);

	return NULL;
}

#endif