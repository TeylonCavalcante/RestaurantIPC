/*
 * server.c  -- "Cozinha" que recebe pedidos via fila POSIX e registra em log.
 *
 * Compilar: gcc server.c -o server -lrt -pthread
 *
 * Executar: ./server
 * Ctrl+C para encerrar (server faz cleanup)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <mqueue.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <semaphore.h>
#include <pthread.h>
#include <errno.h>
#include "common.h"

static volatile int running = 1;
static mqd_t mq = (mqd_t)-1;
static int *shm_total = NULL;
static int shm_fd = -1;
static sem_t *sem = NULL;

/* logger queue - simple singly linked list of messages */
typedef struct MsgNode {
    char *text;
    struct MsgNode *next;
} MsgNode;

static MsgNode *head = NULL;
static MsgNode *tail = NULL;
static pthread_mutex_t q_mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t q_cond = PTHREAD_COND_INITIALIZER;
static pthread_t logger_tid;

/* signal handler for graceful shutdown */
void handle_sigint(int s) {
    (void)s;
    running = 0;
}

/* enqueue message to logger queue (caller holds no lock) */
void enqueue_log(const char *line) {
    MsgNode *n = malloc(sizeof(MsgNode));
    if (!n) return;
    n->text = strdup(line);
    n->next = NULL;

    pthread_mutex_lock(&q_mtx);
    if (tail) tail->next = n;
    else head = n;
    tail = n;
    pthread_cond_signal(&q_cond);
    pthread_mutex_unlock(&q_mtx);
}

/* logger thread: writes queued messages to LOGFILE */
void *logger_thread(void *arg) {
    FILE *f = fopen(LOGFILE, "a");
    if (!f) {
        perror("fopen log");
        return NULL;
    }

    while (running || head != NULL) {
        pthread_mutex_lock(&q_mtx);
        while (head == NULL && running) {
            pthread_cond_wait(&q_cond, &q_mtx);
        }
        /* pop all nodes */
        while (head) {
            MsgNode *n = head;
            head = head->next;
            if (head == NULL) tail = NULL;
            pthread_mutex_unlock(&q_mtx);

            /* write to file */
            fprintf(f, "%s\n", n->text);
            fflush(f);
            free(n->text);
            free(n);

            pthread_mutex_lock(&q_mtx);
        }
        pthread_mutex_unlock(&q_mtx);
    }

    fclose(f);
    return NULL;
}

void cleanup_all(void) {
    if (mq != (mqd_t)-1) {
        mq_close(mq);
        mq_unlink(MQ_NAME);
    }
    if (sem) {
        sem_close(sem);
        sem_unlink(SEM_NAME);
    }
    if (shm_total) {
        munmap(shm_total, SHM_SIZE);
    }
    if (shm_fd != -1) close(shm_fd);
    shm_unlink(SHM_NAME);
}

int main(void) {
    signal(SIGINT, handle_sigint);

    /* 1) Setup shared memory (total orders) */
    shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) { perror("shm_open"); exit(1); }
    if (ftruncate(shm_fd, SHM_SIZE) == -1) { perror("ftruncate"); exit(1); }
    shm_total = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm_total == MAP_FAILED) { perror("mmap"); exit(1); }
    *shm_total = 0; // inicia contador

    /* 2) Setup named semaphore (capacidade de clientes simultâneos que podem 'entrar' para enviar pedido) */
    sem_unlink(SEM_NAME); // garante clean
    sem = sem_open(SEM_NAME, O_CREAT | O_EXCL, 0666, 3); // capacidade 3
    if (sem == SEM_FAILED) { perror("sem_open"); /* continue sem sem */ sem = NULL; }

    /* 3) Setup message queue (server recebe) */
    mq_unlink(MQ_NAME); // garante clean
    struct mq_attr attr;
    attr.mq_flags = 0;
    attr.mq_maxmsg = MQ_MAXMSG;
    attr.mq_msgsize = MQ_MSGSIZE;
    attr.mq_curmsgs = 0;

    mq = mq_open(MQ_NAME, O_CREAT | O_RDONLY, 0666, &attr);
    if (mq == (mqd_t)-1) { perror("mq_open server"); cleanup_all(); exit(1); }

    /* 4) Start logger thread */
    pthread_create(&logger_tid, NULL, logger_thread, NULL);

    printf("[COZINHA] Pronta. Aguardando pedidos (mq=%s). Ctrl+C para sair.\n", MQ_NAME);

    /* 5) Receive loop */
    char buf[MQ_MSGSIZE+1];
    while (running) {
        ssize_t n = mq_receive(mq, buf, MQ_MSGSIZE, NULL);
        if (n >= 0) {
            buf[n] = '\0';
            /* expected message format: "mesa:pedido_text" e.g. "2:Lasanha" */
            int mesa = -1;
            char pedido[MQ_MSGSIZE];
            pedido[0] = '\0';
            sscanf(buf, "%d:%255[^\n]", &mesa, pedido);

            /* update shared total */
            __sync_fetch_and_add(shm_total, 1); // atomic increment for safety

            /* build log line and print */
            char line[512];
            snprintf(line, sizeof(line), "[COZINHA] Pedido recebido: Mesa %d -> %s | TotalPedidos=%d",
                     mesa, pedido, *shm_total);

            printf("%s\n", line);
            /* enqueue to logger */
            enqueue_log(line);
        } else {
            if (errno == EINTR) continue;
            perror("mq_receive");
            break;
        }
    }

    /* shutdown */
    printf("[COZINHA] Encerrando... aguardando logger finalizar\n");
    pthread_mutex_lock(&q_mtx);
    pthread_cond_signal(&q_cond);
    pthread_mutex_unlock(&q_mtx);
    pthread_join(logger_tid, NULL);

    cleanup_all();
    printf("[COZINHA] Finalizado.\n");
    return 0;
}
