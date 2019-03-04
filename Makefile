all:
	@make -C src/ BOARD=test LOGLEVEL=10
	@make -C src/ BOARD=test LOGLEVEL=10 test
	@cat src/test_log.txt
clean:
	make -C src/ BOARD=test clean
