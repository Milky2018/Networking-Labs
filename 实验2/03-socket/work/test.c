#include<stdio.h>
#include<string.h>
#include<stdlib.h>

#define max 0x2000000

char buf[max];

int main()
{
    char *file = "war_and_peace.txt";
    FILE *fd = fopen(file, "r");
    int count = fread(buf, sizeof(char), max, fd);

    printf("%d\n", count);

    fclose(fd);
}