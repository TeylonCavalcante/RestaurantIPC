/*
 * client.c  -- cliente que envia pedido (mesa) via fila POSIX
 *
 * Compilar: gcc client.c -o client -lrt -pthread
 *
 * Uso:
 *  ./client
 *  será solicitado: número da mesa e texto do pedido
 *
 * O cliente:
 *  - abre a fila em modo escrita
 *  - abre o semáforo nomeado e faz sem_wait() antes de enviar (respeita limite)
 *  - opcionalmente lê o total atual de pedidos da shm (somente leitura)
 *  - envia mensagem e faz sem_post()
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mqueue.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include "common.h"

int main(void) {
    int mesa = 0;
    char pedido[200];

    printf("Número da mesa: ");
    if (scanf("%d%*c", &mesa) != 1) { printf("Entrada inválida\n"); return 1; }

    printf("Digite o pedido: ");
    if (!fgets(pedido, sizeof(pedido), stdin)) return 1;
    // remove newline
    size_t L = strlen(pedido);
    if (L > 0 && pedido[L-1] == '\n') pedido[L-1] = '\0';

    /* open semaphore */
    sem_t *sem = sem_open(SEM_NAME, 0);
    if (sem == SEM_FAILED) {
        perror("sem_open (client) - continuing without sem");
        sem = NULL;
    } else {
        printf("[MESA %d] Aguardando vaga para enviar pedido...\n", mesa);
        sem_wait(sem);
    }

    /* open message queue for writing */
    mqd_t mq = mq_open(MQ_NAME, O_WRONLY);
    if (mq == (mqd_t)-1) {
        perror("mq_open client");
        if (sem) sem_post(sem);
        return 1;
    }

    /* optional: read current total from shm (read-only) */
    int shm_fd = shm_open(SHM_NAME, O_RDONLY, 0);
    if (shm_fd != -1) {
        int *total = mmap(NULL, SHM_SIZE, PROT_READ, MAP_SHARED, shm_fd, 0);
        if (total != MAP_FAILED) {
            printf("[MESA %d] Total atual de pedidos (antes): %d\n", mesa, *total);
            munmap(total, SHM_SIZE);
        }
        close(shm_fd);
    }

    /* send message format "mesa:pedido" */
    char buf[MQ_MSGSIZE];
    snprintf(buf, sizeof(buf), "%d:%s", mesa, pedido);

    if (mq_send(mq, buf, strlen(buf), 0) == -1) {
        perror("mq_send");
        mq_close(mq);
        if (sem) sem_post(sem);
        return 1;
    }

    printf("[MESA %d] Pedido enviado: %s\n", mesa, pedido);

    mq_close(mq);
    if (sem) sem_post(sem);
    return 0;
}
