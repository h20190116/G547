
#include<linux/kernel.h>
#include<linux/module.h>
#include<linux/usb.h>
#include<linux/slab.h>
#include<linux/errno.h>

#define HPPD_VID  0x03F0
#define HPPD_PID  0x3640

/*#define SAMSUNG_MEDIA_VID  0x04e8
#define SAMSUNG_MEDIA_PID  0x6860 */


#define BULK_EP_IN    0x81
#define BULK_EP_OUT   0x02

#define RETRY_MAX                     5
#define REQUEST_SENSE_LENGTH          0x12
#define INQUIRY_LENGTH                0x24
#define READ_CAPACITY_LENGTH          0x08

#define be_to_int32(buf) (((buf)[0]<<24)|((buf)[1]<<16)|((buf)[2]<<8)|(buf)[3])

struct command_block_wrapper {
	uint8_t dCBWSignature[4];
	uint32_t dCBWTag;
	uint32_t dCBWDataTransferLength;
	uint8_t bmCBWFlags;
	uint8_t bCBWLUN;
	uint8_t bCBWCBLength;
	uint8_t CBWCB[16];
};

// Section 2: Command Status Wrapper (CSW)
struct command_status_wrapper {
	uint8_t dCSWSignature[4];
	uint32_t dCSWTag;
	uint32_t dCSWDataResidue;
	uint8_t bCSWStatus;
};

static uint8_t cdb_length[256] = {
//	 0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F
	06,06,06,06,06,06,06,06,06,06,06,06,06,06,06,06,  //  0
	06,06,06,06,06,06,06,06,06,06,06,06,06,06,06,06,  //  1
	10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,  //  2
	10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,  //  3
	10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,  //  4
	10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,  //  5
	00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,  //  6
	00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,  //  7
	16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,  //  8
	16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,  //  9
	12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,  //  A
	12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,  //  B
	00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,  //  C
	00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,  //  D
	00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,  //  E
	00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,  //  F
};


struct usbdev_private
{
	struct usb_device *udev;
	unsigned char class;
	unsigned char subclass;
	unsigned char protocol;
	unsigned char ep_in;
	unsigned char ep_out;
};

struct usbdev_private *p_usbdev_info;

static void usbdev_disconnect(struct usb_interface *interface)
{
	printk(KERN_INFO "USBDEV Device Removed\n");
	return;
}

static struct usb_device_id usbdev_table [] = {
	{USB_DEVICE(HPPD_VID, HPPD_PID)},
	{} /*terminating entry*/	
};


static int cbw_bulk_message(struct usb_device *udev, uint8_t *cdb)
{
	//uint8_t lun;
	uint32_t max_lba, block_size;
	double device_size;
	int retval, ret;
    int read_cnt;
	char vid[9] , pid[9] , rev[5] ;
    uint8_t cdb_len;
	int i,  size;
	static uint32_t tag = 1;
	uint8_t *buffer = (uint8_t *)kmalloc(64*sizeof(uint8_t), GFP_KERNEL); 
	struct command_block_wrapper *cbw;
    cbw = (struct command_block_wrapper *)kmalloc(sizeof(struct command_block_wrapper), GFP_KERNEL);
	//cbw = (struct command_block_wrapper *)kcalloc(0, sizeof(struct command_block_wrapper), GFP_KERNEL);
	//uint8_t *cdb = (uint8_t *)kmalloc(16*sizeof(uint8_t), GFP_KERNEL);
	for(i=0; i<64; i++)
	{
		*(buffer+i) = 0;
	}
	printk(KERN_INFO "Sending Inquiry:\n");
	//printk(KERN_INFO "Reading capacity:\n");
	cdb_len = cdb_length[cdb[0]];
	printk(KERN_INFO "cdb_len = %d\n", cdb_len);
    memset(cbw, 0, sizeof(struct command_block_wrapper));
	cbw->dCBWSignature[0] = 'U';
	cbw->dCBWSignature[1] = 'S';
	cbw->dCBWSignature[2] = 'B';
	cbw->dCBWSignature[3] = 'C';
	cbw->dCBWTag = tag++;
	cbw->dCBWDataTransferLength = READ_CAPACITY_LENGTH;
	cbw->bmCBWFlags = 0x80;
	cbw->bCBWLUN = 0;
	
	cbw->bCBWCBLength = cdb_len;
	/*for(i = 0; i<16; i++)
	{
		cbw->CBWCB[i] = 0;
	}*/
	memcpy(cbw->CBWCB, cdb, cdb_len);
	/*printk(KERN_INFO "cdb[0] = %d\n", cdb[0]);
	printk(KERN_INFO "cbw->CBWCB[0] = %d\n", cbw->CBWCB[0]);
	printk(KERN_INFO "cdb[4] = %d\n", cdb[4]);
	printk(KERN_INFO "cbw->CBWCB[4] = %d\n", cbw->CBWCB[4]);
	printk(KERN_INFO "cbw->CBWCB[15] = %d\n", cbw->CBWCB[15]);*/
	for(i = 0; i<16; i++)
	{
		printk(KERN_INFO "cbw->CBWCB[%d] = %d\n", i, cbw->CBWCB[i]);
	}
	i = 0;
	do {
	retval = usb_bulk_msg(udev, usb_sndbulkpipe(udev, BULK_EP_OUT), (void *)cbw, 31, &read_cnt, 1000);
    if (retval)
    {
        printk(KERN_ERR "Bulk message sent %d\n", retval);
        //return retval;
    }
	printk(KERN_INFO "read count = %d\n", read_cnt);
	i++;
	} while ((retval < 0) && (i<RETRY_MAX));


	ret = usb_bulk_msg(udev, usb_rcvbulkpipe(udev, 0x81), (void *)buffer, READ_CAPACITY_LENGTH , &size, 1000);
    if (ret)
    {
        printk(KERN_ERR "Bulk message returned %d\n", ret);
		ret = usb_clear_halt(udev, usb_rcvbulkpipe(udev, BULK_EP_IN));
        //return retval;
    }
	if (ret)
    {
        printk(KERN_ERR "clear halt %d\n", ret);
 	//ret = usb_clear_halt(udev, usb_rcvbulkpipe(udev, BULK_EP_IN));
        //return retval;
    }
	
	printk(KERN_INFO "received %d bytes\n", size);
	for(i = 0;i < 36; i++)
	{	
	printk(KERN_INFO "Buffer[%d] = %c",i, buffer[i]);
	}	
	// The following strings are not zero terminated
	/*for (i=0; i<8; i++) {
		vid[i] = buffer[8+i];
		pid[i] = buffer[16+i];
		rev[i/2] = buffer[32+i/2];	// instead of another loop
	}
	vid[8] = 0;
	pid[8] = 0;
	rev[4] = 0;
	printk(KERN_INFO "   VID:PID:REV \"%8s\":\"%8s\":\"%4s\"\n", vid, pid, rev);*/
	/*printk(KERN_INFO "%8s\n", vid);
	printk(KERN_INFO "%8s\n", pid);
	printk(KERN_INFO "%8s\n", rev);*/

	//max_lba = be_to_int32(&buffer[0]);
	//block_size = be_to_int32(&buffer[4]);
	//device_size = ((double)(max_lba+1))*block_size/(1024*1024*1024);
	//printk(KERN_INFO " Max LBA: %08X, Block Size: %08X\n", max_lba, block_size);
	return 0;
	
}

static int usbdev_probe(struct usb_interface *interface, const struct usb_device_id *id)
{
	unsigned char epAddr, epAttr;
	int i;
	uint8_t cdb[16];
	int ret;
	//uint8_t buffer[64];
	
	//struct usb_host_interface *if_desc;

	struct usb_device *udev = interface_to_usbdev(interface);
	struct usb_endpoint_descriptor *ep_desc;
	
	
	/*for(i=0; i<16; i++)
	{
		*(cdb+i) = 0;
	}*/
	if(id->idProduct == HPPD_PID)
	{
		printk(KERN_INFO "USB storage device Plugged in\n");
	}
	else if(id->idVendor == HPPD_VID)
	{
		printk(KERN_INFO "USB storage device Plugged in\n");
	}
	

	//if_desc = interface->cur_altsetting;
	printk(KERN_INFO "No. of Altsettings = %d\n",interface->num_altsetting);

	printk(KERN_INFO "No. of Endpoints = %d\n", interface->cur_altsetting->desc.bNumEndpoints);

	for(i=0;i<interface->cur_altsetting->desc.bNumEndpoints;i++)
	{
		ep_desc = &interface->cur_altsetting->endpoint[i].desc;
		epAddr = ep_desc->bEndpointAddress;
		epAttr = ep_desc->bmAttributes;
	
		if((epAttr & USB_ENDPOINT_XFERTYPE_MASK)==USB_ENDPOINT_XFER_BULK)
		{
			if(epAddr & 0x80)
				printk(KERN_INFO "EP %d is Bulk IN addr  %d\n", i, epAddr);
			else
				printk(KERN_INFO "EP %d is Bulk OUT addr %d\n", i, epAddr);
	
		}

	}
    
	/*retval = usb_control_msg(udev, usb_rcvctrlpipe(udev, 0), 0xFE, 0xA1, 0x0, 0x0, lun, sizeof(char), 5000);

	if (retval < 0)
	{
		printk(KERN_INFO "control message error (%d)\n", retval);
	}
	else
	{
		printk(KERN_INFO "MAX LUN = %d\n", lun);
	}*/
	//memset(buffer, 0, sizeof(buffer));
	memset(cdb, 0, sizeof(cdb));
	
	//item[h] = (char*)calloc(20,sizeof(char));
   //	cdb[0] = (uint8_t *)kcalloc(0x12, sizeof(uint8_t), GFP_KERNEL);
	//cdb[4] = (uint8_t *)kcalloc(INQUIRY_LENGTH, sizeof(uint8_t), GFP_KERNEL);
	cdb[0] = 0x25;	// Inquiry
	//cdb[4] = INQUIRY_LENGTH;
	ret = cbw_bulk_message(udev, cdb);
    if(ret != 0)
	{
		printk(KERN_INFO "cbw unsuccesfull\n");
	}
	//this line causing error
	//p_usbdev_info->class = interface->cur_altsetting->desc.bInterfaceClass;
	printk(KERN_INFO "USB DEVICE CLASS : %x", interface->cur_altsetting->desc.bInterfaceClass);
	printk(KERN_INFO "USB DEVICE SUB CLASS : %x", interface->cur_altsetting->desc.bInterfaceSubClass);
	printk(KERN_INFO "USB DEVICE Protocol : %x", interface->cur_altsetting->desc.bInterfaceProtocol);

return 0;
}



/*Operations structure*/
static struct usb_driver usbdev_driver = {
	name: "hp_device",  //name of the device
	probe: usbdev_probe, // Whenever Device is plugged in
	disconnect: usbdev_disconnect, // When we remove a device
	id_table: usbdev_table, //  List of devices served by this driver
};


int device_init(void)
{
	usb_register(&usbdev_driver);
	return 0;
}

void device_exit(void)
{
	usb_deregister(&usbdev_driver);
	printk(KERN_NOTICE "Leaving Kernel\n");
	//return 0;
}

module_init(device_init);
module_exit(device_exit);
MODULE_AUTHOR("APURVA SINGH <h20190116@pilani.bits-pilani.ac.in>");
MODULE_DESCRIPTION("USB Device Driver");



