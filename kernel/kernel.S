[bits 32]

; 对于CPU会自动压入错误码的中断类型，无需额外的操作
%define ERROR_CODE nop
; 如果CPU没有压入错误码，为了保持处理逻辑的一致性，我们需要手动压入一个0
%define ZERO push 0

extern put_str
; 中断处理函数数组
extern idt_table

section .data
intr_str db "interrupt occur!", 0xa, 0
global intr_entry_table
intr_entry_table:

; 中断处理程序宏定义
%macro VECTOR 2
section .text
intr%1entry:
    
    %2
    ; 保存上下文
    push ds
    push es
    push fs
    push gs
    pushad

    mov al, 0x20
    out 0xa0, al
    out 0x20, al

    push %1

    ; 调用C的中断处理函数
    call [idt_table + 4 * %1]
    jmp intr_exit

section .data
    dd intr%1entry

%endmacro

section .text
global intr_exit
intr_exit:
    add esp, 4
    popad
    pop gs
    pop fs
    pop es
    pop ds
    add esp, 4
    iretd

VECTOR 0x00, ZERO
VECTOR 0x01, ZERO
VECTOR 0x02, ZERO
VECTOR 0x03, ZERO
VECTOR 0x04, ZERO
VECTOR 0x05, ZERO
VECTOR 0x06, ZERO
VECTOR 0x07, ZERO
VECTOR 0x08, ZERO
VECTOR 0x09, ZERO
VECTOR 0x0a, ZERO
VECTOR 0x0b, ZERO
VECTOR 0x0c, ZERO
VECTOR 0x0d, ZERO
VECTOR 0x0e, ZERO
VECTOR 0x0f, ZERO
VECTOR 0x10, ZERO
VECTOR 0x11, ZERO
VECTOR 0x12, ZERO
VECTOR 0x13, ZERO
VECTOR 0x14, ZERO
VECTOR 0x15, ZERO
VECTOR 0x16, ZERO
VECTOR 0x17, ZERO
VECTOR 0x18, ZERO
VECTOR 0x19, ZERO
VECTOR 0x1a, ZERO
VECTOR 0x1b, ZERO
VECTOR 0x1c, ZERO
VECTOR 0x1d, ZERO
VECTOR 0x1e, ERROR_CODE
VECTOR 0x1f, ZERO
VECTOR 0x20, ZERO


;;;;;;;;;;;;;;;;   0x80号中断   ;;;;;;;;;;;;;;;;
[bits 32]
extern syscall_table
section .text
global syscall_handler
syscall_handler:
;1 保存上下文环境
   push 0			    ; 压入0, 使栈中格式统一

   push ds
   push es
   push fs
   push gs
   pushad			    ; PUSHAD指令压入32位寄存器，其入栈顺序是:
				    ; EAX,ECX,EDX,EBX,ESP,EBP,ESI,EDI 
				 
   push 0x80			    ; 此位置压入0x80也是为了保持统一的栈格式

;2 为系统调用子功能传入参数
   push edx			    ; 系统调用中第3个参数
   push ecx			    ; 系统调用中第2个参数
   push ebx			    ; 系统调用中第1个参数

;3 调用子功能处理函数
   call [syscall_table + eax*4]	    ; 编译器会在栈中根据C函数声明匹配正确数量的参数
   add esp, 12			    ; 跨过上面的三个参数

;4 将call调用后的返回值存入待当前内核栈中eax的位置
   mov [esp + 8*4], eax	
   jmp intr_exit		    ; intr_exit返回,恢复上下文