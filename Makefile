COV_BUILD ?= $(shell which cov-build)
CURL ?= $(shell which curl)
COVERITY_SECRET ?= $(shell cat ~/.coverity_secret)
DOCKER ?= docker
IMX_FIRMWARE_VER ?= 8.1
IMX_FIRMWARE_URL ?= http://www.freescale.com/lgfiles/NMG/MAD/YOCTO/firmware-imx-$(IMX_FIRMWARE_VER).bin

all:
	@echo Top level makefile is used to run tests and coverity. \
			Use src/Makefile for normal builds
tests:
	@make -C src/tools/punchboot/src clean
	@make -C src/tools/pbimage/src clean
	@make -C src/tools/pbconfig/src clean
	@make -C src/ BOARD=test LOGLEVEL=10 clean
	@make -C src/ BOARD=test LOGLEVEL=10
	@make -C src/ BOARD=test LOGLEVEL=10 test

all-boards:
	@rm -rf src/build-*
	@make -C src/ BOARD=test LOGLEVEL=3
	@make -C src/ BOARD=jiffy LOGLEVEL=3
	@make -C src/ BOARD=pico8ml LOGLEVEL=3
	@make -C src/ BOARD=imx8qxmek LOGLEVEL=3
	@make -C src/ BOARD=imx8mevk LOGLEVEL=3

coverity:
	@rm -rf cov-int && \
		cov-configure --config cov.xml \
	                  --compiler arm-none-eabi-gcc --comptype gcc \
					  --template \
					  --xml-option=skip_file:".*/src/tests/.*" \
					  --xml-option=skip_file:".*/src/3pp/.*" && \
		$(COV_BUILD) --config cov.xml --dir cov-int make tests && \
		tar -czf coverity.tar.gz cov-int && \
		rm -rf cov-int && \
		$(CURL) --form token=$(COVERITY_SECRET) \
				--form email=jonpe960@gmail.com \
				--form file=@coverity.tar.gz \
				--form version="Version" \
				--form description="Description" \
				https://scan.coverity.com/builds\?project\=jonpe960%2Fpunchboot && \
		rm -rf coverity.tar.gz
docker:
	@rm -rf blobs/*
	@mkdir -p blobs
	@wget -q $(IMX_FIRMWARE_URL)
	@mv firmware-imx-$(IMX_FIRMWARE_VER).bin blobs/
	@cd blobs && chmod +x ./firmware-imx-$(IMX_FIRMWARE_VER).bin && \
			./firmware-imx-$(IMX_FIRMWARE_VER).bin && cd ..
	@$(DOCKER) build -f pb.Dockerfile -t pb_docker_env .
dockerenv:
	@$(DOCKER) run -it -u $(shell id -u ${USER}) -v $(shell realpath .):/pb/ pb_docker_env

clean:
	@make -C src/ BOARD=test clean
	@make -C src/tools/punchboot/src clean
	@make -C src/tools/pbimage/src clean
	@make -C src/tools/pbconfig/src clean
