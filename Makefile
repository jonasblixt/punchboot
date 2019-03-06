all:
	@make -C src/ BOARD=test LOGLEVEL=10
	@make -C src/ BOARD=test LOGLEVEL=10 test
clean:
	make -C src/ BOARD=test clean
