src-$(CONFIG_LITTLE_FS) += src/drivers/fs/lfs.c
src-$(CONFIG_LITTLE_FS) += src/drivers/fs/lfs_util.c
cflags-$(CONFIG_LITTLE_FS) += -DLFS_NO_MALLOC -DLFS_NO_ASSERT
