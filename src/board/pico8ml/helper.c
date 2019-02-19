#include <stdio.h>
#include <pb.h>
#include <io.h>
#include <board/pico8ml/ddr.h>
#include <board/pico8ml/ddr_memory_map.h>

#define IMEM_LEN 32768//23400	//byte
#define DMEM_LEN 16384//1720	//byte
#define IMEM_2D_OFFSET 	49152

#define IMEM_OFFSET_ADDR 0x00050000
#define DMEM_OFFSET_ADDR 0x00054000
#define DDR_TRAIN_CODE_BASE_ADDR IP2APB_DDRPHY_IPS_BASE_ADDR(0)

extern uint32_t _data_region_end;
extern uint32_t  _binary_lpddr4_pmu_train_1d_imem_bin_start;
extern uint32_t  _binary_lpddr4_pmu_train_1d_dmem_bin_start;
extern uint32_t  _binary_lpddr4_pmu_train_2d_imem_bin_start;
extern uint32_t  _binary_lpddr4_pmu_train_2d_dmem_bin_start;

extern uint32_t  _binary_lpddr4_pmu_train_1d_imem_bin_end;
extern uint32_t  _binary_lpddr4_pmu_train_1d_dmem_bin_end;
extern uint32_t  _binary_lpddr4_pmu_train_2d_imem_bin_end;
extern uint32_t  _binary_lpddr4_pmu_train_2d_dmem_bin_end;

void ddr_load_train_code(enum fw_type type)
{
	uint32_t tmp32, i;
	uint32_t error = 0;
	unsigned long pr_to32, pr_from32;

	unsigned long imem_start = 0;
	unsigned long dmem_start = 0;
    unsigned long imem_sz = 0;
    unsigned long dmem_sz = 0;

    if (type == FW_1D_IMAGE)
    {
        imem_start = (unsigned long) &_binary_lpddr4_pmu_train_1d_imem_bin_start;
        dmem_start = (unsigned long) &_binary_lpddr4_pmu_train_1d_dmem_bin_start;
        imem_sz = (unsigned long) &_binary_lpddr4_pmu_train_1d_imem_bin_end;
        imem_sz -= (unsigned long)&_binary_lpddr4_pmu_train_1d_imem_bin_start;
        dmem_sz = (unsigned long)&_binary_lpddr4_pmu_train_1d_dmem_bin_end;
        dmem_sz -= (unsigned long)&_binary_lpddr4_pmu_train_1d_dmem_bin_start;
    } else {
        imem_start = (unsigned long)&_binary_lpddr4_pmu_train_2d_imem_bin_start;
        dmem_start = (unsigned long)&_binary_lpddr4_pmu_train_2d_dmem_bin_start;
        imem_sz = (unsigned long)&_binary_lpddr4_pmu_train_2d_imem_bin_end;
        imem_sz -= (unsigned long)&_binary_lpddr4_pmu_train_2d_imem_bin_start;
        dmem_sz = (unsigned long)&_binary_lpddr4_pmu_train_2d_dmem_bin_end;
        dmem_sz -= (unsigned long)&_binary_lpddr4_pmu_train_2d_dmem_bin_start;
    }

    LOG_INFO("Loading imem image from %lx, sz %lu bytes", imem_start, imem_sz);

	pr_from32 = imem_start;
	pr_to32 = DDR_TRAIN_CODE_BASE_ADDR + 4 * IMEM_OFFSET_ADDR;

	for(i = 0x0; i < imem_sz; )
    {
		tmp32 = pb_read32(pr_from32);
		pb_write16(tmp32 & 0x0000ffff, pr_to32);
		pr_to32 += 4;
		pb_write16((tmp32 >> 16) & 0x0000ffff, pr_to32);
		pr_to32 += 4;
		pr_from32 += 4;
		i += 4;
	}

	pr_from32 = dmem_start;
	pr_to32 = DDR_TRAIN_CODE_BASE_ADDR + 4 * DMEM_OFFSET_ADDR;

	for(i = 0x0; i < dmem_sz;)
    {
		tmp32 = pb_read32(pr_from32);
		pb_write16(tmp32 & 0x0000ffff, pr_to32);
		pr_to32 += 4;
		pb_write16((tmp32 >> 16) & 0x0000ffff, pr_to32);
		pr_to32 += 4;
		pr_from32 += 4;
		i += 4;
	}

	LOG_INFO("check ddr4_pmu_train_imem code");
	pr_from32 = imem_start;
	pr_to32 = DDR_TRAIN_CODE_BASE_ADDR + 4 * IMEM_OFFSET_ADDR;

	for(i = 0x0; i < imem_sz;)
    {
		tmp32 = (pb_read16(pr_to32) & 0x0000ffff);
		pr_to32 += 4;
		tmp32 += ((pb_read16(pr_to32) & 0x0000ffff) << 16);

		if(tmp32 != pb_read32(pr_from32))
        {
			LOG_ERR("%lx %lx", pr_from32, pr_to32);
			error++;
		}
		pr_from32 += 4;
		pr_to32 += 4;
		i += 4;
	}

	if(error)
		LOG_ERR("check ddr4_pmu_train_imem code fail=%d",error);
	else
		LOG_INFO("check ddr4_pmu_train_imem code pass");

	LOG_INFO("check ddr4_pmu_train_dmem code");
	pr_from32 = dmem_start;
	pr_to32 = DDR_TRAIN_CODE_BASE_ADDR + 4 * DMEM_OFFSET_ADDR;

	for(i = 0x0; i < dmem_sz;)
    {
		tmp32 = (pb_read16(pr_to32) & 0x0000ffff);
		pr_to32 += 4;
		tmp32 += ((pb_read16(pr_to32) & 0x0000ffff) << 16);

		if(tmp32 != pb_read32(pr_from32))
        {
			LOG_ERR("%lx %lx", pr_from32, pr_to32);
			error++;
		}

		pr_from32 += 4;
		pr_to32 += 4;
		i += 4;
	}

	if(error)
		LOG_ERR("check ddr4_pmu_train_dmem code fail=%d",error);
	else
		LOG_INFO("check ddr4_pmu_train_dmem code pass");

}
