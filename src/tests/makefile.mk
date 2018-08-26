TESTS  = test_boot

QEMU = qemu-system-arm
QEMU_AUDIO_DRV = "none"
QEMU_FLAGS  = -machine virt -cpu cortex-a15 -m 1024 
QEMU_FLAGS += -nographic -semihosting

TEST_C_SRCS  = tests/gcov.c 
TEST_C_SRCS += tests/common.c
TEST_C_SRCS += tinyprintf.c

TEST_OBJS      = $(TEST_C_SRCS:.c=.o) 

CFLAGS += -fprofile-arcs -ftest-coverage
LDFLAGS += 

BOARD = test

test: $(ARCH_OBJS) $(PLAT_OBJS) $(BOARD_OBJS) $(TEST_OBJS) 
	@$(foreach TEST,$(TESTS), \
		$(CC) $(CFLAGS) -c tests/$(TEST).c && \
		$(LD) $(LDFLAGS) $(ARCH_OBJS) $(BOARD_OBJS) $(PLAT_OBJS) $(TEST_OBJS) \
			 $(LIBS) $(TEST).o -o $(TEST) && \
		echo "TEST $(TEST)" && \
		$(QEMU) $(QEMU_FLAGS) -kernel $(TEST))
	@echo "*** ALL $(words ${TESTS}) TESTS PASSED ***"

