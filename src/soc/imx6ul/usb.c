#include <string.h>
#include <regs.h>
#include <io.h>
#include <types.h>
#include <usb.h>
#include <tinyprintf.h>
#include <recovery.h>

#define BULK_BUFFER_SIZE 1024*1024*8
#define BULK_NO_OF_DTDS BULK_BUFFER_SIZE/(1024*16)
static struct ehci_device _ehci_dev;
static u8 __attribute__ ((aligned(4096))) __attribute__((section (".bigbuffer"))) bulk_buffer[BULK_BUFFER_SIZE];
static  struct ehci_dTH  __attribute__((aligned(4096))) bulk_dtds[512];
 
static volatile u32 _base_addr;
static u32 version = 0x51AABBCC;

void soc_usb_init(u32 base_addr){
    u32 reg;
    struct ehci_dQH * qh_out = &_ehci_dev.dqh[0];
    struct ehci_dQH * qh_in = &_ehci_dev.dqh[1];
    struct ehci_dQH * qh_bulk_out = &_ehci_dev.dqh[2];
 
    _base_addr = base_addr;

    _ehci_dev.base = base_addr;

    tfp_printf ("USB Init...\n\r");

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
    
    tfp_printf("USB Reset complete\n\r");

    u32 usb_id = pb_readl(REG(_ehci_dev.base, 0));

    tfp_printf("USB ID: 0x%8.8X\n\r",usb_id);

    u32 usb_dci_version = pb_readl(REG(_base_addr, USB_DCIVERSION));

    tfp_printf("USB Controller version: v%i.%i\n\r",(usb_dci_version >> 4)&0x0f,
                                        (usb_dci_version & 0x0f));


    _ehci_dev.enumerated = 0;
    _ehci_dev.ready = 0;


    for (int i = 0; i < 32; i++) {
        _ehci_dev.dqh[i].caps = 0;
        _ehci_dev.dqh[i].next_dtd = 0xDEAD0001;
        _ehci_dev.dqh[i].dtd_token = 0;
        _ehci_dev.dqh[i].current_dtd = 0;
    }

    qh_in->caps = (1 << 29) | (0x40 << 16);
    qh_in->next_dtd = 0xDEAD0001;
    qh_in->current_dtd = 0;
    qh_in->dtd_token = 0;


    qh_out->caps = (1 << 29) | (0x40 << 16) ;
    qh_out->dtd_token = 0;
    qh_out->current_dtd = 0;
    qh_out->next_dtd = 0xDEAD0001;



    qh_bulk_out->caps = (1 << 29) | (1 << 15) | /*(1 << 30) |*/(0x0200 << 16) ;
    qh_bulk_out->dtd_token = 0;
    qh_bulk_out->current_dtd = 0;
    qh_bulk_out->next_dtd = 0xDEAD001;

    /* Program QH top */
    pb_writel((u32) _ehci_dev.dqh, REG(_base_addr, USB_ENDPTLISTADDR)); 


  
    /* Enable USB */
    pb_writel(0x0A | (1 << 4), REG(_base_addr, USB_USBMODE));
    pb_writel( (0x40 << 16) |0x01, REG(_ehci_dev.base, USB_CMD));


    pb_writel(0xFFFFFFFF, REG(_base_addr, USB_USBSTS));

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
 
    tfp_printf ("USB Init completed\n\r");
}

static void send_zlp(u8 ep) {
    struct ehci_dQH * qh_in = &_ehci_dev.dqh[ep*2+1];
    struct ehci_dTH  __attribute__((aligned(4096))) dtd_in;
 
    dtd_in.next_dtd = 0xDEAD0001;
    dtd_in.dtd_token =  0x80 | (1 << 15);

    qh_in->next_dtd = (u32) &dtd_in;

    pb_writel((1<<16) , REG(_base_addr, USB_ENDPTPRIME));
   
    while (pb_readl(REG(_ehci_dev.base, USB_ENDPTPRIME)) & (1 << ep) )
        asm("nop");


    while (! (pb_readl(REG(_ehci_dev.base, USB_USBSTS)) & 1))
        asm("nop"); // Wait for irq

    pb_writel(1 << 16, REG(_ehci_dev.base, USB_ENDPTCOMPLETE));


    while (qh_in->dtd_token & 0x80)
        asm("nop");

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

const struct usb_descriptors __attribute__ ((align(32))) descriptors = {
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
        .wTotalLength = 0x20, // Total length of data, includes interface, HID and endpoint
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
        .bNumEndpoints = 0x02, // Number of endpoints for this interface
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
    .endpoint_bulk_in = {
        .bLength = 0x07,
        .bDescriptorType = 0x05, // Endpoint descriptor
        .bEndpointAddress = 0x82, 
        .bmAttributes = 0x2, 
        .wMaxPacketSize = 0x0200, 
        .bInterval = 0x0, // 10 ms interval
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
 
    // memcpy (bfr_in, bfr, sz_to_tx);
    
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

    
    pb_writel((1<<16), REG(_base_addr, USB_ENDPTPRIME));
    pb_writel((1<<0), REG(_base_addr, USB_ENDPTPRIME));
    
    while (pb_readl(REG(_ehci_dev.base, USB_ENDPTPRIME)) & (1 << 0) )
        asm("nop");


    while (! (pb_readl(REG(_ehci_dev.base, USB_USBSTS)) & 1) )
        asm("nop"); // Wait for irq

    pb_writel(1 << 16, REG(_ehci_dev.base, USB_ENDPTCOMPLETE));
    pb_writel(1 << 0, REG(_ehci_dev.base, USB_ENDPTCOMPLETE));




    while (qh_in->dtd_token & 0x80)
        asm("nop");

}



static int prep_bulk_buffer(uint16_t no_of_blocks)
{
    struct ehci_dTH * dtd = bulk_dtds;
    struct ehci_dQH * qh_out = &_ehci_dev.dqh[2];
    struct ehci_dQH * qh_in = &_ehci_dev.dqh[3];
 
    u8 *bulk_bfr_ptr = bulk_buffer;
    u16 blocks_remaining = no_of_blocks;
    u16 blocks_to_tx = 0;
    u16 dtd_counter = 0;
 
    qh_out->current_dtd = 0;
    qh_out->next_dtd = 0xDEAD0001;
    qh_out->dtd_token = 0;

    qh_in->current_dtd = dtd;
    qh_in->next_dtd = 0xDEAD0001;
    qh_in->dtd_token = 0;

    //while (blocks_remaining) {
    //blocks_to_tx = blocks_remaining > 32?32:blocks_remaining;
    dtd->next_dtd = 0xDEAD0001;
    dtd->dtd_token = (512 << 16) |  0x80 | ( 1 << 15);
    dtd->bfr_page0 = (u32) bulk_bfr_ptr;

/*
        blocks_remaining -= blocks_to_tx;

        if (!blocks_remaining)  {
            dtd->dtd_token |= (1 << 15);
            dtd->next_dtd = 0xDEAD0001;
            tfp_printf ("LB: %i\n\r",dtd_counter);
        }
        dtd++;
        dtd_counter ++;

        if (dtd_counter > BULK_NO_OF_DTDS) 
            return 0;
    }
  */  
    //tfp_printf ("Priming... %i\n\r",dtd_counter);
    qh_out->next_dtd = (u32) bulk_dtds;  
    

   pb_writel((2 << 0), REG(_base_addr, USB_ENDPTPRIME));
    //pb_writel((2 << 16), REG(_base_addr, USB_ENDPTPRIME));
    
    while (pb_readl(REG(_ehci_dev.base, USB_ENDPTPRIME)) & (2 << 0))
        asm("nop");

    


   // while (qh_out->dtd_token & 0x80)
    //    asm("nop");

    tfp_printf ("dQH.current_dtd = %x\n\r",(u32) qh_out->current_dtd);
    tfp_printf ("first dTH %x\n\r",(u32) bulk_dtds);
/*
    tfp_printf ("EP1_OUT primed, %i blocks, %i dtd's\n\r",no_of_blocks, dtd_counter);
    tfp_printf ("%x\n\r",bulk_dtds[0].dtd_token);
    tfp_printf ("ENDPTSTAT = %x\n\r",pb_readl(REG(_ehci_dev.base, USB_ENDPTSTAT)));
    tfp_printf ("USBSTAT = %x\n\r",pb_readl(REG(_ehci_dev.base, USB_USBSTS)));
    tfp_printf ("dQH.dtd_token = %x\n\r",qh_out->dtd_token);
    tfp_printf ("dtd[0].token = %x\n\r",bulk_dtds[0].dtd_token);
*/    return 1;
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
    
    pb_writel(1, REG(_base_addr,USB_ENDPTSETUPSTAT));
    
    cmd_reg = pb_readl(REG(_ehci_dev.base,USB_CMD));
    pb_writel(cmd_reg & ~(1 << 13), REG(_ehci_dev.base, USB_CMD));

    pb_writel ((1<< 16) | 1, REG(_ehci_dev.base, USB_ENDPTFLUSH));

    while (pb_readl(REG(_ehci_dev.base, USB_ENDPTSETUPSTAT)) & 1)
        asm("nop");
/*
    tfp_printf("bRequestType = 0x%2.2x\n\r", setup->bRequestType);
    tfp_printf ("bRequest = 0x%2.2x\n\r", setup->bRequest);
    tfp_printf ("wValue = 0x%4.4x\n\r", setup->wValue);
    tfp_printf ("wIndex = 0x%4.4x\n\r",setup->wIndex);
    tfp_printf ("wLength = 0x%4.4x\n\r",setup->wLength);
*/
    tfp_printf ("%4.4X %4.4X %ib\n\r", request, setup->wValue, setup->wLength);
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
                tfp_printf ("Unhandeled descriptor 0x%4.4X\n\r", setup->wValue);
            }
        break;
        case SET_ADDRESS:
            pb_writel( (setup->wValue << 25) | ( 1 << 24 ),
                    REG(_ehci_dev.base, USB_DEVICEADDR));
            send_zlp(0);
            tfp_printf ("Set address: %i\n\r",setup->wValue);
        break;
        case SET_CONFIGURATION:
            tfp_printf ("Set configuration\n\r");
            send_zlp(0);
    /* Configure EP 1 */

    pb_writel ((1 << 7) | (2 << 2) , REG(_ehci_dev.base, USB_ENDPTCTRL1));
   // pb_writel (2, REG(_ehci_dev.base, USB_ENDPTNAKEN));
 
            prep_bulk_buffer(100);
        break;
        case SET_IDLE:
            send_zlp(0);
        break;
        case GET_STATUS:
            send_ep0_msg((u8 *) &device_status, 2);
            send_zlp(0);
        break;
        case PB_PREP_BUFFER:
            //tfp_printf ("PREP BUFFER\n\r");
            //if (prep_bulk_buffer(setup->wValue))
            send_ep0_msg(NULL, 0);
        case 0xa123:
            send_ep0_msg((u8 *) &version, 4);
        break;
       break;
      default:
            tfp_printf ("Unhandled request %4.4x\n\r",request);
            tfp_printf ("bRequestType = 0x%2.2x\n\r", setup->bRequestType);
            tfp_printf ("bRequest = 0x%2.2x\n\r", setup->bRequest);
            tfp_printf ("wValue = 0x%4.4x\n\r", setup->wValue);
            tfp_printf ("wIndex = 0x%4.4x\n\r",setup->wIndex);
            tfp_printf ("wLength = 0x%4.4x\n\r",setup->wLength);


    }
    
    //tfp_printf ("<\n\r");
}

void soc_usb_task(void) {
    u32 sts = pb_readl(REG(_ehci_dev.base, USB_USBSTS));
    
    pb_writel(0xFFFFFFFF, REG(_ehci_dev.base, USB_USBSTS));

    if  (pb_readl(REG(_base_addr, USB_ENDPTSETUPSTAT)) & 1) {
        usb_process_ep0();
    }
    if  (pb_readl(REG(_base_addr, USB_ENDPTSETUPSTAT)) & 2)
        tfp_printf("EP1: Got setup packet\n\r");


    
    if  (pb_readl(REG(_base_addr, USB_ENDPTCOMPLETE)) & 2) {
        tfp_printf ("EP1 ACT\n\r");

        pb_writel(2, REG(_base_addr, USB_ENDPTCOMPLETE));
    }


    if (sts & 2) {
        pb_writel (2, REG(_ehci_dev.base, USB_USBSTS));
        tfp_printf ("USB Error IRQ\n\r");
    }



    if (_ehci_dev.enumerated && !_ehci_dev.ready) {
        _ehci_dev.ready = 1;
        tfp_printf ("USB enumeration done\n\r");
    
    }

    if (_ehci_dev.ready) {
    
    
    }
}
