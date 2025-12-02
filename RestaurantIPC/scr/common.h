#ifndef COMMON_H
#define COMMON_H

#define _POSIX_C_SOURCE 200809L

// POSIX names ( devem começar com '/' )
#define MQ_NAME    "/restaurant_orders_mq"
#define SHM_NAME   "/restaurant_total_shm"
#define SEM_NAME   "/restaurant_sem"

// sizes
#define MQ_MAXMSG   10
#define MQ_MSGSIZE  256
#define SHM_SIZE    sizeof(int)   // armazenaremos um inteiro: total de pedidos

#define LOGFILE "pedidos.log"

#endif // COMMON_H
