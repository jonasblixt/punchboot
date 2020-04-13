
bin_PROGRAMS = pbstate

pbstate_SOURCES  = main.c \
				   crc.c \
				   pbconfig.c

pbstate_CFLAGS  = $(BLKID_CFLAGS) \
                  $(UUID_CFLAGS) \
				  -DPRIMARY_STATE_UUID=\"$(PRIMARY_STATE_UUID)\" \
				  -DBACKUP_STATE_UUID=\"$(PRIMARY_STATE_UUID)\"

pbstate_LDADD = $(BLKID_LIBS) \
				$(UUID_LIBS)

