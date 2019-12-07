
Platform API
============

The platform API provides the abstraction between the main logic and the board
code from the platform. In this context a platform is defined as a speficic
SoC, for example, imx6ul or imx8x.

API Scope
---------
- Watchdog interface
- Monotonic counter / delay function
- Block storage interface
- Crypto interface
- USB Interface
- UART Interface
- Fusebox Interface
- Security configuration interface


Watchdog Interface
------------------

.. code-block:: c

   void plat_wdog_init(void);
   void plat_wdog_kick(void);


plat_wdog_init initializes the watchdog with a pre-set time defined by the 
board file.

plat_wdog_kick kicks the watchdog.

Monotonic counters / delay
--------------------------

.. code-block:: c

   uint32_t plat_get_us_tick(void);
   void plat_delay_ms(uint32_t delay_ms);

plat_get_us_tick returns the number of microseconds since a platform defined
couter/timer has been initialized. If this is done early in the init process
it will give a resonably good reference tick inside the bootloader.

plat_delay_ms, delay 'delay_ms' milli seconds

Misc calls
----------

.. code-block:: c

   void      plat_reset(void);
   uint32_t  plat_early_init(void);
   void      plat_preboot_cleanup(void);
   bool      plat_force_recovery(void);
   uint32_t  plat_prepare_recovery(void);
   uint32_t  plat_get_params(struct param **pp);
   uint32_t  plat_get_uuid(char *out);

plat_reset - Reset the system

plat_early_init()
Calls board_early_init() first. plat_early_init() should initialize platform
relevant io, such as uart, watchdog, clocks

board_early_init() is responsible for setting up board specific things. This could
for example be, enable pins that must be set, power that must be enable to 
some block that is specific to that board.

plat_preboot_cleanup() called before PB jumps into the target. Could for example
be reset a hardware block because the target firmware expects the system
to be in a specific state.

plat_prepare_recovery() Perform necessary setup work to allow the recovery
mode to run. Mostly setup USB.

plat_get_params() Returns a param -list of parameters for a specific platform.
The required parameters for any platform is "Device UUID" and a platform name
"Platform Name". These parameters are exposed throug the punchboot CLI when
the dev -l command is issued.

plat_get_uuid() Returns a device unique UUID3. This UUID must be generated
as a combination of device unique data and a platform namespace UUID.

Block storage interface
-----------------------

.. code-block:: c

    uint32_t  plat_write_block(uint32_t lba_offset,
                                    uintptr_t bfr,
                                    uint32_t no_of_blocks);

    uint32_t  plat_read_block(uint32_t lba_offset,
                              uintptr_t bfr,
                              uint32_t no_of_blocks);

    uint32_t  plat_switch_part(uint8_t part_no);
    uint64_t  plat_get_lastlba(void);
    uint32_t plat_flush_block(void);

    uint32_t plat_write_block_async(uint32_t lba_offset,
                              uintptr_t bfr,
                              uint32_t no_of_blocks);

plat_write_block() write's 'no_of_blocks' 512 byte blocks to the offset 'lba_offset'
from the buffer 'bfr'. Returns PB_OK if operation was sucessfull.

plat_read_block() read's 'no_of_blocks' 512 byte blocks to the buffer 'bfr'
from the offset 'lba_offset'. Returns PB_OK if operation was sucessfull.

plat_switch_part() eMMC specific command to switch between BOOT0/BOOT1,
User area. Returns PB_OK if operation was sucessfull.

plat_get_lastlba() Returns the last usable block.

plat_write_block_async() same as plat_write_block() but will not block until
operation is completed.

plat_flush_block() Flushes any ongoing async writes. This will block until
the writes have completed and returns PB_OK on sucessful completion.

Crypto Interface
----------------

.. code-block:: c

    uint32_t  plat_hash_init(uint32_t hash_kind);
    uint32_t  plat_hash_update(uintptr_t bfr, uint32_t sz);
    uint32_t  plat_hash_finalize(uintptr_t out);

    uint32_t  plat_verify_signature(uint8_t *sig, uint32_t sig_kind,
                                    uint8_t *hash, uint32_t hash_kind,
                                    struct pb_key *k);

plat_hash_init() initialize a hashing context of type 'hash_kind'.

hash kind can be:
   - PB_HASH_MD5
   - PB_HASH_SHA256
   - PB_HASH_SHA384
   - PB_HASH_SHA512
Only one hash context can be active at any given time.

plat_hash_update() Update current hash context with 'sz' bytes from buffer
 'bfr'.

plat_hash_finalize() Output final hash into 'out'. It must be ensured by
the user to have enough space allocated to accomodate the hash result.

plat_verify_signature() Verify signature 'sig' of kind 'sig_kind' with 
the calculated hash 'hash' of kind 'hash'_kind' using the key 'k'.

sig_kind can be:
   - PB_SIGN_RSA4096
   - PB_SIGN_ECP256
   - PB_SIGN_ECP384
   - PB_SIGN_ECP521

Returns PB_OK on sucessful verification.

USB Interface
-------------

.. code-block:: c

    uint32_t  plat_usb_init(struct usb_device *dev);
    void      plat_usb_task(struct usb_device *dev);
    uint32_t  plat_usb_transfer(struct usb_device *dev, uint8_t ep,
                                uint8_t *bfr, uint32_t sz);
    void      plat_usb_set_address(struct usb_device *dev, uint32_t addr);
    void      plat_usb_set_configuration(struct usb_device *dev);
    void      plat_usb_wait_for_ep_completion(struct usb_device *dev,
                                                uint32_t ep);

plat_usb_init() Initializes usb device 'dev'. Returns PB_OK on success.

plat_usb_task() Is called periodically by the main loop when recovery mode
is entered. It's used to pull interrupt events from the USB hardware.

plat_usb_transfer() Transfer 'sz' bytes of data from 'bfr' to usb device 'dev'
on enpoint 'ep'. Returns PB_OK on success.

plat_usb_set_address() Called by the generic usb code when an USB address
has been assigned by the host.

plat_usb_set_configuration() Enable endpoints and prepare to receive data.

plat_usb_wait_for_ep_completion() Wait for a transfere to complete on device
'dev' and endpoint 'ep'.

UART Interface
--------------

.. code-block:: c

    void      plat_uart_putc(void *ptr, char c);

plat_uart_putc() Output character 'c' on debug uart. 'ptr' parameter is 
ignored and only put there to be compatiable with the libc/printf.

Fusebox Interface
-----------------

.. code-block:: c

    uint32_t  plat_fuse_read(struct fuse *f);
    uint32_t  plat_fuse_write(struct fuse *f);
    uint32_t  plat_fuse_to_string(struct fuse *f, char *s, uint32_t n);

plat_fuse_read() read fuse 'f'. 'f' must point to an already initialized
structure. The result is stored in the same structure. The function returns
PB_OK on success.

plat_fuse_write() program fuse 'f'

plat_fuse_to_string() Stores fuse 'f' texutal description in buffer 's' which
is of max length 'n'.

Fuse structure:

.. code-block:: c

    struct fuse
    {
        uint32_t bank;
        uint32_t word;
        uint32_t shadow;
        uint32_t addr;
        volatile uint32_t value;
        uint32_t default_value;
        uint32_t status;
        const char description[20];
    };

Security configuration interface
--------------------------------

.. code-block:: c

    uint32_t plat_setup_device(struct param *params);
    uint32_t plat_setup_lock(void);
    uint32_t plat_get_security_state(uint32_t *state);

plat_setup_device() Configure all fuses and setup secure boot without
commiting to enforced secure boot. i.e. The system will boot even if the
bootloader can't successfuly authenticated.

plat_setup_lock() Perform final fusing and enables enforcment of secure boot.
After this command has been issued the device is enforcing secure boot and
this setting can't be reverted.

plat_get_security_state() Returns the current security state.

Security states:
    - PB_SECURITY_STATE_NOT_SECURE
    - PB_SECURITY_STATE_CONFIGURED_ERR
    - PB_SECURITY_STATE_CONFIGURED_OK
    - PB_SECURITY_STATE_SECURE
