#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include "connection.h"

#define MAXBUF 8192

int main (int argc, char* argv[]) {

typedef struct sockaddr SA;

void write_logic(int connFd, int outputFd)
{
    ssize_t bytesRead;
    char buf[MAXBUF];

    while ((bytesRead = read(connFd, buf, MAXBUF)) > 0)
    {
        ssize_t numToWrite = bytesRead;
        char *writeBuf = buf;
        while (numToWrite > 0)
        {
            ssize_t numWritten = write(outputFd, writeBuf, numToWrite);
            if (numWritten < 0)
            {
                fprintf(stderr, "ERROR writing, meh\n");
                break;
            }
            numToWrite -= numWritten;
            writeBuf += numWritten;
        }
    }
    printf("DEBUG: Connection closed\n");
}
void respond_all(int connFd, char *uri, char *mime)
{
    char buf[MAXBUF];
    int uriFd = open(uri, O_RDONLY);
    char *msg = "404 Not Found";
    if (uriFd < 0)
    {
        sprintf(buf,
                "HTTP/1.1 404 Not Found\r\n"
                "Server: Micro\r\n"
                "Connection: close\r\n\r\n");
        write_all(connFd, buf, strlen(buf));
        write_all(connFd, msg, strlen(msg));
        return;
    }
    struct stat fstatbuf;
    fstat(uriFd, &fstatbuf);
    sprintf(buf,
            "HTTP/1.1 200 OK\r\n"
            "Server: Micro\r\n"
            "Connection: close\r\n"
            "Content-length: %lu\r\n"
            "Content-type: %s\r\n\r\n",
            fstatbuf.st_size, mime);
    write_all(connFd, buf, strlen(buf));
    write_logic(uriFd, connFd);
}

void serve_http(int connFd, char *rootFolder)
{
    char buf[MAXBUF];


    if (!read_line(connFd, buf, MAXBUF))
        return; /* Quit if we can't read the first line */
    /* [METHOD] [URI] [HTTPVER] */
    char method[MAXBUF], uri[MAXBUF], httpVer[MAXBUF];
    sscanf(buf, "%s %s %s", method, uri, httpVer);
    char newPath[80];
    if (strcasecmp(method, "GET") == 0)
    {
        if (uri[0] == '/')
        {
            sprintf(newPath, "%s%s", rootFolder, uri);
            if (strstr(uri, "html") != NULL)
            {
                respond_all(connFd, newPath, "text/html");
            }
            else if (strstr(uri, "jpg") != NULL || strstr(uri, "jpeg") != NULL)
            {
                respond_all(connFd, newPath, "image/jpeg");
            }
            else
            {
                respond_all(connFd, newPath, NULL);
            }
        }
    }
    else
    {
        // respond_all(connFd, newPath, NULL);
        printf("LOG: Unknown request\n");
    }
}

int main(int argc, char *argv[])
{
    int listenFd = open_listenfd(argv[2]);
    char *rootFolder = argv[4];

    for (;;)
    {
        struct sockaddr_storage clientAddr;
        socklen_t clientLen = sizeof(struct sockaddr_storage);

        int connFd = accept(listenFd, (SA *)&clientAddr, &clientLen);
        printf("connFd: %d", connFd);
        if (connFd < 0)
        {
            fprintf(stderr, "Failed to accept\n");
            continue;
        }

        char hostBuf[MAXBUF], svcBuf[MAXBUF];
        if (getnameinfo((SA *)&clientAddr, clientLen,
                        hostBuf, MAXBUF, svcBuf, MAXBUF, 0) == 0)
            printf("Connection from %s:%s\n", hostBuf, svcBuf);
        else
            printf("Connection from ?UNKNOWN?\n");

        serve_http(connFd, rootFolder);
        close(connFd);
    }

    return 0;
}



}
