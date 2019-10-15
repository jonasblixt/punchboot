CROSS_COMPILE ?= arm-linux-gnueabihf-

all:
	@make -C pbimage/
	@make -C punchboot/
	@make -C imxcst/src
clean:
	@make -C pbimage/ clean
	@make -C punchboot/ clean
	@make -C imxcst/src clean
