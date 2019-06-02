PB_BOARD_NAME = pico8ml
PB_PLAT_NAME  = imx8m
PB_ENTRY      = 0x7E1000

CFLAGS += -I board/pico8ml/include

BOARD_C_SRCS += board/pico8ml/pico8ml.c
BOARD_C_SRCS += board/pico8ml/ddr_init.c
BOARD_C_SRCS += board/pico8ml/ddrphy_train.c
BOARD_C_SRCS += board/pico8ml/helper.c
BOARD_C_SRCS += boot/atf_linux.c

MKIMAGE         ?= mkimage_imx8_imx8m
FINAL_IMAGE     = $(TARGET).imx

KEYS  = ../pki/secp256r1-pub-key.der
#KEYS = ../pki/dev_rsa_public.der
#KEYS += ../pki/field1_rsa_public.der
#KEYS += ../pki/field2_rsa_public.der

KEY_TYPE = EC

board_final: $(BUILD_DIR)/$(TARGET).bin
	@echo "board_final"
	$(MKIMAGE) -loader $(BUILD_DIR)/$(TARGET).bin $(PB_ENTRY) -out $(BUILD_DIR)/$(TARGET).imx 
	@echo "mkimage done"

