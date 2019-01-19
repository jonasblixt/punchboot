#include <pb.h>
#include <io.h>
#include <plat/imx/hab.h>
#include <plat/imx/ocotp.h>
#include <stdbool.h>
#include <tinyprintf.h>

#define IS_HAB_ENABLED_BIT 0x02
static uint8_t _event_data[128];




#define MAX_RECORD_BYTES     (8*1024) /* 4 kbytes */

struct record 
{
	uint8_t  tag;						/* Tag */
	uint8_t  len[2];					/* Length */
	uint8_t  par;						/* Version */
	uint8_t  contents[MAX_RECORD_BYTES];/* Record Data */
	bool	 any_rec_flag;
};

static char *rsn_str[] = 
{
  "RSN = HAB_RSN_ANY (0x00)\n",
  "RSN = HAB_ENG_FAIL (0x30)\n",
  "RSN = HAB_INV_ADDRESS (0x22)\n",
  "RSN = HAB_INV_ASSERTION (0x0C)\n",
  "RSN = HAB_INV_CALL (0x28)\n",
  "RSN = HAB_INV_CERTIFICATE (0x21)\n",
  "RSN = HAB_INV_COMMAND (0x06)\n",
  "RSN = HAB_INV_CSF (0x11)\n",
  "RSN = HAB_INV_DCD (0x27)\n",
  "RSN = HAB_INV_INDEX (0x0F)\n",
  "RSN = HAB_INV_IVT (0x05)\n",
  "RSN = HAB_INV_KEY (0x1D)\n",
  "RSN = HAB_INV_RETURN (0x1E)\n",
  "RSN = HAB_INV_SIGNATURE (0x18)\n",
  "RSN = HAB_INV_SIZE (0x17)\n",
  "RSN = HAB_MEM_FAIL (0x2E)\n",
  "RSN = HAB_OVR_COUNT (0x2B)\n",
  "RSN = HAB_OVR_STORAGE (0x2D)\n",
  "RSN = HAB_UNS_ALGORITHM (0x12)\n",
  "RSN = HAB_UNS_COMMAND (0x03)\n",
  "RSN = HAB_UNS_ENGINE (0x0A)\n",
  "RSN = HAB_UNS_ITEM (0x24)\n",
  "RSN = HAB_UNS_KEY (0x1B)\n",
  "RSN = HAB_UNS_PROTOCOL (0x14)\n",
  "RSN = HAB_UNS_STATE (0x09)\n",
  "RSN = INVALID\n",
  NULL
};

static char *sts_str[] = 
{
  "STS = HAB_SUCCESS (0xF0)\n",
  "STS = HAB_FAILURE (0x33)\n",
  "STS = HAB_WARNING (0x69)\n",
  "STS = INVALID\n",
  NULL
};

static char *eng_str[] = 
{
  "ENG = HAB_ENG_ANY (0x00)\n",
  "ENG = HAB_ENG_SCC (0x03)\n",
  "ENG = HAB_ENG_RTIC (0x05)\n",
  "ENG = HAB_ENG_SAHARA (0x06)\n",
  "ENG = HAB_ENG_CSU (0x0A)\n",
  "ENG = HAB_ENG_SRTC (0x0C)\n",
  "ENG = HAB_ENG_DCP (0x1B)\n",
  "ENG = HAB_ENG_CAAM (0x1D)\n",
  "ENG = HAB_ENG_SNVS (0x1E)\n",
  "ENG = HAB_ENG_OCOTP (0x21)\n",
  "ENG = HAB_ENG_DTCP (0x22)\n",
  "ENG = HAB_ENG_ROM (0x36)\n",
  "ENG = HAB_ENG_HDCP (0x24)\n",
  "ENG = HAB_ENG_RTL (0x77)\n",
  "ENG = HAB_ENG_SW (0xFF)\n",
  "ENG = INVALID\n",
  NULL
};

static char *ctx_str[] = 
{
  "CTX = HAB_CTX_ANY(0x00)\n",
  "CTX = HAB_CTX_FAB (0xFF)\n",
  "CTX = HAB_CTX_ENTRY (0xE1)\n",
  "CTX = HAB_CTX_TARGET (0x33)\n",
  "CTX = HAB_CTX_AUTHENTICATE (0x0A)\n",
  "CTX = HAB_CTX_DCD (0xDD)\n",
  "CTX = HAB_CTX_CSF (0xCF)\n",
  "CTX = HAB_CTX_COMMAND (0xC0)\n",
  "CTX = HAB_CTX_AUT_DAT (0xDB)\n",
  "CTX = HAB_CTX_ASSERT (0xA0)\n",
  "CTX = HAB_CTX_EXIT (0xEE)\n",
  "CTX = INVALID\n",
  NULL
};

static uint8_t hab_statuses[5] = 
{
	HAB_STS_ANY,
	HAB_FAILURE,
	HAB_WARNING,
	HAB_SUCCESS,
	-1
};

static uint8_t hab_reasons[26] = 
{
	HAB_RSN_ANY,
	HAB_ENG_FAIL,
	HAB_INV_ADDRESS,
	HAB_INV_ASSERTION,
	HAB_INV_CALL,
	HAB_INV_CERTIFICATE,
	HAB_INV_COMMAND,
	HAB_INV_CSF,
	HAB_INV_DCD,
	HAB_INV_INDEX,
	HAB_INV_IVT,
	HAB_INV_KEY,
	HAB_INV_RETURN,
	HAB_INV_SIGNATURE,
	HAB_INV_SIZE,
	HAB_MEM_FAIL,
	HAB_OVR_COUNT,
	HAB_OVR_STORAGE,
	HAB_UNS_ALGORITHM,
	HAB_UNS_COMMAND,
	HAB_UNS_ENGINE,
	HAB_UNS_ITEM,
	HAB_UNS_KEY,
	HAB_UNS_PROTOCOL,
	HAB_UNS_STATE,
	-1
};

static uint8_t hab_contexts[12] = 
{
	HAB_CTX_ANY,
	HAB_CTX_FAB,
	HAB_CTX_ENTRY,
	HAB_CTX_TARGET,
	HAB_CTX_AUTHENTICATE,
	HAB_CTX_DCD,
	HAB_CTX_CSF,
	HAB_CTX_COMMAND,
	HAB_CTX_AUT_DAT,
	HAB_CTX_ASSERT,
	HAB_CTX_EXIT,
	-1
};

static uint8_t hab_engines[16] = 
{
	HAB_ENG_ANY,
	HAB_ENG_SCC,
	HAB_ENG_RTIC,
	HAB_ENG_SAHARA,
	HAB_ENG_CSU,
	HAB_ENG_SRTC,
	HAB_ENG_DCP,
	HAB_ENG_CAAM,
	HAB_ENG_SNVS,
	HAB_ENG_OCOTP,
	HAB_ENG_DTCP,
	HAB_ENG_ROM,
	HAB_ENG_HDCP,
	HAB_ENG_RTL,
	HAB_ENG_SW,
	-1
};

static inline uint8_t get_idx(uint8_t *list, uint8_t tgt)
{
	uint8_t idx = 0;
	int8_t element = list[idx];

	while (element != -1) 
    {
		if (element == tgt)
			return idx;
		element = list[++idx];
	}
	return -1;
}

bool hab_secureboot_active(void)
{
	uint32_t reg;
	int ret;

	ret = ocotp_read(1, 6, &reg);

	if (ret != PB_OK) 
    {
		LOG_ERR("Secure boot fuse read error");
		return ret;
	}

	return (reg & IS_HAB_ENABLED_BIT) == IS_HAB_ENABLED_BIT;
}

int hab_has_no_errors(void)
{
	uint32_t index = 0;
	size_t bytes = sizeof(_event_data);
    uint32_t result;
	enum hab_config config = 0;
	enum hab_state state = 0;
	uint32_t i;
	struct record *rec = (struct record *)_event_data;
	hab_rvt_report_event_t *hab_rvt_report_event;
	hab_rvt_report_status_t *hab_rvt_report_status;

	hab_rvt_report_event = (hab_rvt_report_event_t *)HAB_RVT_REPORT_EVENT;
	hab_rvt_report_status = (hab_rvt_report_status_t *)HAB_RVT_REPORT_STATUS;

    result = hab_rvt_report_status(&config, &state);
	LOG_INFO("configuration: 0x%02x, state: 0x%02x", config, state);

    while (hab_rvt_report_event(HAB_FAILURE, index, _event_data, &bytes) 
                                            == HAB_SUCCESS) 
    {
        LOG_ERR("Error %lu, event data:", index+1);

        for (i = 0; i < bytes; i++) 
        {
            if (i == 0)
                tfp_printf("\t0x%02x", _event_data[i]);
            else if ((i % 8) == 0)
                tfp_printf("\n\r\t0x%02x", _event_data[i]);
            else
                tfp_printf(" 0x%02x", _event_data[i]);
        }

        tfp_printf("\n\r%s\r", sts_str[get_idx(hab_statuses, rec->contents[0])]);
        tfp_printf("%s\r", rsn_str[get_idx(hab_reasons, rec->contents[1])]);
        tfp_printf("%s\r", ctx_str[get_idx(hab_contexts, rec->contents[2])]);
        tfp_printf("%s\r", eng_str[get_idx(hab_engines, rec->contents[3])]);

        bytes = sizeof(_event_data);
        index++;
    }

	return (result == HAB_SUCCESS)?PB_OK:PB_ERR;
}

