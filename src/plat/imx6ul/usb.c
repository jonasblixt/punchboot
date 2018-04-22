#include <string.h>
#include <regs.h>
#include <io.h>
#include <types.h>
#include <usb.h>
#include <tinyprintf.h>
#include <recovery.h>

#undef USB_DEBUG

#define BULK_BUFFER_SIZE 1024*1024 * 8
#define CMD_BUFFER_SIZE  1024*1024 * 1

#define BULK_NO_OF_DTDS BULK_BUFFER_SIZE/(1024*16)
#define INTR_NO_OF_DTDS CMD_BUFFER_SIZE/(1024*16)

static struct ehci_device _ehci_dev;

static u8 __no_bss __a4k bulk_buffer[BULK_BUFFER_SIZE];
static u8 __no_bss __a4k bulk_buffer2[BULK_BUFFER_SIZE];


static u8 __no_bss __a4k cmd_out_bfr[CMD_BUFFER_SIZE];
static u8 __no_bss __a4k cmd_in_bfr[CMD_BUFFER_SIZE];

static struct ehci_dTH __no_bss __a4k cmd_out_dtd[INTR_NO_OF_DTDS];
static struct ehci_dTH __no_bss __a4k cmd_in_dtd[INTR_NO_OF_DTDS];
static struct ehci_dTH __no_bss __a4k bulk_dtds[BULK_NO_OF_DTDS];
static struct ehci_dTH *dtd_transfer_tail = bulk_dtds;
static struct ehci_dTH *dtd_cmd_out_tail = cmd_out_dtd;


void soc_usb_init(u32 base_addr) {
    u32 reg;
    struct ehci_dQH * qh_out = &_ehci_dev.dqh[0];
    struct ehci_dQH * qh_in = &_ehci_dev.dqh[1];
    struct ehci_dQH * qh_bulk_out = &_ehci_dev.dqh[2];
    struct ehci_dQH * qh_intr_out = &_ehci_dev.dqh[4];
    struct ehci_dQH * qh_intr_in = &_ehci_dev.dqh[7];

    _ehci_dev.base = base_addr;

//    tfp_printf ("USB: Init...\n\r");

    /* Enable USB PLL */
    reg = pb_readl(REG(0x020C8000,0x10));
    reg |= (1<<6);
    pb_writel(reg, REG(0x020C8000, 0x10));

    /* Power up USB */
    pb_writel ((1 << 31) | (1 << 30), REG(0x020C9038,0));
    pb_writel(0xFFFFFFFF, REG(0x020C9008,0));
 
    reg = pb_readl(REG(_ehci_dev.base,USB_CMD));
    reg |= (1<<1);
    pb_writel(reg, REG(_ehci_dev.base,USB_CMD));

    while (pb_readl(REG(_ehci_dev.base, USB_CMD)) & (1<<1))
        asm("nop");
    
//    tfp_printf("USB: Reset complete\n\r");

    u32 usb_id = pb_readl(REG(_ehci_dev.base, 0));

//    tfp_printf("USB: ID=0x%8.8X\n\r",usb_id);

    u32 usb_dci_version = pb_readl(REG(_ehci_dev.base, USB_DCIVERSION));

//    tfp_printf("USB: Controller version: v%i.%i\n\r",(usb_dci_version >> 4)&0x0f,
//                                        (usb_dci_version & 0x0f));



    _ehci_dev.enumerated = 0;
    _ehci_dev.ready = 0;


    for (int i = 0; i < 16; i++) {
        _ehci_dev.dqh[i].caps = 0;
        _ehci_dev.dqh[i].next_dtd = 0xDEAD0001;
        _ehci_dev.dqh[i].dtd_token = 0;
        _ehci_dev.dqh[i].current_dtd = 0;
    }

    /* EP0 */
    qh_in->caps       = (1 << 29) | (0x40 << 16);
    qh_out->caps      = (1 << 29) | (0x40 << 16) ;
    /* EP1 BULK OUT*/
    qh_bulk_out->caps = (1 << 29) | (1 << 15) | (0x0200 << 16) ;
    /* EP2 INTR OUT */
    qh_intr_out->caps = (1 << 29) | (1 << 15) | (0x0040 << 16) ;
    /* EP3 INTR IN */
    qh_intr_in->caps  = (1 << 29) | (1 << 15) | (0x0200 << 16) ;
 
    /* Program QH top */
    pb_writel((u32) _ehci_dev.dqh, REG(_ehci_dev.base, USB_ENDPTLISTADDR)); 
  
    /* Enable USB */
    pb_writel(0x0A | (1 << 4), REG(_ehci_dev.base, USB_USBMODE));
    pb_writel( (0x40 << 16) |0x01, REG(_ehci_dev.base, USB_CMD));


    pb_writel(0xFFFFFFFF, REG(_ehci_dev.base, USB_USBSTS));

    pb_writel(0xFFFF, REG(_ehci_dev.base, USB_ENDPTSETUPSTAT));
    pb_writel((0xff << 16)  | 0xff, REG(_ehci_dev.base, USB_ENDPTCOMPLETE));

    while (pb_readl(REG(_ehci_dev.base, USB_ENDPTPRIME)))
        asm("nop");

    pb_writel(0xFFFFFFFF, REG(_ehci_dev.base, USB_ENDPTFLUSH));
 
    /* Wait for reset */
    while (!(pb_readl(REG(_ehci_dev.base, USB_USBSTS)) & (1<<6)))
        asm("nop");

   
    /* Wait for port to come out of reset */
    while (pb_readl(REG(_ehci_dev.base, USB_PORTSC1)) & (1<<8))
        asm("nop");
  
    pb_writel(7, REG(_ehci_dev.base, USB_SBUSCFG));
    pb_writel(0x0000FFFF, REG(_ehci_dev.base, USB_BURSTSIZE));


//    tfp_printf ("USB: Init completed\n\r");
}

u8 qf_Descriptor[] = {
	0x0A,	//USB_DEV_QUALIFIER_DESC_LEN,
	0x06,   //USB_DEV_QUALIFIER_DESC_TYPE,
	0x00,   //USB_DEV_DESC_SPEC_LB,
	0x02,   //USB_DEV_DESC_SPEC_HB,
	0x00,   //USB_DEV_DESC_DEV_CLASS,
	0x00,   //USB_DEV_DESC_DEV_SUBCLASS,
	0x00,   //USB_DEV_DESC_DEV_PROTOCOL,
	0x40,   //USB_DEV_DESC_EP0_MAXPACKETSIZE,
	0x01,   //USB_DEV_DESC_NUM_OT_SPEED_CONF,
	0		//USB_DEV_DESC_RESERVED
};

const struct usb_descriptors descriptors = {
    .device = {
        .bLength = 0x12, // length of this descriptor
        .bDescriptorType = 0x01, // Device descriptor
        .bcdUSB = 0x0200, // USB version 2.0
        .bDeviceClass = 0x00, // Device class (specified in interface descriptor)
        .bDeviceSubClass = 0x00, // Device Subclass (specified in interface descriptor)
        .bDeviceProtocol = 0x00, // Device Protocol (specified in interface descriptor)
        .bMaxPacketSize = 0x40, // Max packet size for control endpoint
        .idVendor = 0xffff, // Freescale Vendor ID. -- DO NOT USE IN A PRODUCT
        .idProduct = 0x0001, // Decvice ID -- DO NOT USE IN A PRODUCT
        .bcdDevice = 0x0000, // Device revsion
        .iManufacturer = 0x01, // Index of  Manufacturer string descriptor
        .iProduct = 0x01, // Index of Product string descriptor
        .iSerialNumber = 0x01, // Index of serial number string descriptor
        .bNumConfigurations = 0x01, // Number of configurations
    },
    .config = {
        .bLength = 0x09, //
        .bDescriptorType = 0x02, // Configuration descriptor
        .wTotalLength = 0x27, // Total length of data, includes interface, HID and endpoint
        .bNumInterfaces = 0x01, // Number of interfaces
        .bConfigurationValue = 0x01, // Number to select for this configuration
        .iConfiguration = 0x00, // No string descriptor
        .bmAttributes = 0x80, // Self powered, No remote wakeup
        .MaxPower = 0xfa 
    },
    .interface = {
        .bLength = 0x09,
        .bDescriptorType = 0x04, // Interface descriptor
        .bInterfaceNumber = 0x00, // This interface = #0
        .bAlternateSetting = 0x00, // Alternate setting
        .bNumEndpoints = 0x03, // Number of endpoints for this interface
        .bInterfaceClass = 0xFF, // HID class interface
        .bInterfaceSubClass = 0xFF, // Boot interface Subclass
        .bInterfaceProtocol = 0xFF, // Mouse protocol
        .iInterface = 0, // No string descriptor
    },
    .endpoint_bulk_out = {
        .bLength = 0x07,
        .bDescriptorType = 0x05, // Endpoint descriptor
        .bEndpointAddress = 0x01, 
        .bmAttributes = 0x2, 
        .wMaxPacketSize = 0x0200, 
        .bInterval = 0x00, // 10 ms interval
    },
    .endpoint_intr_out = {
        .bLength = 0x07,
        .bDescriptorType = 0x05, // Endpoint descriptor
        .bEndpointAddress = 0x02, 
        .bmAttributes = 0x3, 
        .wMaxPacketSize = 0x0040, 
        .bInterval = 0x05, 
    },
    .endpoint_intr_in = {
        .bLength = 0x07,
        .bDescriptorType = 0x05, // Endpoint descriptor
        .bEndpointAddress = 0x83, 
        .bmAttributes = 0x3, 
        .wMaxPacketSize = 0x0200, 
        .bInterval = 0x05,
    }
};

static void send_ep0_msg(u8 *bfr, u8 sz)
{
    struct ehci_dQH * qh_out = &_ehci_dev.dqh[0];
 
    struct ehci_dQH * qh_in = &_ehci_dev.dqh[1];
    struct ehci_dTH  __attribute__((aligned(4096)))dtd_in;
    struct ehci_dTH  __attribute__((aligned(4096)))dtd_out;
 
    u8 __attribute__((aligned(4096))) bfr_in[4096];
    u8 __attribute__((aligned(4096))) bfr_out[4096];
 
    //memcpy (bfr_in, bfr, sz);
    for (int i = 0; i < sz; i++)
        bfr_in[i] = bfr[i];


    dtd_in.next_dtd = 0xDEAD0001;
    dtd_in.dtd_token = (sz << 16) |  0x80  | (1 << 15);
    dtd_in.bfr_page0 = (u32) bfr_in;

    dtd_out.next_dtd = 0xDEAD0001;
    dtd_out.dtd_token = (0x40 << 16) | 0x80;
    dtd_out.bfr_page0 = (u32) bfr_out;

    qh_in->next_dtd = (u32) &dtd_in;
    qh_in->current_dtd = 0;
    qh_in->dtd_token = 0;

    qh_out->next_dtd = (u32) &dtd_out;
    qh_out->current_dtd = 0;
    qh_out->dtd_token = 0;

    
    pb_writel((1<<16), REG(_ehci_dev.base, USB_ENDPTPRIME));
    pb_writel((1<<0), REG(_ehci_dev.base, USB_ENDPTPRIME));
    
    while (pb_readl(REG(_ehci_dev.base, USB_ENDPTPRIME)) & (1 << 0) )
        asm("nop");


    while (! (pb_readl(REG(_ehci_dev.base, USB_USBSTS)) & 1) )
        asm("nop"); // Wait for irq

    pb_writel(1 << 16, REG(_ehci_dev.base, USB_ENDPTCOMPLETE));
    pb_writel(1 << 0, REG(_ehci_dev.base, USB_ENDPTCOMPLETE));

    while (qh_in->dtd_token & 0x80)
        asm("nop");

}

static void prep_next_cmd(void)
{
    struct ehci_dQH * qh_out = &_ehci_dev.dqh[4];
    struct ehci_dTH *dtd = cmd_out_dtd;


    dtd->next_dtd = 0xDEAD0001;
    dtd->dtd_token = (0x0040 << 16) | 0x80 | (1 << 15);
    dtd->bfr_page0 = (u32) cmd_out_bfr;
    dtd->bfr_page1 = 0;
    dtd->bfr_page2 = 0;
    dtd->bfr_page3 = 0;
    dtd->bfr_page4 = 0;


    qh_out->next_dtd = (u32) dtd;
    
    pb_writel((1<<2), REG(_ehci_dev.base, USB_ENDPTPRIME));
    
    while (pb_readl(REG(_ehci_dev.base, USB_ENDPTPRIME)) & (1 << 2) )
        asm("nop");
}



int plat_usb_prep_bulk_buffer(u16 no_of_blocks, u8 n)
{
    struct ehci_dTH * dtd = bulk_dtds;
    struct ehci_dQH * qh_out = &_ehci_dev.dqh[2];
    struct ehci_dTH * dtd_tmp;


    u8 *bulk_bfr_ptr = ((n == 0)?bulk_buffer:bulk_buffer2);
    u16 blocks_remaining = no_of_blocks;
    u16 blocks_to_tx = 0;
    u16 dtd_counter = 0;
 
    qh_out->current_dtd = 0;
    qh_out->next_dtd = 0xDEAD0001;
    qh_out->dtd_token = 0;

    while (blocks_remaining) {
        blocks_to_tx = blocks_remaining > 32?32:blocks_remaining;
        dtd->dtd_token = (blocks_to_tx*512 << 16) |  0x80;
        dtd->bfr_page0 = (u32) bulk_bfr_ptr;
        bulk_bfr_ptr += 4096;
        dtd->bfr_page1 = (u32) bulk_bfr_ptr;
        bulk_bfr_ptr += 4096;
        dtd->bfr_page2 = (u32) bulk_bfr_ptr;
        bulk_bfr_ptr += 4096;
        dtd->bfr_page3 = (u32) bulk_bfr_ptr;
        bulk_bfr_ptr += 4096;
        dtd->bfr_page4 = 0;

        blocks_remaining -= blocks_to_tx;

        if (!blocks_remaining)  {
            dtd->dtd_token |= (1 << 15);
            dtd->next_dtd = 0xDEAD0001;
            dtd_transfer_tail = dtd;
            break;
        }
        dtd_tmp = dtd;
        dtd++;
        dtd_tmp->next_dtd = (u32) dtd;
        dtd_counter ++;

        if (dtd_counter > BULK_NO_OF_DTDS) 
            return 0;
    }
    
    qh_out->next_dtd = (u32) bulk_dtds;  
    pb_writel((1 << 1), REG(_ehci_dev.base, USB_ENDPTPRIME));
    
    while (pb_readl(REG(_ehci_dev.base, USB_ENDPTPRIME)) & (1 << 1))
        asm("nop");

    return 0;
}

int plat_usb_send(u8 *bfr, u32 sz) {
    struct ehci_dQH * qh_in = &_ehci_dev.dqh[7]; // EP3 IN
    u16 bytes_to_tx = 0;
    struct ehci_dTH *dtd_tmp = NULL;
    struct ehci_dTH *dtd = cmd_in_dtd;
    u32 bytes_remaining = sz;
    u8 *p = cmd_in_bfr;
    u8 dtd_count = 0;
     if (sz > CMD_BUFFER_SIZE)
         bytes_remaining = CMD_BUFFER_SIZE;
       

    for (int i = 0; i < bytes_remaining; i++) // TODO: Replace when memcpy works...
        cmd_in_bfr[i] = bfr[i];

    while (bytes_remaining) {
        
        bytes_to_tx = bytes_remaining>0x4000?0x4000:bytes_remaining; 

        dtd->dtd_token = (bytes_to_tx << 16) | 0x80;
        dtd->bfr_page0 = (u32) p;
        p += 4096;
        dtd->bfr_page1 = (u32) p;
        p += 4096;
        dtd->bfr_page2 = (u32) p;
        p += 4096;
        dtd->bfr_page3 = (u32) p;
        p += 4096;
        dtd->bfr_page4 = 0;

        dtd_tmp = dtd;
        bytes_remaining -= bytes_to_tx;
        dtd_count++;

        dtd++;
        dtd_tmp->next_dtd = (u32) dtd;
 
        if (!bytes_remaining) {
            dtd_tmp->dtd_token |= (1<<15);
            dtd_tmp->next_dtd = 0xDEAD0001;
            break;
        }

   }

    qh_in->next_dtd = (u32) &cmd_in_dtd;
    
    pb_writel((1<<19), REG(_ehci_dev.base, USB_ENDPTPRIME));
    
    while (pb_readl(REG(_ehci_dev.base, USB_ENDPTPRIME)) & (1 << 19) )
        asm("nop");

    return 0;
}


static void usb_process_ep0(void)
{
    u16 request;
    struct usbdSetupPacket *setup = &_ehci_dev.setup;
 

    u32 cmd_reg = pb_readl(REG(_ehci_dev.base,USB_CMD));
 
    
    do {
        pb_writel(cmd_reg | (1 << 13), REG(_ehci_dev.base, USB_CMD));
        memcpy(setup, _ehci_dev.dqh[0].setup, 8);
    } while (! (pb_readl(REG(_ehci_dev.base, USB_CMD)) & (1<<13)));
    
    request = (setup->bRequestType << 8) | setup->bRequest;
    
    pb_writel(1, REG(_ehci_dev.base,USB_ENDPTSETUPSTAT));
    
    cmd_reg = pb_readl(REG(_ehci_dev.base,USB_CMD));
    pb_writel(cmd_reg & ~(1 << 13), REG(_ehci_dev.base, USB_CMD));

    pb_writel ((1<< 16) | 1, REG(_ehci_dev.base, USB_ENDPTFLUSH));

    while (pb_readl(REG(_ehci_dev.base, USB_ENDPTSETUPSTAT)) & 1)
        asm("nop");
#ifdef USB_DEBUG
    tfp_printf ("USB: EP0 %4.4X %4.4X %ib\n\r", request, setup->wValue, setup->wLength);
#endif
    u16 sz = 0;
    u16 device_status = 0;
    switch (request) {
        case GET_DESCRIPTOR:
            if(setup->wValue == 0x0600) {
                send_ep0_msg(qf_Descriptor, sizeof(qf_Descriptor));
            } else if (setup->wValue == 0x0100) {
                
                sz = sizeof(struct usb_device_descriptor);

                if (setup->wLength < sz)
                    sz = setup->wLength;

                send_ep0_msg( (u8 *) &descriptors.device, sz);

            } else if (setup->wValue == 0x0200) {
                u16 desc_tot_sz = descriptors.config.wTotalLength;

                sz = desc_tot_sz;

                if (setup->wLength < sz)
                    sz = setup->wLength;

                send_ep0_msg( (u8 *) &descriptors.config, sz);
            } else if (setup->wValue == 0x0300) { 
                const u8 _usb_strings[] = "\x04\x03\x04\x09";
                send_ep0_msg( (u8 *) _usb_strings, 4);
            } else if(setup->wValue == 0x0301) {
                
                const u8 _usb_string_id[] = 
                    {0x16,3,'P',0,'u',0,'n',0,'c',0,'h',0,' ',0,'B',0,'O',0,'O',0,'T',0};

                sz = setup->wLength > sizeof(_usb_string_id)?
                            sizeof(_usb_string_id): setup->wLength;
                
                send_ep0_msg( (u8 *) _usb_string_id, sz);
     
            } else if (setup->wValue == 0x0A00) {
                 u16 desc_tot_sz = descriptors.interface.bLength;

                sz = desc_tot_sz;

                if (setup->wLength < sz)
                    sz = setup->wLength;

                send_ep0_msg( (u8 *) &descriptors.interface, sz);
                
            } else {
                tfp_printf ("USB: Unhandeled descriptor 0x%4.4X\n\r", setup->wValue);
            }
        break;
        case SET_ADDRESS:
            pb_writel( (setup->wValue << 25) | ( 1 << 24 ),
                    REG(_ehci_dev.base, USB_DEVICEADDR));
            send_ep0_msg(NULL,0);
            //tfp_printf ("USB: Set address: %i\n\r",setup->wValue);
        break;
        case SET_CONFIGURATION:
            //tfp_printf ("USB: Set configuration\n\r");
            send_ep0_msg(NULL, 0);

            /* Configure EP 1 as bulk OUT */
            pb_writel ((1 << 7) | (2 << 2) , REG(_ehci_dev.base, USB_ENDPTCTRL1));

            /* Configure EP 2 as intr OUT */
            pb_writel ((1 << 7) | (3 << 2) , REG(_ehci_dev.base, USB_ENDPTCTRL2));
            prep_next_cmd();

            /* Configure EP3 as intr IN */
            pb_writel((1 << 23) | (3 << 18), REG(_ehci_dev.base, USB_ENDPTCTRL3));
 
        break;
        case SET_IDLE:
            send_ep0_msg(NULL,0);
        break;
        case GET_STATUS:
            send_ep0_msg((u8 *) &device_status, 2);
            send_ep0_msg(NULL,0);
        break;
       default:
            tfp_printf ("USB: EP0 Unhandled request %4.4x\n\r",request);
            tfp_printf (" bRequestType = 0x%2.2x\n\r", setup->bRequestType);
            tfp_printf (" bRequest = 0x%2.2x\n\r", setup->bRequest);
            tfp_printf (" wValue = 0x%4.4x\n\r", setup->wValue);
            tfp_printf (" wIndex = 0x%4.4x\n\r",setup->wIndex);
            tfp_printf (" wLength = 0x%4.4x\n\r",setup->wLength);
    }
}

void soc_usb_task(void) {
    u32 sts = pb_readl(REG(_ehci_dev.base, USB_USBSTS));
    u32 epc = pb_readl(REG(_ehci_dev.base, USB_ENDPTCOMPLETE));

    pb_writel(0xFFFFFFFF, REG(_ehci_dev.base, USB_USBSTS));

    if  (pb_readl(REG(_ehci_dev.base, USB_ENDPTSETUPSTAT)) & 1) {
        usb_process_ep0();
        return;
    }

    /* EP2 INTR OUT */
    if  (epc & (1 << 2)) {
        if (! (cmd_out_dtd[0].dtd_token & 0x80)) {
            recovery_cmd_event((struct pb_usb_cmd *) cmd_out_bfr, 
                    bulk_buffer, bulk_buffer2);
       }
        prep_next_cmd();
        pb_writel( (1 << 2) , REG(_ehci_dev.base, USB_ENDPTCOMPLETE));    
   }

    /* EP1 OUT Bulk transfers */
    if  (epc & (1 << 1)) {
         pb_writel( (1 << 1) , REG(_ehci_dev.base, USB_ENDPTCOMPLETE));    
      /*  if (! (dtd_transfer_tail->dtd_token & 0x80)) {
            tfp_printf ("USB: EP1 xfer done\n\r");
        }*/
       
    }

    /* EP3 IN Intr transfers */
    if  (epc & (1 << 19)) {
         pb_writel( (1 << 19) , REG(_ehci_dev.base, USB_ENDPTCOMPLETE));    
        //tfp_printf ("USB: EP3 IRQ %x\n\r",_ehci_dev.dqh[7].dtd_token);
        if (!(_ehci_dev.dqh[7].dtd_token & 0x80)) {
            //tfp_printf("USB: EP3 IN done\n\r");
        }
    }
    if (sts & 2) {
        pb_writel (2, REG(_ehci_dev.base, USB_USBSTS));
        tfp_printf ("USB: Error IRQ\n\r");
    }
}
