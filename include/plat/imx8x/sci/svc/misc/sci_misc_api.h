/*
 * Copyright (C) 2016 Freescale Semiconductor, Inc.
 * Copyright 2017-2019 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*!
 * Header file containing the public API for the System Controller (SC)
 * Miscellaneous (MISC) function.
 *
 * @addtogroup MISC_SVC MISC: Miscellaneous Service
 *
 * Module for the Miscellaneous (MISC) service.
 *
 * @{
 */

#ifndef SC_MISC_API_H
#define SC_MISC_API_H

/* Includes */

#include <plat/imx8x/sci/sci_types.h>
#include <plat/imx8x/sci/svc/rm/sci_rm_api.h>

/* Defines */

/*!
 * @name Defines for type widths
 */
/*@{*/
#define SC_MISC_DMA_GRP_W       5U	/* Width of sc_misc_dma_group_t */
/*@}*/

/*! Max DMA channel priority group */
#define SC_MISC_DMA_GRP_MAX     31U

/*!
 * @name Defines for sc_misc_boot_status_t
 */
/*@{*/
#define SC_MISC_BOOT_STATUS_SUCCESS     0U	/* Success */
#define SC_MISC_BOOT_STATUS_SECURITY    1U	/* Security violation */
/*@}*/

/*!
 * @name Defines for sc_misc_temp_t
 */
/*@{*/
#define SC_MISC_TEMP                    0U	/* Temp sensor */
#define SC_MISC_TEMP_HIGH               1U	/* Temp high alarm */
#define SC_MISC_TEMP_LOW                2U	/* Temp low alarm */
/*@}*/

/*!
 * @name Defines for sc_misc_bt_t
 */
/*@{*/
#define SC_MISC_BT_PRIMARY              0U	/* Primary boot */
#define SC_MISC_BT_SECONDARY            1U	/* Secondary boot */
#define SC_MISC_BT_RECOVERY             2U	/* Recovery boot */
#define SC_MISC_BT_MANUFACTURE          3U	/* Manufacture boot */
#define SC_MISC_BT_SERIAL               4U	/* Serial boot */
/*@}*/

/* Types */

/*!
 * This type is used to store a DMA channel priority group.
 */
typedef uint8_t sc_misc_dma_group_t;

/*!
 * This type is used report boot status.
 */
typedef uint8_t sc_misc_boot_status_t;

/*!
 * This type is used report boot status.
 */
typedef uint8_t sc_misc_temp_t;

/*!
 * This type is used report the boot type.
 */
typedef uint8_t sc_misc_bt_t;

/* Functions */

/*!
 * @name Control Functions
 * @{
 */

/*!
 * This function sets a miscellaneous control value.
 *
 * @param[in]     ipc         IPC handle
 * @param[in]     resource    resource the control is associated with
 * @param[in]     ctrl        control to change
 * @param[in]     val         value to apply to the control
 *
 * @return Returns an error code (SC_ERR_NONE = success).
 *
 * Return errors:
 * - SC_PARM if arguments out of range or invalid,
 * - SC_ERR_NOACCESS if caller's partition is not the resource owner or parent
 *   of the owner
 *
 * Refer to the [Control List](@ref CONTROLS) for valid control values.
 */
sc_err_t sc_misc_set_control(sc_ipc_t ipc, sc_rsrc_t resource,
			     sc_ctrl_t ctrl, uint32_t val);

/*!
 * This function gets a miscellaneous control value.
 *
 * @param[in]     ipc         IPC handle
 * @param[in]     resource    resource the control is associated with
 * @param[in]     ctrl        control to get
 * @param[out]    val         pointer to return the control value
 *
 * @return Returns an error code (SC_ERR_NONE = success).
 *
 * Return errors:
 * - SC_PARM if arguments out of range or invalid,
 * - SC_ERR_NOACCESS if caller's partition is not the resource owner or parent
 *   of the owner
 *
 * Refer to the [Control List](@ref CONTROLS) for valid control values.
 */
sc_err_t sc_misc_get_control(sc_ipc_t ipc, sc_rsrc_t resource,
			     sc_ctrl_t ctrl, uint32_t *val);

/* @} */

/*!
 * @name DMA Functions
 * @{
 */

/*!
 * This function configures the max DMA channel priority group for a
 * partition.
 *
 * @param[in]     ipc         IPC handle
 * @param[in]     pt          handle of partition to assign \a max
 * @param[in]     max         max priority group (0-31)
 *
 * @return Returns an error code (SC_ERR_NONE = success).
 *
 * Return errors:
 * - SC_PARM if arguments out of range or invalid,
 * - SC_ERR_NOACCESS if caller's partition is not the parent
 *   of the affected partition
 *
 * Valid \a max range is 0-31 with 0 being the lowest and 31 the highest.
 * Default is the max priority group for the parent partition of \a pt.
 */
sc_err_t sc_misc_set_max_dma_group(sc_ipc_t ipc, sc_rm_pt_t pt,
				   sc_misc_dma_group_t max);

/*!
 * This function configures the priority group for a DMA channel.
 *
 * @param[in]     ipc         IPC handle
 * @param[in]     resource    DMA channel resource
 * @param[in]     group       priority group (0-31)
 *
 * @return Returns an error code (SC_ERR_NONE = success).
 *
 * Return errors:
 * - SC_PARM if arguments out of range or invalid,
 * - SC_ERR_NOACCESS if caller's partition is not the owner or parent
 *   of the owner of the DMA channel
 *
 * Valid \a group range is 0-31 with 0 being the lowest and 31 the highest.
 * The max value of \a group is limited by the partition max set using
 * sc_misc_set_max_dma_group().
 */
sc_err_t sc_misc_set_dma_group(sc_ipc_t ipc, sc_rsrc_t resource,
			       sc_misc_dma_group_t group);

/* @} */

/*!
 * @name Debug Functions
 * @{
 */

/*!
 * This function is used output a debug character from the SCU UART.
 *
 * @param[in]     ipc         IPC handle
 * @param[in]     ch          character to output
 */
void sc_misc_debug_out(sc_ipc_t ipc, uint8_t ch);

/*!
 * This function starts/stops emulation waveform capture.
 *
 * @param[in]     ipc         IPC handle
 * @param[in]     enable      flag to enable/disable capture
 *
 * @return Returns an error code (SC_ERR_NONE = success).
 *
 * Return errors:
 * - SC_ERR_UNAVAILABLE if not running on emulation
 */
sc_err_t sc_misc_waveform_capture(sc_ipc_t ipc, sc_bool_t enable);

/*!
 * This function is used to return the SCFW build info.
 *
 * @param[in]     ipc         IPC handle
 * @param[out]    build       pointer to return build number
 * @param[out]    commit      pointer to return commit ID (git SHA-1)
 */
void sc_misc_build_info(sc_ipc_t ipc, uint32_t *build, uint32_t *commit);

/*!
 * This function is used to return the SCFW API versions.
 *
 * @param[in]     ipc         IPC handle
 * @param[out]    cl_maj      pointer to return major part of client version
 * @param[out]    cl_min      pointer to return minor part of client version
 * @param[out]    sv_maj      pointer to return major part of SCFW version
 * @param[out]    sv_min      pointer to return minor part of SCFW version
 *
 * Client version is the version of the API ported to and used by the caller.
 * SCFW version is the version of the SCFW binary running on the CPU.
 *
 * Note a major version difference indicates a break in compatibility.
 */
void sc_misc_api_ver(sc_ipc_t ipc, uint16_t *cl_maj,
		     uint16_t *cl_min, uint16_t *sv_maj, uint16_t *sv_min);

/*!
 * This function is used to return the device's unique ID.
 *
 * @param[in]     ipc         IPC handle
 * @param[out]    id_l        pointer to return lower 32-bit of ID [31:0]
 * @param[out]    id_h        pointer to return upper 32-bits of ID [63:32]
 */
void sc_misc_unique_id(sc_ipc_t ipc, uint32_t *id_l, uint32_t *id_h);

/* @} */

/*!
 * @name Other Functions
 * @{
 */

/*!
 * This function configures the ARI match value for PCIe/SATA resources.
 *
 * @param[in]     ipc          IPC handle
 * @param[in]     resource     match resource
 * @param[in]     resource_mst PCIe/SATA master to match
 * @param[in]     ari          ARI to match
 * @param[in]     enable       enable match or not
 *
 * @return Returns an error code (SC_ERR_NONE = success).
 *
 * Return errors:
 * - SC_PARM if arguments out of range or invalid,
 * - SC_ERR_NOACCESS if caller's partition is not the owner or parent
 *   of the owner of the resource and translation
 *
 * For PCIe, the ARI is the 16-bit value that includes the bus number,
 * device number, and function number. For SATA, this value includes the
 * FISType and PM_Port.
 */
sc_err_t sc_misc_set_ari(sc_ipc_t ipc, sc_rsrc_t resource,
			 sc_rsrc_t resource_mst, uint16_t ari,
			 sc_bool_t enable);

/*!
 * This function reports boot status.
 *
 * @param[in]     ipc         IPC handle
 * @param[in]     status      boot status
 *
 * This is used by SW partitions to report status of boot. This is
 * normally used to report a boot failure.
 */
void sc_misc_boot_status(sc_ipc_t ipc, sc_misc_boot_status_t status);

/*!
 * This function tells the SCFW that a CPU is done booting.
 *
 * @param[in]     ipc         IPC handle
 * @param[in]     cpu         CPU that is done booting
 *
 * This is called by early booting CPUs to report they are done with
 * initialization. After starting early CPUs, the SCFW halts the
 * booting process until they are done. During this time, early
 * CPUs can call the SCFW with lower latency as the SCFW is idle.
 *
 * @return Returns an error code (SC_ERR_NONE = success).
 *
 * Return errors:
 * - SC_PARM if arguments out of range or invalid,
 * - SC_ERR_NOACCESS if caller's partition is not the CPU owner
 */
sc_err_t sc_misc_boot_done(sc_ipc_t ipc, sc_rsrc_t cpu);

/*!
 * This function reads a given fuse word index.
 *
 * @param[in]     ipc         IPC handle
 * @param[in]     word        fuse word index
 * @param[out]    val         fuse read value
 *
 * @return Returns and error code (SC_ERR_NONE = success).
 *
 * Return errors codes:
 * - SC_ERR_PARM if word fuse index param out of range or invalid
 * - SC_ERR_NOACCESS if read operation failed
 * - SC_ERR_LOCKED if read operation is locked
 */
sc_err_t sc_misc_otp_fuse_read(sc_ipc_t ipc, uint32_t word, uint32_t *val);

/*!
 * This function writes a given fuse word index. Only the owner of the
 * SC_R_SYSTEM resource or a partition with access permissions to
 * SC_R_SYSTEM can do this.
 *
 * @param[in]     ipc         IPC handle
 * @param[in]     word        fuse word index
 * @param[in]     val         fuse write value
 *
 * The command is passed as is to SECO. SECO uses part of the
 * \a word parameter to indicate if the fuse should be locked
 * after programming. See the "Write common fuse" section of
 * the SECO API Reference Guide for more info.
 *
 * @return Returns and error code (SC_ERR_NONE = success).
 *
 * Return errors codes:
 * - SC_ERR_PARM if word fuse index param out of range or invalid
 * - SC_ERR_NOACCESS if caller does not have SC_R_SYSTEM access
 * - SC_ERR_NOACCESS if write operation failed
 * - SC_ERR_LOCKED if write operation is locked
 */
sc_err_t sc_misc_otp_fuse_write(sc_ipc_t ipc, uint32_t word, uint32_t val);

/*!
 * This function sets a temp sensor alarm.
 *
 * @param[in]     ipc         IPC handle
 * @param[in]     resource    resource with sensor
 * @param[in]     temp        alarm to set
 * @param[in]     celsius     whole part of temp to set
 * @param[in]     tenths      fractional part of temp to set
 *
 * @return Returns and error code (SC_ERR_NONE = success).
 *
 * This function will enable the alarm interrupt if the temp requested is
 * not the min/max temp. This enable automatically clears when the alarm
 * occurs and this function has to be called again to re-enable.
 *
 * Return errors codes:
 * - SC_ERR_PARM if parameters invalid
 * - SC_ERR_NOACCESS if caller does not own the resource
 * - SC_ERR_NOPOWER if power domain of resource not powered
 */
sc_err_t sc_misc_set_temp(sc_ipc_t ipc, sc_rsrc_t resource,
			  sc_misc_temp_t temp, int16_t celsius, int8_t tenths);

/*!
 * This function gets a temp sensor value.
 *
 * @param[in]     ipc         IPC handle
 * @param[in]     resource    resource with sensor
 * @param[in]     temp        value to get (sensor or alarm)
 * @param[out]    celsius     whole part of temp to get
 * @param[out]    tenths      fractional part of temp to get
 *
 * @return Returns and error code (SC_ERR_NONE = success).
 *
 * Return errors codes:
 * - SC_ERR_PARM if parameters invalid
 * - SC_ERR_BUSY if temp not ready yet (time delay after power on)
 * - SC_ERR_NOPOWER if power domain of resource not powered
 */
sc_err_t sc_misc_get_temp(sc_ipc_t ipc, sc_rsrc_t resource,
			  sc_misc_temp_t temp, int16_t * celsius,
			  int8_t * tenths);

/*!
 * This function returns the boot device.
 *
 * @param[in]     ipc         IPC handle
 * @param[out]    dev         pointer to return boot device
 */
void sc_misc_get_boot_dev(sc_ipc_t ipc, sc_rsrc_t * dev);

/*!
 * This function returns the boot type.
 *
 * @param[in]     ipc         IPC handle
 * @param[out]    type        pointer to return boot type
 *
 * @return Returns and error code (SC_ERR_NONE = success).
 *
 * Return errors code:
 * - SC_ERR_UNAVAILABLE if type not passed by ROM
 */
sc_err_t sc_misc_get_boot_type(sc_ipc_t ipc, sc_misc_bt_t * type);

/*!
 * This function returns the boot container index.
 *
 * @param[in]     ipc         IPC handle
 * @param[out]    idx         pointer to return index
 *
 * Return \a idx = 1 for first container, 2 for second.
 *
 * @return Returns and error code (SC_ERR_NONE = success).
 *
 * Return errors code:
 * - SC_ERR_UNAVAILABLE if index not passed by ROM
 */
sc_err_t sc_misc_get_boot_container(sc_ipc_t ipc, uint8_t *idx);

/*!
 * This function returns the current status of the ON/OFF button.
 *
 * @param[in]     ipc         IPC handle
 * @param[out]    status      pointer to return button status
 */
void sc_misc_get_button_status(sc_ipc_t ipc, sc_bool_t *status);

/*!
 * This function returns the ROM patch checksum.
 *
 * @param[in]     ipc         IPC handle
 * @param[out]    checksum    pointer to return checksum
 *
 * @return Returns and error code (SC_ERR_NONE = success).
 */
sc_err_t sc_misc_rompatch_checksum(sc_ipc_t ipc, uint32_t *checksum);

/*!
 * This function calls the board IOCTL function.
 *
 * @param[in]     ipc         IPC handle
 * @param[in,out] parm1       pointer to pass parameter 1
 * @param[in,out] parm2       pointer to pass parameter 2
 * @param[in,out] parm3       pointer to pass parameter 3
 *
 * @return Returns and error code (SC_ERR_NONE = success).
 */
sc_err_t sc_misc_board_ioctl(sc_ipc_t ipc, uint32_t *parm1,
			     uint32_t *parm2, uint32_t *parm3);

/* @} */

#endif				/* SC_MISC_API_H */

/**@}*/
