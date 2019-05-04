all:
	@make -C src/tools/pbimage
	@make -C src/tools/punchboot TRANSPORT=socket
	@make -C src/ BOARD=test LOGLEVEL=10 clean
	@make -C src/ BOARD=test LOGLEVEL=10
	@make -C src/ BOARD=test LOGLEVEL=10 test
clean:
	make -C src/ BOARD=test clean
