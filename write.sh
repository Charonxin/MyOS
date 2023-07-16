nasm -I boot/include/ boot/loader.S -o boot/loader.bin
dd if=./boot/loader.bin of=/home/charon/bochs/hd60M.img bs=512 seek=2 count=4  conv=notrunc

nasm -I boot/include/ -o boot/mbr.bin boot/mbr.S
dd if=./boot/mbr.bin of=/home/charon/bochs/hd60M.img bs=512 count=1  conv=notrunc

gcc kernel/main.c -c -o kernel/main.o 
ld kernel/main.o -Ttext 0xc0001500 -e main -o kernel/kernel.bin 
dd if=kernel/kernel.bin of=/home/charon/bochs/hd60M.img bs=512 count=200 seek=9 conv=notrunc
