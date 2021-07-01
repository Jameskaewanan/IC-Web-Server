extern "C" {
    #include <sys/stat.h>
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netdb.h>
    #include <stdio.h>
    #include <stdlib.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <getopt.h>
    #include "parse.h"
    #include "connection.h"
}

#include <iostream>
#include <pthread.h>
#include <time.h>
#include <mutex>
#include <thread>
#include <string>
#include <cstring>
#include <poll.h>
#include "work_queue.cpp"

#define MAXBUF 4096
#define MAX_HEADER_BUF 8192

using namespace std;

char rootFolder[MAXBUF];
char listenPort[MAXBUF];
int threads;
int timeout;
mutex mtx;

typedef struct sockaddr SA;

struct { work_queue workQueue; } shared;

static const char* DAY_NAMES[] = {
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

static const char* MONTH_NAMES[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

char* getDateTime(){
    const int TIME = 29;
    time_t t;
    struct tm tm;
    char* buf = (char *)malloc(TIME+1);
    time(&t);
    gmtime_r(&t, &tm);
    strftime(buf, TIME+1, "%a, %d %b %Y %H:%M:%S GMT", &tm);
    memcpy(buf, DAY_NAMES[tm.tm_wday], 3);
    memcpy(buf, MONTH_NAMES[tm.tm_mon], 3);
    return buf;
}

char *createError(char* buf, const char* text) {
    sprintf(buf,
        "HTTP/1.1 %s\r\n"
        "Server: ICWS\r\n"
        "Connection: closed\r\n\r\n",
        text);

    return buf;
}

char * createResponse(char* buf, unsigned long size, const char* mimeType) {
	char *dateTime = getDateTime();
    sprintf(buf,
        "HTTP/1.1 200 OK\r\n"
		"Date: %s\r\n"
        "Server: ICWS\r\n"
        "Connection: closed\r\n"
        "Content-Length: %lu\r\n"
        "Content-Type: %s\r\n"
		"Last-Modified: %s\r\n\r\n",
        dateTime, size, mimeType, dateTime);

    return buf;
}

void respond_all(int connFd, char *uri, const char* mimeType, char* method) {

    char buf[MAXBUF];

    if ((strcasecmp(method, "GET") != 0) && strcasecmp(method, "HEAD") != 0) {
        createError(buf, "501 Method Unimplemented");
        write_all(connFd, buf, strlen(buf));
    }

    else {

        int uriFd = open(uri, O_RDONLY);

        if (uriFd < 0) {
            createError(buf, "404 File Not Found");
            write_all(connFd, buf, strlen(buf));
            return;
        }

        struct stat fstatbuf;
        fstat(uriFd, &fstatbuf);
        createResponse(buf, fstatbuf.st_size, mimeType);
        write_all(connFd, buf, strlen(buf));

        if (strcasecmp(method, "GET") == 0) {
            ssize_t count ;

            while ((count = read(uriFd, buf, MAXBUF)) > 0)
                write_all(connFd, buf, count);
        }

        if (strcasecmp(method, "HEAD") == 0) {
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
            printf("Connection Terminated.\n");
            break;
        }

        else {
            printf("Error\n");
            return -1;
        }
    }

    *bufp = '\0';
    return count;
}

void errorChecks(int connFd) {

    char buf[MAXBUF+666];
    struct pollfd pfd[1];

    for (;;) {
        pfd[0].fd = connFd;
        pfd[0].events = POLLIN;
        int ret = poll(pfd,1,timeout);

        if (ret < 0) {
            createError(buf, "400 Bad Request");
            write_all(connFd,buf,strlen(buf));
        }

        else if (ret == 0) {
            createError(buf, "408 Request Timeout");
            write_all(connFd,buf,strlen(buf));
        }
        else if ((pfd[0].fd == connFd) && (pfd[0].revents == POLLIN)) {
            ret = read(connFd,buf,MAXBUF);
            break;
        }
    }

}

void serve_http(int connFd, char* rootFolder) {

    char buf[MAXBUF+666];
    char* ext;

    ssize_t requestBuffer = getBuffer(connFd, buf);

    Request *request;

    if(requestBuffer > 0) {
        mtx.lock();
        request = parse(buf, requestBuffer, connFd);
        mtx.unlock();
    }

    char dir[MAXBUF];
    char* method = request->http_method;
    char* uri = request->http_uri;

    strcpy(dir, rootFolder);
    strcat(dir, uri);

    ext = strrchr(dir, '.');
    ext++;

    const char* mimeType;

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
    {"numThreads", required_argument, NULL, 'n'},
    {"timeout", required_argument, NULL, 't'},
    {NULL, 0, NULL, 0}};

    while ((check = getopt_long(argc, argv, "p:r:n:t:", long_options, NULL)) != -1) {
        switch (check) {
            case 'p':
                printf("port: %s\n", optarg);
                strcpy(listenPort, optarg);
                break;
            case 'r':
                printf("root: %s\n", optarg);
                strcpy(rootFolder, optarg);
                break;
            case 'n':
                printf("threads: %s\n", optarg);
                threads = atoi(optarg);
                break;
            case 't':
                printf("timeout: %s\n", optarg);
                timeout = atoi(optarg);
                break;
        }
    }
}

void doWork() {
    for (;;) {
        int w;
        if (shared.workQueue.removeJob(&w)) {

            if (w < 0)
                break;

            serve_http(w,rootFolder);
            close(w);
        }

        else {
            usleep(250000);
        }
    }
}

int main_loop() {

    thread threadWork[threads];

    for (int i = 0; i < threads; i++) {
        threadWork[i] = thread(doWork);
    }

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

        shared.workQueue.addJob(connFd);
    }
}

int main(int argc, char **argv) {
    getParameters(argc, argv);
    main_loop();
    return 0;
}