#ifndef __CONNECTION_
#define __CONNECTION_
#include <unistd.h>

int open_listenfd(char *port);
int open_clientfd(char *hostname, char *port);
ssize_t read_line(int connFd, char *usrbuf, size_t maxlen);
void write_all(int connFd, char *buf, size_t len);

#endif
