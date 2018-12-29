all:
	@make -C src/ BOARD=jiffy clean
	@make -C src/ BOARD=bebop clean
	@make -C src/ BOARD=test clean
	@make -C src/ BOARD=jiffy
	@make -C src/ BOARD=jiffy clean
	@make -C src/ BOARD=bebop
	@make -C src/ BOARD=bebop clean
	@make -C src/ BOARD=test LOGLEVEL=10
	@make -C src/ BOARD=test LOGLEVEL=10 test
clean:
	make -C src/ BOARD=test clean
