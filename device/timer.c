# include "io.h"
# include "kernel/print.h"
# include "interrupt.h"
# include "thread.h"
# include "debug.h"

# define IRQ0_FREQUENCY 1000
# define INPUT_FREQUENCY 1193180
# define COUNTER0_VALUE INPUT_FREQUENCY / IRQ0_FREQUENCY
# define COUNTER0_PORT 0x40
# define COUNTER_MODE 2
# define COUNTER0_NO 0
# define READ_WRITE_LATCH 3
# define PIT_CONTROL_PORT 0x43

/**
 * 内核自开启中断后所有的嘀嗒数.
 */
uint32_t ticks;

static void frequency_set(uint8_t counter_port,
                          uint8_t counter_no,
                          uint8_t rwl,
                          uint8_t counter_mode,
                          uint16_t counter_value) {
    outb(PIT_CONTROL_PORT, (uint8_t) (counter_no << 6 | rwl << 4 | counter_mode << 1));
    outb(counter_port, (uint8_t) counter_value);
    outb(counter_port, (uint8_t) counter_value >> 8);
}

static void intr_timer_handler(void) {
    struct task_struct* cur_thread = running_thread();

    ASSERT(cur_thread->stack_magic == 0x77777777);

    cur_thread->elaspsed_ticks++;
    ticks++;

    if (cur_thread->ticks == 0) {
        schedule();
    } else {
        cur_thread->ticks--;
    }
}

/**
 * 初始化PIT 8253.
 */ 
void timer_init() {
    put_str("timer_init start.\n");
    frequency_set(COUNTER0_PORT, COUNTER0_NO, READ_WRITE_LATCH, COUNTER_MODE, COUNTER0_VALUE);
    register_handler(0x20, intr_timer_handler);
    put_str("timer_init done.\n");
}