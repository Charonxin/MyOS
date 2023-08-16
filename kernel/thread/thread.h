# ifndef _THREAD_H
# define _THREAD_H

# include "stdint.h"
# include "kernel/list.h"
#include "bitmap.h"
#include "memory.h"

/**
 * 自定义通用函数类型.
 */ 
typedef void thread_func(void*);

/**
 * 线程状态.
 */ 
enum task_status {
    TASK_RUNNING,
    TASK_READY,
    TASK_BLOCKED,
    TASK_WAITTING,
    TASK_HANGING,
    TASK_DIED
};

/**
 * 中断栈.
 */
struct intr_stack {
    uint32_t vec_no;
    uint32_t edi;  
    uint32_t esi;  
    uint32_t ebp;  
    uint32_t esp_dummy;  
    uint32_t ebx;  
    uint32_t edx;  
    uint32_t ecx;  
    uint32_t eax;  
    uint32_t gs;  
    uint32_t fs;  
    uint32_t es;  
    uint32_t ds;

    // 下面的属性由CPU从低特权级进入高特权级时压入
    uint32_t err_code;  
    void (*eip) (void);
    uint32_t cs;
    uint32_t eflags;
    void* esp;
    uint32_t ss;
};

struct thread_stack {
    uint32_t ebp;
    uint32_t ebx;
    uint32_t edi;
    uint32_t esi;

    // 第一次执行时指向待调用的函数kernel_thread，其它时候指向switch_to的返回地址.
    void (*eip) (thread_func* func, void* func_args);

    void (*unused_retaddr);
    thread_func* function;
    void* func_args;
};

/**
 * PCB，进程或线程的控制块.
 */ 
struct task_struct {
   uint32_t* self_kstack;	 // 各内核线程都用自己的内核栈
   pid_t pid;
   enum task_status status;
   char name[16];
   uint8_t priority;
   uint8_t ticks;	   // 每次在处理器上执行的时间嘀嗒数

/* 此任务自上cpu运行后至今占用了多少cpu嘀嗒数,
 * 也就是此任务执行了多久*/
   uint32_t elapsed_ticks;

/* general_tag的作用是用于线程在一般的队列中的结点 */
   struct list_elem general_tag;				    

/* all_list_tag的作用是用于线程队列thread_all_list中的结点 */
   struct list_elem all_list_tag;

   uint32_t* pgdir;              // 进程自己页表的虚拟地址

   struct virtual_addr userprog_vaddr;   // 用户进程的虚拟地址
   struct mem_block_desc u_block_desc[DESC_CNT];   // 用户进程内存块描述符
   uint32_t stack_magic;	 // 用这串数字做栈的边界标记,用于检测栈的溢出
};

struct task_struct* running_thread();
void thread_create(struct task_struct* pthread, thread_func function, void* func_args);
void init_thread(struct task_struct* pthread, char* name, int prio);
struct task_struct* thread_start(char* name, int prio, thread_func function, void* func_args);
void schedule();
void thread_init();
void thread_block(enum task_status status);
void thread_unblock(struct task_struct* pthread);

# endif