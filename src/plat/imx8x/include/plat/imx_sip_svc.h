/*
 * Copyright (c) 2015-2019, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __IMX_SIP_SVC_H__
#define __IMX_SIP_SVC_H__

/* SMC function IDs for SiP Service queries */
#define IMX_SIP_GPC			0xC2000000

#define IMX_SIP_CPUFREQ			0xC2000001
#define IMX_SIP_SET_CPUFREQ		0x00

#define IMX_SIP_SRTC			0xC2000002
#define IMX_SIP_SRTC_SET_TIME		0x00
#define IMX_SIP_SRTC_START_WDOG		0x01
#define IMX_SIP_SRTC_STOP_WDOG		0x02
#define IMX_SIP_SRTC_SET_WDOG_ACT	0x03
#define IMX_SIP_SRTC_PING_WDOG		0x04
#define IMX_SIP_SRTC_SET_TIMEOUT_WDOG	0x05
#define IMX_SIP_SRTC_GET_WDOG_STAT	0x06
#define IMX_SIP_SRTC_SET_PRETIME_WDOG	0x07

#define IMX_SIP_BUILDINFO			0xC2000003
#define IMX_SIP_BUILDINFO_GET_COMMITHASH	0x00

#define IMX_SIP_SRC			0xc2000005
#define IMX_SIP_SRC_M4_START		0x00
#define IMX_SIP_SRC_M4_STARTED		0x01
#define IMX_SIP_SRC_M4_STOP		0x02

#define IMX_SIP_GET_SOC_INFO		0xC2000006

#define IMX_SIP_HAB            0xc2000007
#define IMX_SIP_HAB_AUTHENTICATE	0x00
#define IMX_SIP_HAB_ENTRY			0x01
#define IMX_SIP_HAB_EXIT			0x02
#define IMX_SIP_HAB_REPORT_EVENT	0x03
#define IMX_SIP_HAB_REPORT_STATUS	0x04
#define IMX_SIP_HAB_FAILSAFE		0x05
#define IMX_SIP_HAB_CHECK_TARGET	0x06

#define IMX_SIP_NOC			0xc2000008
#define IMX_SIP_NOC_LCDIF		0x0
#define IMX_SIP_NOC_PRIORITY		0x1
#define NOC_GPU_PRIORITY		0x10
#define NOC_DCSS_PRIORITY		0x11
#define NOC_VPU_PRIORITY		0x12
#define NOC_CPU_PRIORITY		0x13
#define NOC_MIX_PRIORITY		0x14

#define IMX_SIP_WAKEUP_SRC		0xC2000009
#define IMX_SIP_WAKEUP_SRC_SCU		0x1
#define IMX_SIP_WAKEUP_SRC_IRQSTEER	0x2

#define IMX_SIP_OTP_READ		0xC200000A
#define IMX_SIP_OTP_WRITE		0xC200000B

#define IMX_SIP_MISC_SET_TEMP		0xC200000C

#define IMX_SIP_DDR_DVFS		0xc2000004
#if defined(PLAT_imx8mq)
int imx_soc_info_handler(uint32_t smc_fid, u_register_t x1,
			 u_register_t x2, u_register_t x3);
int imx_gpc_handler(uint32_t smc_fid, u_register_t x1,
		    u_register_t x2, u_register_t x3);
int imx_src_handler(uint32_t smc_fid, u_register_t x1,
		    u_register_t x2, u_register_t x3, void *handle);
int dram_dvfs_handler(uint32_t smc_fid, void *handle,
	u_register_t x1, u_register_t x2, u_register_t x3);

int imx_hab_handler(uint32_t smc_fid, u_register_t x1,
	u_register_t x2, u_register_t x3, u_register_t x4);
int imx_noc_handler(uint32_t smc_fid, u_register_t x1,
	u_register_t x2, u_register_t x3);
#endif
#if defined(PLAT_imx8mm) || defined(PLAT_imx8mn) || defined(PLAT_imx8mp)
int dram_dvfs_handler(uint32_t smc_fid, void *handle,
	u_register_t x1, u_register_t x2, u_register_t x3);

int imx_gpc_handler(uint32_t smc_fid, u_register_t x1,
		    u_register_t x2, u_register_t x3);
int imx_src_handler(uint32_t smc_fid, u_register_t x1,
		    u_register_t x2, u_register_t x3, void *handle);
int imx_hab_handler(uint32_t smc_fid, u_register_t x1,
	u_register_t x2, u_register_t x3, u_register_t x4);
#endif

#if (defined(PLAT_imx8qm) || defined(PLAT_imx8qx) || defined(PLAT_imx8dx) || defined(PLAT_imx8dxl))
int imx_cpufreq_handler(uint32_t smc_fid, u_register_t x1,
			u_register_t x2, u_register_t x3);
int imx_srtc_handler(uint32_t smc_fid, void *handle, u_register_t x1,
		     u_register_t x2, u_register_t x3, u_register_t x4);
int imx_wakeup_src_handler(uint32_t smc_fid, u_register_t x1,
			   u_register_t x2, u_register_t x3);
int imx_otp_handler(uint32_t smc_fid, void *handle,
		    u_register_t x1, u_register_t x2);
int imx_misc_set_temp_handler(uint32_t smc_fid, u_register_t x1,
			      u_register_t x2, u_register_t x3,
			      u_register_t x4);
int imx_get_cpu_rev(uint32_t *cpu_id, uint32_t *cpu_rev);
#endif
uint64_t imx_buildinfo_handler(uint32_t smc_fid, u_register_t x1,
			       u_register_t x2, u_register_t x3,
			       u_register_t x4);

#endif /* __IMX_SIP_SVC_H__ */
