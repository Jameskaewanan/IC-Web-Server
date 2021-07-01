#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>
#include "parse.h"
#include "connection.h"

#define MAXBUF 4096
#define MAX_HEADER_BUF 8192

char rootFolder[MAXBUF];
char listenPort[MAXBUF];
typedef struct sockaddr SA;

void respond_all(int connFd, char *uri, char *mime, char* method) {

    char buf[MAXBUF];

    if ((strcasecmp(method, "GET") != 0) && strcasecmp(method, "HEAD") != 0) {
        sprintf(buf,
                "HTTP/1.1 501 Method Unimplemented\r\n"
                "Server: ICWS\r\n"
                "Connection: closed\r\n\r\n");

        write_all(connFd, buf, strlen(buf));
    }

    else {

        int uriFd = open(uri, O_RDONLY);

        if (uriFd < 0) {
            sprintf(buf,
                    "HTTP/1.1 404 File Not Found\r\n"
                    "Server: ICWS\r\n"
                    "Connection: closed\r\n\r\n");

            write_all(connFd, buf, strlen(buf));
            return;
        }

        struct stat fstatbuf;
        fstat(uriFd, &fstatbuf);
        sprintf(buf,
                "HTTP/1.1 200 OK\r\n"
                "Server: ICWS\r\n"
                "Connection: closed\r\n"
                "Content-length: %lu\r\n"
                "Content-type: %s\r\n\r\n",
                fstatbuf.st_size, mime);

        write_all(connFd, buf, strlen(buf));

        if (strcasecmp(method, "GET") == 0) {
            ssize_t count ;

            while ((count = read(uriFd, buf, MAXBUF)) > 0)
                write_all(connFd, buf, count);
        }

        close(uriFd);

    }

}

ssize_t getBuffer(int connFd, char* buf) {
    int count = 0;
    int numRead;
    char eof_limiter[] = {'\r','\n'};
    int eof_pointer = 0;
    char *bufp = buf;
    printf("Accepting Requests...\n");
    while(count <= MAX_HEADER_BUF) {

        if((numRead = read(connFd, bufp, 1)) > 0) {
            count += numRead;
            if(count > MAX_HEADER_BUF) return -1;
            bufp += numRead;
            if(eof_limiter[eof_pointer % 2] == buf[count-1]) {
                if(++eof_pointer == 4)
                    break;
            }
            else
                eof_pointer = 0;
        }
        else if(numRead == 0) {
            printf("Client terminate connection...\n");
            break;
        }
        else {
            printf("Ops error..\n");
            return -1;
        }
    }
    bufp = '\0';
    return count;
}

void serve_http(int connFd, char* rootFolder) {

    char buf[MAXBUF+666];
    char* ext;

    ssize_t requestBuffer = getBuffer(connFd, buf);

    Request *request;

    if(requestBuffer > 0)
        request = parse(buf, requestBuffer, connFd);

    char dir[MAXBUF];
    char* method = request->http_method;
    char* uri = request->http_uri;

    strcpy(dir, rootFolder);
    strcat(dir, uri);

    ext = strrchr(dir, '.');
    ext++;

    char *mimeType;

    if (strcmp(ext,"html") == 0)
        mimeType = "text/html";
    else if (strcmp(ext,"css") == 0)
        mimeType = "text/css";
    else if (strcmp(ext,"plain") == 0)
        mimeType = "text/plain";
    else if (strcmp(ext,"js") == 0)
        mimeType = "text/javascript";
    else if (strcmp(ext,"png") == 0)
        mimeType = "image/png";
    else if (strcmp(ext,"gif") == 0)
        mimeType = "image/gif";
    else if (strcmp(ext,"jpg") == 0 || strcmp(ext,"jpeg") == 0)
        mimeType = "image/jpg";
    else
        mimeType = "";

    respond_all(connFd, dir, mimeType, method);
}

void getParameters(int argc, char** argv) {
    int check;

    static struct option long_options[] = {
    {"port", required_argument, NULL, 'p'},
    {"root", required_argument, NULL, 'r'},
    {NULL, 0, NULL, 0}};

    while ((check = getopt_long(argc, argv, "p:r:", long_options, NULL)) != -1) {
        switch (check) {
            case 'p':
                printf("port: %s\n", optarg);
                strcpy(listenPort, optarg);
                break;
            case 'r':
                printf("root: %s\n", optarg);
                strcpy(rootFolder, optarg);
                break;
        }
    }
}


int main_loop() {

    int listenFd = open_listenfd(listenPort);
    printf("Initialising Server...\n");

    for (;;) {
        struct sockaddr_storage clientAddr;
        socklen_t clientLen = sizeof(struct sockaddr_storage);

        int connFd = accept(listenFd, (SA *)&clientAddr, &clientLen);
        printf("connFd: %d\n", connFd);

        if (connFd < 0) {
            fprintf(stderr, "Failed to accept\n");
            continue;
        }

        char hostBuf[MAXBUF], svcBuf[MAXBUF];

        if (getnameinfo((SA *)&clientAddr, clientLen, hostBuf, MAXBUF, svcBuf, MAXBUF, 0) == 0)
            printf("Connection from %s:%s\n", hostBuf, svcBuf);
        else
            printf("Connection from ?UNKNOWN?\n");

        serve_http(connFd, rootFolder);
        close(connFd);
    }
}

int main(int argc, char **argv) {
    getParameters(argc, argv);
    main_loop();
    return 0;
}