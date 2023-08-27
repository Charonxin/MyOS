#include "init.h"
#include "console.h"
#include "kernel/print.h"
#include "memory.h"
#include "thread.h"
#include "timer.h"

void
init_all () {
  put_str ("init_all.\n");
  idt_init ();
  mem_init ();
  thread_init ();
  timer_init ();
  console_init ();
}
