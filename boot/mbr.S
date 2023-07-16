%include "boot.inc"
SECTION MBR vstart=0x7c00 ;SECTION 伪指令
    mov ax,cs
    mov ds,ax
    mov es,ax
    mov ss,ax
    mov fs,ax
    mov sp,0x7c00
    mov ax,0xb800
    mov gs,ax

    ; 清屏利用0x06号功能，上卷全部行，则可清屏
    ; -----------------------------------------------------------
    ;INT 0x10   功能号:0x06       功能描述:上卷窗口
    ;------------------------------------------------------
    ;输入
    ;AH 功能号= 0x06
    ;AL = 上卷行数(如果为零,表示全部)
    ;BH = 上卷行属性
    ;(CL,CH) =窗口左上角的(X,Y)位置
    ;(DL,DH) = 窗口右下角(X,Y)位置
    ;无返回值
    mov     ax, 0x600
    mov     bx, 0x700
    mov     cx, 0        ; 左上角: (0, 0)
    mov     dx, 0x184f  ; 右下角: (80,25),
                        ; 80个字符 25行
                        ; 从下标0开始,所以0x18=24,0x4f=79
    int     0x10         ; int 0x10


    ;;;;;;;;;   输出背景色为绿色，前景色为红色，并且跳动的字符串字符 “1 MBR”    ;;;;;;;;;
    mov byte [gs:0x00], '1'
    mov byte [gs:0x01], 0xa4

    mov byte [gs:0x02], ' '
    mov byte [gs:0x04], 0xa4

    mov byte [gs:0x04], 'M'
    mov byte [gs:0x05], 0xa4

    mov byte [gs:0x06], 'B'
    mov byte [gs:0x07], 0xa4

    mov byte [gs:0x08], 'R'
    mov byte [gs:0x09], 0xa4

    mov eax,LOADER_START_SECTOR ; 起始扇区 lba 地址 0x2 定义了loader在硬盘上的逻辑扇区地址 LBA
    mov bx,LOADER_BASE_ADDR ; 写入的地址 0x900 定义了loader在内存中的位置，MBR要把loader从硬盘读入后放到此处
    mov cx,1 ; 待读入的扇区数
    call rd_disk_m_16 ; 以下读取程序的起始部分(一个扇区)

    jmp LOADER_BASE_ADDR ;mbr程序结束，程序跳到laoder

    ;-------------------------------------------------------------------------------
    ;功能:读取硬盘 n 个扇区
    rd_disk_m_16:
    ;-------------------------------------------------------------------------------
    ; eax=LBA 扇区号
    ; bx=将数据写入的内存地址
    ; cx=读入的扇区数
    mov esi,eax ;备份 eax
    mov di,cx ;备份 cx
    ;读写硬盘:
    ;第 1 步:设置要读取的扇区数
    mov dx,0x1f2
    mov al,cl
    out dx,al ;写入的扇区数量 serctor count 

    mov eax,esi ;恢复 ax

    ;第 2 步:将 LBA 地址存入 0x1f3 ～ 0x1f6 

    ;LBA 地址 7～ 0 位写入端口 0x1f3
    mov dx,0x1f3;LBAlow
    out dx,al

    ;LBA 地址 15～ 8 位写入端口 0x1f4
    mov cl,8
    shr eax,cl
    mov dx,0x1f4
    out dx,al

    ;LBA 地址 23～ 16 位写入端口 0x1f5
    shr eax,cl
    mov dx,0x1f5
    out dx,al

    shr eax,cl
    and al,0x0f ;lba 第 24～ 27 位
    or al,0xe0 ; 设置 7～ 4 位为 1110,表示 lba 模式
    mov dx,0x1f6
    out dx,al

    ;第 3 步:向 0x1f7 端口写入读命令,0x20;
    mov dx,0x1f7
    mov al,0x20
    out dx,al

    ;第 4 步:检测硬盘状态
    .not_ready:
        ;同一端口,写时表示写入命令字,读时表示读入硬盘状态
        nop
        in al,dx
        and al,0x88 ;第 4 位为 1 表示硬盘控制器已准备好数据传输
        ;第 7 位为 1 表示硬盘忙
        cmp al,0x08
        jnz .not_ready ;若未准备好,继续等

        ;第 5 步:从 0x1f0 端口读数据
        mov ax, di
        mov dx, 256
        mul dx
        mov cx, ax
        ; di 为要读取的扇区数,一个扇区有 512 字节,每次读入一个字
        ; 共需 di*512/2 次,所以 di*256
        mov dx, 0x1f0
    .go_on_read:
        in ax,dx
        mov [bx],ax ;将数据写入的内存地址
        add bx,2
        loop .go_on_read
        ret

    times 510-($-$$) db 0
    db 0x55,0xaa



























































































































































































































































