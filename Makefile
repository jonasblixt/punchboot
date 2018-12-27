all:
	@make -C src/ BOARD=jiffy
	@make -C src/ BOARD=jiffy clean
	@make -C src/ BOARD=bebop
	@make -C src/ BOARD=bebop clean
	@make -C src/ BOARD=test LOGLEVEL=10
	@dd if=/dev/zero of=/tmp/disk bs=1M count=32
	@make -C src/ BOARD=test LOGLEVEL=10 test
clean:
	make -C src/ BOARD=test clean
