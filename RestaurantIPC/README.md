# RestaurantIPC — Trabalho (Restaurante IPC)

## Objetivo
Projeto simples que demonstra mecanismos de concorrência e IPC:
- Filas de mensagens POSIX (mq_send/mq_receive)
- Processos (cliente e server são binários separados)
- Threads (logger thread no server)
- Mutex/cond (para fila interna do logger)
- Semáforo POSIX nomeado (limita clientes simultâneos)
- Memória compartilhada POSIX (shm_open + mmap) para contador total de pedidos
- Sinais (SIGINT para shutdown)

## Arquivos
- `src/server.c` — servidor (cozinha)
- `src/client.c` — cliente (mesa)
- `src/common.h` — nomes de IPC
- `Makefile`
- `pedidos.log` — gerado pelo servidor (após receber pedidos)

## Requisitos
- Linux / WSL (Ubuntu)
- gcc, librt, pthreads
- compilar com: `-lrt -pthread`

## Compilar
```bash
make

