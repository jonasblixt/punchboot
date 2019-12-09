BUILD = "build-$(shell ${CC} -dumpmachine)"


ifeq (,$(shell which pkg-config))
$(error "No pkg-config found")
endif

ifeq (,$(shell pkg-config --libs --cflags openssl))
$(error "No openssl detected")
endif

ifeq (,$(shell pkg-config --libs --cflags libpkcs11-helper-1))
$(error "No pkcs11-helper detected")
endif

GIT_VERSION = $(shell git describe --abbrev=4 --dirty --always --tags)

CC=$(CROSS_COMPILE)gcc
LD=$(CROSS_COMPILE)ld
AR=$(CROSS_COMPILE)ar
SIZE=$(CROSS_COMPILE)size
STRIP=$(CROSS_COMPILE)strip
OBJCOPY=$(CROSS_COMPILE)objcopy
YACC=$(shell which yacc)
LEX=$(shell which flex)

C_FLAGS   = -std=c99 -D_POSIX_C_SOURCE=200809L -Wall -I include/
C_FLAGS  += -DVERSION=\"$(GIT_VERSION)\"
C_FLAGS  += $(shell pkg-config --cflags openssl libpkcs11-helper-1)
C_FLAGS  += -MMD -MP
C_FLAGS  +=  -Werror -pedantic -fPIC -g -DREMOVE_ENCRYPTION -m64
C_FLAGS  += -I $(BUILD)/

LIBS    = $(shell pkg-config --libs openssl libpkcs11-helper-1)

CST_C_SRCS  = cst.c
CST_C_SRCS += acst.c
CST_C_SRCS += openssl_helper.c
CST_C_SRCS += adapt_layer_openssl.c
CST_C_SRCS += csf_cmd_aut_dat.c
CST_C_SRCS += csf_cmd_ins_key.c
CST_C_SRCS += csf_cmd_misc.c
CST_C_SRCS += err.c
CST_C_SRCS += pkey.c
CST_C_SRCS += ssl_wrapper.c
CST_C_SRCS += srk_helper.c

CST_Y_SRCS += cst_parser.y
CST_L_SRCS += cst_lexer.l

SRK_C_SRCS  = srktool.c
SRK_C_SRCS += srk_helper.c
SRK_C_SRCS += openssl_helper.c

CST_OBJS = $(patsubst %.c, $(BUILD)/%.o, $(CST_C_SRCS))
SRK_OBJS = $(patsubst %.c, $(BUILD)/%.o, $(SRK_C_SRCS))

CST_L_OBJS = $(patsubst %.l, $(BUILD)/%.c, $(CST_L_SRCS))
CST_Y_OBJS = $(patsubst %.y, $(BUILD)/%.c, $(CST_Y_SRCS))

CST_OBJS += $(CST_L_OBJS:.c=.o) $(CST_Y_OBJS:.c=.o)

DEPS      = $(OBJS:.o=.d) $(CST_OBJS:.o=.d) $(SRK_OBJS:.o=.d)

all: builddir cst srktool

builddir:
	@echo Build output: ${BUILD}
	@mkdir -p ${BUILD}

clean:
	@rm -rf $(BUILD)

cst : $(CST_L_OBJS) $(CST_Y_OBJS) $(OBJS) $(CST_OBJS)
	@echo LD $@
	@$(CC) $(OBJS) $(CST_OBJS) $(C_FLAGS) $(LIBS) -o $(BUILD)/$@

srktool: $(OBJS) $(SRK_OBJS)
	@echo LD $@
	@$(CC) $(OBJS) $(SRK_OBJS) $(C_FLAGS) $(LIBS) -o $(BUILD)/$@

%.o : %.c
$(BUILD)/%.o : %.c
	@echo CC $<
	@${CC} -c $< $(C_FLAGS) -o $@

$(BUILD)/%.c: %.y
	@echo YACC $@
	@$(YACC) -d -o $@ $<
$(BUILD)/%.c: %.l
	@echo LEX $@
	@$(LEX) -o $@ $<

-include $(DEPS)
