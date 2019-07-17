#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

const int num = 3;
void master_do(int);

int main(int argc, const char *argv[])
{
    int master_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (master_fd < 0) {
        perror("create socket failed");
		return -1;
    }

    struct sockaddr_in server = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(12345)
    };

    if (bind(master_fd, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("bind failed");
        return -1;
    }

    listen(master_fd, 1024);

    int addrlen = sizeof(struct sockaddr_in);
    struct sockaddr_in client;
    int cd = accept(master_fd, (struct sockaddr *)&client, (socklen_t *)addrlen);

    master_do(cd);
     
    return 0;
}

void master_do(int cd)
{
    char *novel = "./war_and_peace.txt";
    int len = strlen(novel);

}