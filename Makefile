all:
	make -C src/ BOARD=test LOGLEVEL=10
	dd if=/dev/zero of=/tmp/disk bs=1M count=32
	qemu-system-arm -machine virt -cpu cortex-a15 -m 1024 -nographic -semihosting -device virtio-serial-device  -chardev socket,path=/tmp/foo,server,nowait,id=foo -device virtserialport,chardev=foo -device virtio-blk-device,drive=disk -drive id=disk,file=/tmp/disk,if=none,format=raw  -kernel src/pb
clean:
	make -C src/ BOARD=test clean
