# include "kernel/print.h"
# include "init.h"
# include "thread.h"

void k_thread_function(void*);

int main(void) {
    put_str("I am kernel.\n");
    init_all();

    thread_start("k_thread_1", 31, k_thread_a, "argA ");
    thread_start("k_thread_2", 31, k_thread_b, "argB ");

    intr_enable();
    while(1) {
        put_str("Main ");
    }

    return 0;
}

void k_thread_a(void* args) {
    char* para = args;
    while (1) {
        put_str(para);
    }
}

void k_thread_b(void* args) {
    char* para = args;
    while (1) {
        put_str(para);
    }
}

void k_thread_function(void* args) {
    // 这里必须是死循环，否则执行流并不会返回到main函数，所以CPU将会放飞自我，触发6号未知操作码异常
    while (1) {
        put_str((char*) args);
    }
}