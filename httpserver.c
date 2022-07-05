#include <err.h>
#include <fcntl.h>
#include <inttypes.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <sys/stat.h>
#include <stdint.h>
#include <getopt.h>
#include <pthread.h>
#include "helper.h"
#include "queue.h"

#define OPTIONS              "t:l:"
#define BUF_SIZE             4096
#define DEFAULT_THREAD_COUNT 4
#define DEFAULT_MUTEX_COUNT  10

static FILE *logfile;
#define LOG(...) fprintf(logfile, __VA_ARGS__);

int threads_size;

pthread_mutex_t mutex;
pthread_mutex_t condmutex;
pthread_mutex_t file_mutexes[DEFAULT_MUTEX_COUNT];
pthread_t *threads;
pthread_cond_t condition_var;

Queue *queue;

bool should_quit = false;

// Converts a string to an 16 bits unsigned integer.
// Returns 0 if the string is malformed or out of the range.
static size_t strtouint16(char number[]) {
    char *last;
    long num = strtol(number, &last, 10);
    if (num <= 0 || num > UINT16_MAX || *last != '\0') {
        return 0;
    }
    return num;
}

// Creates a socket for listening for connections.
// Closes the program and prints an error message on error.
static int create_listen_socket(uint16_t port) {
    struct sockaddr_in addr;
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) {
        err(EXIT_FAILURE, "socket error");
    }
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htons(INADDR_ANY);
    addr.sin_port = htons(port);
    if (bind(listenfd, (struct sockaddr *) &addr, sizeof addr) < 0) {
        err(EXIT_FAILURE, "bind error");
    }
    if (listen(listenfd, 128) < 0) {
        err(EXIT_FAILURE, "listen error");
    }
    return listenfd;
}

void writelog(char method[], char uri[], int status, uint64_t requestid) {
    LOG("%s,/%s,%d,%" PRIu64 "\n", method, uri, status, requestid);
    fflush(logfile);
}

bool check(char buf[], int size) {
    for (int i = 0; i < size - 3; i++) {
        if (buf[i] == '\r' & buf[i + 1] == '\n' & buf[i + 2] == '\r' & buf[i + 3] == '\n') {
            return true;
        }
    }
    return false;
}

void *handle_connection(int connfd) {
    // return NULL;
    char buffer[BUF_SIZE] = "";
    char method[2048] = "";
    char uri[2048] = "";
    ssize_t bytez = 0;
    uint64_t remaining = 0;
    int status = 0;
    uint64_t requestid = 0;
    bool found_all = false;
    char found_buffer[BUF_SIZE] = "";
    int found_read = 0;
    // read all bytes from connfd until we see an error or EOF
    while ((bytez = read(connfd, buffer, BUF_SIZE)) > 0) { // read by buffersize length
        ssize_t bytez_written = 0, curr_write = 0;
        bytez_written = 0;
        if (found_all) {
            memset(found_buffer, 0, BUF_SIZE);
            found_read = 0;
        }
        memcpy(&found_buffer[found_read], buffer, bytez);
        if (found_buffer[0]) {
            found_read += bytez;
            if (check(found_buffer, found_read)) {
                found_all = true;
            }
        }
        if (found_all) {
            while (bytez_written < bytez) {
                if (remaining == 0) {
                    curr_write = readfile(connfd,
                        /*&file_mutexes[connfd],*/ found_buffer + bytez_written,
                        found_read - bytez_written, method, uri, &remaining, &status, &requestid);
                } else {
                    curr_write = puttofile(found_buffer + bytez_written, found_read - bytez_written,
                        method, uri, &remaining);
                }
                if (remaining == 0) { // print status
                    if (status == 200) {
                        ok(connfd);
                        writelog(method, uri, status, requestid);
                    } else if (status == 201) {
                        created(connfd);
                        writelog(method, uri, status, requestid);
                    } else if (status == 400) {
                        bad(connfd);
                        close(connfd);
                        return NULL;
                    } else if (status == 403) {
                        forbidden(connfd);
                        writelog(method, uri, status, requestid);
                        close(connfd);
                        return NULL;
                    } else if (status == 404) {
                        notfound(connfd);
                        writelog(method, uri, status, requestid);
                        close(connfd);
                        return NULL;
                    } else if (status == 500) {
                        internal(connfd);
                        close(connfd);
                        return NULL;
                    } else if (status == 501) {
                        notimplemented(connfd);
                        close(connfd);
                        return NULL;
                    } else if (status == 0) { //GET
                        writelog(method, uri, 200, requestid);
                    }
                }
                if (curr_write < 0) { //nothing
                    return NULL;
                }
                bytez_written += curr_write; // curr_write;
            }
            //mutex lock and cond
        }
        //fprintf(stderr, "here\n");
        memset(buffer, 0, BUF_SIZE);
        //return NULL;
    }
    //fprintf(stderr, "ending\n");
    //return NULL;
    memset(method, 0, 2048);
    memset(uri, 0, 2048);
    (void) connfd;
    return NULL;
}

void set_quit(bool quit) {
    pthread_mutex_lock(&condmutex);
    should_quit = quit;
    pthread_mutex_unlock(&condmutex);
}

bool get_should_quit() {
    pthread_mutex_lock(&condmutex);
    bool b = should_quit;
    pthread_mutex_unlock(&condmutex);
    return b;
}

// static void join_threads() {
//     set_quit(true);
//     pthread_cond_broadcast(&condition_var);
//     for (int i = 0; i < threads_size; i++) {
//         if (pthread_join(threads[i], NULL) != 0) {
//             errx(EXIT_FAILURE, "Thread Error: Unable to join thread %d\n", i);
//         }
//     }
// }

static void sig_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        //join_threads();
        free(threads);
        queue_delete(&queue);
        warnx("received SIGINT or SIGTERM");
        fclose(logfile);
        exit(EXIT_SUCCESS);
    }
}

static void usage(char *exec) {
    fprintf(stderr, "usage: %s [-t threads] [-l logfile] <port>\n", exec);
    fflush(logfile);
}

static void *thread_function(void *arg) {
    // int connfd;
    // while (true) {
    //     connfd = -1;
    //     pthread_mutex_lock(&mutex);
    //     if (!dequeue(queue, &connfd)) {
    //         pthread_cond_wait(&condition_var, &mutex);
    //         dequeue(queue, &connfd); //try again
    //     }
    //     pthread_mutex_unlock(&mutex);
    //     if (connfd >= 0) {
    //         handle_connection(connfd);
    //         close(connfd);
    //     }
    //     if (should_quit) {
    //         return NULL;
    //     }
    // }
    int connfd;
    while (1) {
        pthread_mutex_lock(&mutex);
        while (queue_size(queue) <= 0 && !get_should_quit()) {
            pthread_cond_wait(&condition_var, &mutex);
        }
        pthread_mutex_unlock(&mutex);
        if (queue_size(queue) > 0) {
            connfd = dequeue(queue);
            handle_connection(connfd);
            close(connfd);
        }
        if (get_should_quit()) {
            break;
        }
    }
    // return NULL;
    // while (true) {
    //     pthread_mutex_lock(&mutex);
    //     printf("%lu HAS LOCK\n", pthread_self());
    //     while (queue_size(queue) == 0) {
    //         pthread_cond_wait(&condition_var, &mutex);
    //     }
    //     int connfd = dequeue(queue);
    //     pthread_mutex_unlock(&mutex);
    //     printf("%lu RELEASED LOCK\n", pthread_self());
    //     handle_connection(connfd);
    //     close(connfd);
    // }
    return arg;
}

static void create_threads() {
    threads = calloc(threads_size, sizeof(pthread_t));
    for (int i = 0; i < threads_size; i++) {
        if (pthread_create(&threads[i], NULL, &thread_function, NULL) != 0) {
            errx(EXIT_FAILURE, "Thread Error: Unable to create thread %d\n", i);
        }
    }
}

int main(int argc, char *argv[]) {
    int opt = 0;
    threads_size = DEFAULT_THREAD_COUNT;
    logfile = stderr;

    while ((opt = getopt(argc, argv, OPTIONS)) != -1) {
        switch (opt) {
        case 't':
            threads_size = strtol(optarg, NULL, 10);
            if (threads_size <= 0) {
                errx(EXIT_FAILURE, "bad number of threads");
            }
            break;
        case 'l':
            logfile = fopen(optarg, "w");
            if (!logfile) {
                errx(EXIT_FAILURE, "bad logfile");
            }
            break;
        default: usage(argv[0]); return EXIT_FAILURE;
        }
    }

    if (optind >= argc) {
        warnx("wrong number of arguments");
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    uint16_t port = strtouint16(argv[optind]);
    if (port == 0) {
        errx(EXIT_FAILURE, "bad port number: %s", argv[1]);
    }

    signal(SIGPIPE, SIG_IGN);
    signal(SIGTERM, sig_handler);
    signal(SIGINT, sig_handler);

    pthread_mutex_init(&mutex, NULL);
    pthread_mutex_init(&condmutex, NULL);
    pthread_cond_init(&condition_var, NULL);
    for (int i = 0; i < DEFAULT_MUTEX_COUNT; i++) {
        pthread_mutex_init(&file_mutexes[i], NULL);
    }

    // //create queue
    queue = queue_create();

    //create threads
    create_threads();

    int listenfd = create_listen_socket(port);
    // LOG("port=%" PRIu16 ", threads=%d\n", port, threads);

    for (;;) {
        int connfd = accept(listenfd, NULL, NULL);
        if (connfd < 0) {
            warn("accept error");
            continue;
        }
        pthread_mutex_lock(&mutex);
        pthread_cond_signal(&condition_var);
        enqueue(queue, connfd);
        pthread_mutex_unlock(&mutex);
    }
    fclose(logfile);
    return EXIT_SUCCESS;
}
