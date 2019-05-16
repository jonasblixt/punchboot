COV_BUILD ?= $(shell which cov-build)
CURL ?= $(shell which curl)
COVERITY_SECRET ?= $(shell cat ~/.coverity_secret)

all:
	@echo Top level makefile is used to run tests and coverity. \
			Use src/Makefile for normal builds
tests:
	@make -C src/tools/punchboot clean
	@make -C src/tools/pbimage clean
	@make -C src/ BOARD=test LOGLEVEL=10 clean
	@make -C src/ BOARD=test LOGLEVEL=10
	@make -C src/ BOARD=test LOGLEVEL=10 test
coverity:
	@rm -rf cov-int && \
		cov-configure --config cov.xml \
	                  --compiler arm-eabi-gcc --comptype gcc \
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

clean:
	@make -C src/ BOARD=test clean
	@make -C src/tools/punchboot clean
	@make -C src/tools/pbimage clean
