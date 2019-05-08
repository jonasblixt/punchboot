all:
	@make -C src/tools/punchboot clean
	@make -C src/tools/pbimage clean
	@make -C src/ BOARD=test LOGLEVEL=10 clean
	@make -C src/ BOARD=test LOGLEVEL=10
	@make -C src/ BOARD=test LOGLEVEL=10 test
clean:
	make -C src/ BOARD=test clean
