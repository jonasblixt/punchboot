#ifndef __ATF_H__
#define __ATF_H__

#include <stdint.h>


#define PARAM_EP		0x01
#define PARAM_IMAGE_BINARY	0x02
#define PARAM_BL31		0x03
#define PARAM_BL_LOAD_INFO	0x04
#define PARAM_BL_PARAMS		0x05
#define PARAM_PSCI_LIB_ARGS	0x06


struct atf_param_header 
{
	uint8_t type;		/* type of the structure */
	uint8_t version;    /* version of this structure */
	uint16_t size;      /* size of this structure in bytes */
	uint32_t attr;      /* attributes: unused bits SBZ */
};

struct atf_image_info 
{
	struct atf_param_header h;
	uintptr_t image_base;   /* physical address of base of image */
	uint32_t image_size;    /* bytes read from image file */
};

struct aapcs64_params 
{
	uint64_t arg0;
	uint64_t arg1;
	uint64_t arg2;
	uint64_t arg3;
	uint64_t arg4;
	uint64_t arg5;
	uint64_t arg6;
	uint64_t arg7;
};

struct entry_point_info 
{
	struct atf_param_header h;
	uintptr_t pc;
	uint32_t spsr;
	struct aapcs64_params args;
};


struct atf_bl31_params 
{
	struct atf_param_header h;
	struct atf_image_info *bl31_image_info;
	struct entry_point_info *bl32_ep_info;
	struct atf_image_info *bl32_image_info;
	struct entry_point_info *bl33_ep_info;
	struct atf_image_info *bl33_image_info;
};


#endif
