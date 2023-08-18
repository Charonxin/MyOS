# ifndef _DEVICE_IOQUEUE_H
# define _DEVICE_IOQUEUE_H

# include "stdint.h"
# include "thread.h"
# include "sync.h"

# define buf_size 64

struct ioqueue {    
    struct lock* lock;
    struct task_struct* producer;
    struct task_struct* consumer;
    char buf[buf_size];
    int32_t head;
    int32_t tail;
};

int is_queue_full(struct ioqueue* queue);
void ioqueue_init(struct ioqueue* queue);
int is_queue_empty(struct ioqueue* queue);
char queue_getchar(struct ioqueue* queue);
char queue_putchar(struct ioqueue* queue, char byte);

# endif
