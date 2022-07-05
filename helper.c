#include "helper.h"

#include <fcntl.h>
#include <math.h>
#include <regex.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <sys/file.h>
#include <pthread.h>

uint64_t strtoint64(char *number) {
    char *last;
    uint64_t num = strtol(number, &last, 10);
    if (num <= 0 || num > UINT64_MAX || *last != '\0')
        return 0;
    return num;
}

ssize_t readfile(int fd, /*pthread_mutex_t *mutex,*/ char *str, uint64_t cnt, char rmethod[],
    char ruri[], uint64_t *remaining, int *status, uint64_t *requestid) {
    //pthread_mutex_lock(mutex);
    char *a = strstr(str, "\r\n");
    uint64_t indexheader = a - str;
    char first[2048] = "";
    strncpy(first, &str[0], indexheader);

    char *method;
    char *uri;
    char *version;
    char *tfirst = str + (int) (first - str);
    char *tok;
    for (int i = 0; i < 3; i++) {
        tok = strtok_r(tfirst, " ", &tfirst);
        if (i == 0) {
            method = tok;
        } else if (i == 1) {
            uri = tok;
        } else {
            version = tok;
        }
    }
    if (*(uri) == '/') {
        uri += 1;
    } else {
        *status = 400;
        return -1;
    }
    for (int i = 0; i < 2048; i++) { // copy to rmethod
        rmethod[i] = *(method + i);
        ruri[i] = *(uri + i);
    }
    if (strcmp(method, "GET") && strcmp(method, "PUT") && strcmp(method, "APPEND")) {
        *status = 501;
        return -1;
    }
    if (strcmp(version, "HTTP/1.1") != 0) {
        *status = 400;
        return -1;
    }

    uint64_t contentlength;
    char second[2048] = "";
    char *b = strstr(str, "\r\n\r\n");
    uint64_t indexmessage = b - str;
    //const char s[3] = "\r\n";
    char *token;
    char *headers[2048] = { "" };
    uint64_t numheaders = 0;
    char *key;
    char *value;
    if ((indexheader == indexmessage && !(strcmp(method, "GET") == 0)) || b == NULL) {
        *status = 400;
        return -1;
    }
    if (!(indexmessage == indexheader
            && strcmp(method, "GET") == 0)) { // NOT(is GET and empty header)
        strncpy(second, &str[indexheader + 2], indexmessage - indexheader - 2);
        char *tsecond = str + (int) (second - str);
        contentlength = 0;
        while ((token = strtok_r(tsecond, "\r\n", &tsecond))) {
            headers[numheaders++] = token;
            //fprintf(stderr, "before\n");
            key = strtok_r(token, " ", &token);
            if (*(key + strlen(key) - 1) == ':') {
                if (strcmp(key, "Content-Length:") == 0) {
                    value = strtok_r(token, " ", &token);
                    uint64_t c = strtoint64(value);
                    if (c != 0) {
                        //fprintf(stderr, "%lu\n", c);
                        contentlength = c;
                    }
                } else if (strcmp(key, "Request-Id:") == 0) {
                    value = strtok_r(token, " ", &token);
                    uint64_t c = strtoint64(value);
                    if (c != 0) {
                        *requestid = c;
                    }
                }
            } else {
                *status = 400;
                return -1;
            }
        }
    }

    struct stat sb;
    char buffer[1024] = "";
    uint64_t byteread = 0;
    int file;
    if (strcmp(method, "GET") == 0) { // GET
        file = open(uri, O_RDONLY);
        if (errno == EACCES || errno == EISDIR) {
            *status = 403;
            return -1;
        }
        if (file == -1) {
            *status = 404;
        } else if (fstat(file, &sb) == -1) {
            *status = 404;
        } else {
            if (S_ISREG(sb.st_mode) == false) {
                *status = 403;
                close(file);
                return -1;
            }
            *status = 0;
            flock(file, LOCK_EX);
            // pthread_mutex_lock(mute);
            write(fd, "HTTP/1.1 200 OK\r\n", 17);
            char filesize[30];
            sprintf(filesize, "Content-Length: %lu\r\n\r\n", sb.st_size);
            write(fd, filesize, 18 + (int) (log10(sb.st_size) + 3));
            while ((byteread = read(file, buffer, 1024)) > 0) {
                write(fd, buffer, byteread);
            }
            flock(file, LOCK_UN);
            // pthread_mutex_unlock(mute);
        }
        if (file > 0)
            close(file);
    } else if (strcmp(method, "PUT") == 0) { // PUT
        if (stat(uri, &sb) == -1) { // file not exist
            if ((file = open(uri, O_CREAT | O_WRONLY | O_TRUNC, 0644)) == -1) {
                printf("issue creating file\n");
            } else {
                *status = 201;
            }
        } else {
            if (S_ISREG(sb.st_mode) == false) {
                *status = 403;
                return -1;
            }
            if ((file = open(uri, O_TRUNC | O_WRONLY, 0644)) == -1) {
                printf("issue opening file\n");
            } else {
                *status = 200;
            }
        }
        if (errno == EACCES || errno == EISDIR) {
            *status = 403;
            close(file);
            return -1;
        }
        char *ret = str;
        ret += indexmessage + 4;
        if (contentlength > cnt - indexmessage - 4) { // if contentlength is greater than given
            *remaining = contentlength - (cnt - indexmessage - 4);
            contentlength = cnt - indexmessage - 4; // how much to write
        }
        // TODO if more remaining == 0, but indexmessage+4+contentlength < cnt,
        // return.
        // fprintf(stderr,"%d, %d\n", *remaining, contentlength);
        flock(file, LOCK_EX);
        // pthread_mutex_lock(mute);
        write(file, ret, contentlength);
        flock(file, LOCK_UN);
        // pthread_mutex_unlock(mute);
        if (file > 0)
            close(file);
    } else { // APPEND
        if (stat(uri, &sb) == -1) { // file not exist
            if ((file = open(uri, O_CREAT | O_WRONLY | O_TRUNC, 0644)) == -1) { // create file
                fprintf(stderr, "issue creating file\n");
            } else {
                *status = 201;
            }
        } else {
            if (S_ISREG(sb.st_mode) == false) {
                *status = 403;
                return -1;
            }
            if ((file = open(uri, O_WRONLY | O_APPEND, 0644)) == -1) {
                fprintf(stderr, "issue opening file\n");
            } else {
                *status = 200;
            }
        }
        if (errno == EACCES || errno == EISDIR) {
            *status = 403;
            return -1;
        }
        char *ret = str;
        ret += indexmessage + 4;
        if (contentlength
            > cnt - indexmessage - 4) { // if contentlength is greater than message-body
            *remaining = contentlength - (cnt - indexmessage - 4);
            contentlength = cnt - indexmessage - 4; // limit contentlength to all of messagebody
        }
        flock(file, LOCK_EX);
        // pthread_mutex_lock(mute);
        write(file, ret, contentlength);
        flock(file, LOCK_UN);
        // pthread_mutex_unlock(mute);
        if (file > 0)
            close(file);
    }
    //pthread_mutex_unlock(mutex);
    return cnt;
}

ssize_t puttofile(char *str, uint64_t cnt, char *method, char *uri, uint64_t *remaining) {
    int file;
    if (strcmp(method, "PUT") == 0 || strcmp(method, "APPEND") == 0) {
        if ((file = open(uri, O_WRONLY | O_APPEND, 0644)) == -1) {
            fprintf(stderr, "issue opening file\n");
        } else {
            char *ret = str;
            if (*remaining < cnt) {
                flock(file, LOCK_EX);
                write(file, ret, *remaining);
                flock(file, LOCK_UN);
                *remaining = 0;
            } else {
                flock(file, LOCK_EX);
                write(file, ret, cnt);
                flock(file, LOCK_UN);
                *remaining -= cnt;
            }
        }
    }
    return cnt;
}

void ok(int fd) {
    write(fd, "HTTP/1.1 200 OK\r\n", 17);
    write(fd, "Content-Length: 3\r\n\r\n", 20);
    write(fd, "OK\n", 3);
}

void created(int fd) {
    write(fd, "HTTP/1.1 201 Created\r\n", 22);
    write(fd, "Content-Length: 8\r\n\r\n", 20);
    write(fd, "Created\n", 8);
}

void bad(int fd) {
    write(fd, "HTTP/1.1 400 Bad Request\r\n", 26);
    write(fd, "Content-Length: 12\r\n\r\n", 21);
    write(fd, "Bad Request\n", 12);
}

void forbidden(int fd) {
    write(fd, "HTTP/1.1 403 Forbidden\r\n", 24);
    write(fd, "Content-Length: 10\r\n\r\n", 21);
    write(fd, "Forbidden\n", 10);
}

void notfound(int fd) {
    write(fd, "HTTP/1.1 404 Not Found\r\n", 24);
    write(fd, "Content-Length: 10\r\n\r\n", 21);
    write(fd, "Not Found\n", 10);
}

void internal(int fd) {
    write(fd, "HTTP/1.1 404 Internal Server Error\r\n", 36);
    write(fd, "Content-Length: 23\r\n\r\n", 21);
    write(fd, "Internal Server Error\n", 22);
}

void notimplemented(int fd) {
    write(fd, "HTTP/1.1 404 Not Implemented\r\n", 30);
    write(fd, "Content-Length: 16\r\n\r\n", 21);
    write(fd, "Not Implemented\n", 17);
}

bool checkregex(char *regexp, char *word) {
    regex_t regex;
    int reti;
    reti = regcomp(&regex, regexp, 0);
    if (reti) {
        fprintf(stderr, "could not compile regex\n");
        exit(1);
    }
    reti = regexec(&regex, word, 0, NULL, 0);
    if (!reti)
        return true;
    else
        return false;
    regfree(&regex);
}

void foo() {
    printf("foo\n");
}
