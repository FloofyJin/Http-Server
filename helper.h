#ifndef __HELPER_H__
#define __HELPER_H__

#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>

ssize_t readfile(int fd, /*pthread_mutex_t mutex,*/ char *str, uint64_t cnt, char method[],
    char uri[], uint64_t *remaining, int *status, uint64_t *requestid);
ssize_t puttofile(char *str, uint64_t cnt, char method[], char uri[], uint64_t *remaining);
void ok(int fd);
void created(int fd);
void bad(int fd);
void forbidden(int fd);
void notfound(int fd);
void internal(int fd);
void notimplemented(int fd);

bool checkregex(char *regexp, char *word);

void foo();

#endif
