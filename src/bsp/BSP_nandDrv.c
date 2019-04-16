/*
 * BSP_nandFlashDrv.c
 *
 *  Created on: Oct 2, 2018
 *      Author: toan
 */

//*****************************************************************************
// File dependencies.
//*****************************************************************************
#include "stdlib.h"
#include "stdio.h"
#include "FreeRTOS.h"
#include "task.h"
#include "fsl_clock.h"
#include "fsl_common.h"
#include "fsl_iomuxc.h"
#include "fsl_semc.h"
#include "BSP_nandDrv.h"
#include "ubiFsConfig.h"
#include <cr_section_macros.h>

//*****************************************************************************
// Private definitions.
//*****************************************************************************
#define NAND_SEMC                      SEMC
#define NAND_SEMC_CLK_FREQ             CLOCK_GetFreq(kCLOCK_SemcClk)

/* NAND Command Code */
#define NAND_CMD_READ_PAGE_1ST          (0x00)
#define NAND_CMD_READ_PAGE_2ND          (0x30)
#define NAND_CMD_READ_RANDOM_1ST        (0x05)
#define NAND_CMD_READ_RANDOM_2ND        (0xE0)
#define NAND_CMD_READ_ID                (0x90)
#define NAND_CMD_READ_STATUS            (0x70)


#define NAND_CMD_PROGRAM_PAGE_1ST       (0x80)
#define NAND_CMD_PROGRAM_PAGE_2ND       (0x10)
#define NAND_CMD_PROGRAM_SERIAL         (0x80)
#define NAND_CMD_PROGRAM_RANDOM         (0x85)

#define NAND_CMD_COPY_READ_1ST          (0x00)
#define NAND_CMD_COPY_READ_2ND          (0x35)
#define NAND_CMD_COPY_PROGRAM_1ST       (0x85)
#define NAND_CMD_COPY_PROGRAM_2ND       (0x10)

#define NAND_CMD_ERASE_1ST              (0x60)
#define NAND_CMD_ERASE_2ND              (0xD0)

#define NAND_CMD_RESET                  (0xFF)

//*****************************************************************************
// Private member declarations.
//*****************************************************************************
static bool bInit = false;

//AT_NONCACHEABLE_SECTION_INIT(uint8_t nand_readBuf[NAND_PAGE_SIZE_PHYSICAL]) = {0U};
//AT_NONCACHEABLE_SECTION_INIT(uint8_t nand_writeBuf[NAND_PAGE_SIZE_PHYSICAL]) = {0U};

//*****************************************************************************
// Public / Internal member external declarations.
//*****************************************************************************

//*****************************************************************************
// Private function prototypes.
//*****************************************************************************
static void _NAND_InitPins(void);
static status_t _NAND_InitSEMC(void);

//*****************************************************************************
// Public function implementations
//*****************************************************************************

//*****************************************************************************
//!
//! \brief This function initializes the NAND Flash
//!
//! \param  None
//!
//! \return \c ::BSP_NAND_GENERIC_FAIL  Failed in NAND Flash initialization
//! \return \c ::BSP_NAND_SUCCESS       Success in NAND Flash initialization
//!
//*****************************************************************************
BSP_NAND_RET_T BSP_NAND_Init(void)
{
	BSP_NAND_RET_T retval = BSP_NAND_SUCCESS;
#if 0 /* Do not re-initialize SEMC, it will crash when code is running from SDRAM */
	_NAND_InitPins();

    CLOCK_InitSysPfd(kCLOCK_Pfd2, 30);
    /* Set semc clock to 163.86 MHz */
    CLOCK_SetMux(kCLOCK_SemcMux, 1);
    CLOCK_SetDiv(kCLOCK_SemcDiv, 1);
#endif
	if (kStatus_Success != _NAND_InitSEMC())
	{
		retval = BSP_NAND_GENERIC_FAIL;
		return retval;
	}
	bInit = true;
}

//*****************************************************************************
// Private function implementations.
//*****************************************************************************
static void _NAND_InitPins(void)
{
	CLOCK_EnableClock(kCLOCK_Iomuxc);           /* iomuxc clock (iomuxc_clk_enable): 0x03u */

	IOMUXC_SetPinMux(
	  IOMUXC_GPIO_EMC_00_SEMC_DATA00,         /* GPIO_EMC_00 is configured as SEMC_DATA00 */
	  0U);                                    /* Software Input On Field: Input Path is determined by functionality */
	IOMUXC_SetPinMux(
	  IOMUXC_GPIO_EMC_01_SEMC_DATA01,         /* GPIO_EMC_01 is configured as SEMC_DATA01 */
	  0U);                                    /* Software Input On Field: Input Path is determined by functionality */
	IOMUXC_SetPinMux(
	  IOMUXC_GPIO_EMC_02_SEMC_DATA02,         /* GPIO_EMC_02 is configured as SEMC_DATA02 */
	  0U);                                    /* Software Input On Field: Input Path is determined by functionality */
	IOMUXC_SetPinMux(
	  IOMUXC_GPIO_EMC_03_SEMC_DATA03,         /* GPIO_EMC_03 is configured as SEMC_DATA03 */
	  0U);                                    /* Software Input On Field: Input Path is determined by functionality */
	IOMUXC_SetPinMux(
	  IOMUXC_GPIO_EMC_04_SEMC_DATA04,         /* GPIO_EMC_04 is configured as SEMC_DATA04 */
	  0U);                                    /* Software Input On Field: Input Path is determined by functionality */
	IOMUXC_SetPinMux(
	  IOMUXC_GPIO_EMC_05_SEMC_DATA05,         /* GPIO_EMC_05 is configured as SEMC_DATA05 */
	  0U);                                    /* Software Input On Field: Input Path is determined by functionality */
	IOMUXC_SetPinMux(
	  IOMUXC_GPIO_EMC_06_SEMC_DATA06,         /* GPIO_EMC_06 is configured as SEMC_DATA06 */
	  0U);                                    /* Software Input On Field: Input Path is determined by functionality */
	IOMUXC_SetPinMux(
	  IOMUXC_GPIO_EMC_07_SEMC_DATA07,         /* GPIO_EMC_07 is configured as SEMC_DATA07 */
	  0U);									  /* Software Input On Field: Input Path is determined by functionality */
	IOMUXC_SetPinMux(
	  IOMUXC_GPIO_EMC_40_SEMC_RDY,			 /* GPIO_EMC_40 is configured as SEMC_RDY */
	  0U);									 /* Software Input On Field: Input Path is determined by functionality */
	IOMUXC_SetPinMux(
	  IOMUXC_GPIO_EMC_41_SEMC_CSX00,	     /* GPIO_EMC_41 is configured as SEMC_CSX00 */
	  0U);									 /* Software Input On Field: Input Path is determined by functionality */
	IOMUXC_SetPinMux(
	  IOMUXC_GPIO_EMC_19_SEMC_ADDR11,	     /* GPIO_EMC_19 is configured as SEMC_ADDR11 */
	  0U);									 /* Software Input On Field: Input Path is determined by functionality */
	IOMUXC_SetPinMux(
	  IOMUXC_GPIO_EMC_20_SEMC_ADDR12,	     /* GPIO_EMC_19 is configured as SEMC_ADDR12 */
	  0U);									 /* Software Input On Field: Input Path is determined by functionality */
	IOMUXC_SetPinMux(
	  IOMUXC_GPIO_EMC_22_SEMC_BA1,	     	/* GPIO_EMC_22 is configured as SEMC_BA1 */
	  0U);									/* Software Input On Field: Input Path is determined by functionality */
	IOMUXC_SetPinMux(
	  IOMUXC_GPIO_EMC_18_SEMC_ADDR09,	    /* GPIO_EMC_22 is configured as SEMC_ADDR09 */
	  0U);									/* Software Input On Field: Input Path is determined by functionality */

	IOMUXC_SetPinConfig(
	  IOMUXC_GPIO_EMC_00_SEMC_DATA00,         /* GPIO_EMC_00 PAD functional properties : */
	  0x0110F9u);                             /* Slew Rate Field: Fast Slew Rate
												 Drive Strength Field: R0/7
												 Speed Field: max(200MHz)
												 Open Drain Enable Field: Open Drain Disabled
												 Pull / Keep Enable Field: Pull/Keeper Enabled
												 Pull / Keep Select Field: Keeper
												 Pull Up / Down Config. Field: 100K Ohm Pull Down
												 Hyst. Enable Field: Hysteresis Enabled */
	IOMUXC_SetPinConfig(
	  IOMUXC_GPIO_EMC_01_SEMC_DATA01,         /* GPIO_EMC_01 PAD functional properties : */
	  0x0110F9u);                             /* Slew Rate Field: Fast Slew Rate
												 Drive Strength Field: R0/7
												 Speed Field: max(200MHz)
												 Open Drain Enable Field: Open Drain Disabled
												 Pull / Keep Enable Field: Pull/Keeper Enabled
												 Pull / Keep Select Field: Keeper
												 Pull Up / Down Config. Field: 100K Ohm Pull Down
												 Hyst. Enable Field: Hysteresis Enabled */
	IOMUXC_SetPinConfig(
	  IOMUXC_GPIO_EMC_02_SEMC_DATA02,         /* GPIO_EMC_02 PAD functional properties : */
	  0x0110F9u);                             /* Slew Rate Field: Fast Slew Rate
												 Drive Strength Field: R0/7
												 Speed Field: max(200MHz)
												 Open Drain Enable Field: Open Drain Disabled
												 Pull / Keep Enable Field: Pull/Keeper Enabled
												 Pull / Keep Select Field: Keeper
												 Pull Up / Down Config. Field: 100K Ohm Pull Down
												 Hyst. Enable Field: Hysteresis Enabled */
	IOMUXC_SetPinConfig(
	  IOMUXC_GPIO_EMC_03_SEMC_DATA03,         /* GPIO_EMC_03 PAD functional properties : */
	  0x0110F9u);                             /* Slew Rate Field: Fast Slew Rate
												 Drive Strength Field: R0/7
												 Speed Field: max(200MHz)
												 Open Drain Enable Field: Open Drain Disabled
												 Pull / Keep Enable Field: Pull/Keeper Enabled
												 Pull / Keep Select Field: Keeper
												 Pull Up / Down Config. Field: 100K Ohm Pull Down
												 Hyst. Enable Field: Hysteresis Enabled */
	IOMUXC_SetPinConfig(
	  IOMUXC_GPIO_EMC_04_SEMC_DATA04,         /* GPIO_EMC_04 PAD functional properties : */
	  0x0110F9u);                             /* Slew Rate Field: Fast Slew Rate
												 Drive Strength Field: R0/7
												 Speed Field: max(200MHz)
												 Open Drain Enable Field: Open Drain Disabled
												 Pull / Keep Enable Field: Pull/Keeper Enabled
												 Pull / Keep Select Field: Keeper
												 Pull Up / Down Config. Field: 100K Ohm Pull Down
												 Hyst. Enable Field: Hysteresis Enabled */
	IOMUXC_SetPinConfig(
	  IOMUXC_GPIO_EMC_05_SEMC_DATA05,         /* GPIO_EMC_05 PAD functional properties : */
	  0x0110F9u);                             /* Slew Rate Field: Fast Slew Rate
												 Drive Strength Field: R0/7
												 Speed Field: max(200MHz)
												 Open Drain Enable Field: Open Drain Disabled
												 Pull / Keep Enable Field: Pull/Keeper Enabled
												 Pull / Keep Select Field: Keeper
												 Pull Up / Down Config. Field: 100K Ohm Pull Down
												 Hyst. Enable Field: Hysteresis Enabled */
	IOMUXC_SetPinConfig(
	  IOMUXC_GPIO_EMC_06_SEMC_DATA06,         /* GPIO_EMC_06 PAD functional properties : */
	  0x0110F9u);                             /* Slew Rate Field: Fast Slew Rate
												 Drive Strength Field: R0/7
												 Speed Field: max(200MHz)
												 Open Drain Enable Field: Open Drain Disabled
												 Pull / Keep Enable Field: Pull/Keeper Enabled
												 Pull / Keep Select Field: Keeper
												 Pull Up / Down Config. Field: 100K Ohm Pull Down
												 Hyst. Enable Field: Hysteresis Enabled */
	IOMUXC_SetPinConfig(
	  IOMUXC_GPIO_EMC_07_SEMC_DATA07,         /* GPIO_EMC_07 PAD functional properties : */
	  0x0110F9u);                             /* Slew Rate Field: Fast Slew Rate
												 Drive Strength Field: R0/7
												 Speed Field: max(200MHz)
												 Open Drain Enable Field: Open Drain Disabled
												 Pull / Keep Enable Field: Pull/Keeper Enabled
												 Pull / Keep Select Field: Keeper
												 Pull Up / Down Config. Field: 100K Ohm Pull Down
												 Hyst. Enable Field: Hysteresis Enabled */


    IOMUXC_SetPinConfig(
        IOMUXC_GPIO_EMC_40_SEMC_RDY,            /* GPIO_EMC_40 PAD functional properties : */
        0x01F0F9U);                             /* Slew Rate Field: Fast Slew Rate
                                                   Drive Strength Field: R0/7
                                                   Speed Field: max(200MHz)
                                                   Open Drain Enable Field: Open Drain Disabled
                                                   Pull / Keep Enable Field: Pull/Keeper Enabled
                                                   Pull / Keep Select Field: Pull
                                                   Pull Up / Down Config. Field: 22K Ohm Pull Up
                                                   Hyst. Enable Field: Hysteresis Enabled */
    IOMUXC_SetPinConfig(
		IOMUXC_GPIO_EMC_41_SEMC_CSX00,         		/* GPIO_EMC_41 PAD functional properties : */
		0x0110F9u);                             	/* Slew Rate Field: Fast Slew Rate
													 Drive Strength Field: R0/7
													 Speed Field: max(200MHz)
													 Open Drain Enable Field: Open Drain Disabled
													 Pull / Keep Enable Field: Pull/Keeper Enabled
													 Pull / Keep Select Field: Keeper
													 Pull Up / Down Config. Field: 100K Ohm Pull Down
													 Hyst. Enable Field: Hysteresis Enabled */
	IOMUXC_SetPinConfig(
		IOMUXC_GPIO_EMC_19_SEMC_ADDR11,         	/* GPIO_EMC_19 PAD functional properties : */
		0x0110F9u);                             	/* Slew Rate Field: Fast Slew Rate
													 Drive Strength Field: R0/7
													 Speed Field: max(200MHz)
													 Open Drain Enable Field: Open Drain Disabled
													 Pull / Keep Enable Field: Pull/Keeper Enabled
													 Pull / Keep Select Field: Keeper
													 Pull Up / Down Config. Field: 100K Ohm Pull Down
													 Hyst. Enable Field: Hysteresis Enabled */
	IOMUXC_SetPinConfig(
		IOMUXC_GPIO_EMC_20_SEMC_ADDR12,         	/* GPIO_EMC_20 PAD functional properties : */
		0x0110F9u);                             	/* Slew Rate Field: Fast Slew Rate
													 Drive Strength Field: R0/7
													 Speed Field: max(200MHz)
													 Open Drain Enable Field: Open Drain Disabled
													 Pull / Keep Enable Field: Pull/Keeper Enabled
													 Pull / Keep Select Field: Keeper
													 Pull Up / Down Config. Field: 100K Ohm Pull Down
													 Hyst. Enable Field: Hysteresis Enabled */
	IOMUXC_SetPinConfig(
		IOMUXC_GPIO_EMC_22_SEMC_BA1,         		/* GPIO_EMC_22 PAD functional properties : */
		0x0110F9u);                             	/* Slew Rate Field: Fast Slew Rate
													 Drive Strength Field: R0/7
													 Speed Field: max(200MHz)
													 Open Drain Enable Field: Open Drain Disabled
													 Pull / Keep Enable Field: Pull/Keeper Enabled
													 Pull / Keep Select Field: Keeper
													 Pull Up / Down Config. Field: 100K Ohm Pull Down
													 Hyst. Enable Field: Hysteresis Enabled */
	IOMUXC_SetPinConfig(
		IOMUXC_GPIO_EMC_18_SEMC_ADDR09,         	/* GPIO_EMC_18 PAD functional properties : */
		0x0110F9u);                             	/* Slew Rate Field: Fast Slew Rate
													 Drive Strength Field: R0/7
													 Speed Field: max(200MHz)
													 Open Drain Enable Field: Open Drain Disabled
													 Pull / Keep Enable Field: Pull/Keeper Enabled
													 Pull / Keep Select Field: Keeper
													 Pull Up / Down Config. Field: 100K Ohm Pull Down
													 Hyst. Enable Field: Hysteresis Enabled */
}

static status_t _NAND_InitSEMC(void)
{
	semc_config_t config;
	semc_nand_config_t nandConfig;
	semc_nand_timing_config_t timeConfig;
	uint32_t clockFrq = NAND_SEMC_CLK_FREQ;

	/* Initializes the configure structure to zero. */
	memset(&config, 0, sizeof(semc_config_t));
	memset(&nandConfig, 0, sizeof(semc_nand_config_t));

#if 0 /* Do not re-initialize SEMC, it will crash when code is already running from SDRAM */
	/* Initialize SEMC. */
	SEMC_GetDefaultConfig(&config);
	config.dqsMode = kSEMC_Loopbackdqspad;
	SEMC_Init(NAND_SEMC, &config);
#endif /* 0 */

	timeConfig.tCeSetup_Ns = 66;
	timeConfig.tCeHold_Ns = 18;
	timeConfig.tCeInterval_Ns = 3;
	timeConfig.tWeLow_Ns = 48;
	timeConfig.tWeHigh_Ns = 30;
	timeConfig.tReLow_Ns = 48;
	timeConfig.tReHigh_Ns = 30;
	timeConfig.tTurnAround_Ns = 3;
	timeConfig.tWehigh2Relow_Ns = 120;
	timeConfig.tRehigh2Welow_Ns = 198;
	timeConfig.tAle2WriteStart_Ns = 198;
	timeConfig.tReady2Relow_Ns = 36;
	timeConfig.tWehigh2Busy_Ns = 198;

	/* Config NAND Flash */
	nandConfig.cePinMux = kSEMC_MUXCSX0;
	nandConfig.axiAddress = 0x80000000U;
	nandConfig.axiMemsize_kbytes = CONFIG_SYS_NAND_SIZE_KB;
	nandConfig.ipgAddress = CONFIG_SYS_NAND_BASE;
	nandConfig.ipgMemsize_kbytes = CONFIG_SYS_NAND_SIZE_KB;
	nandConfig.rdyactivePolarity = kSEMC_RdyActiveLow;
	nandConfig.edoModeEnabled = false;
	nandConfig.columnAddrBitNum = kSEMC_NandColum_11bit;
	nandConfig.arrayAddrOption = kSEMC_NandAddrOption_5byte_CA2RA3;

	nandConfig.burstLen = kSEMC_Nand_BurstLen64;
	nandConfig.portSize = kSEMC_PortSize8Bit;
	nandConfig.timingConfig = &timeConfig;
	return SEMC_ConfigureNAND(NAND_SEMC, &nandConfig, clockFrq);
}

bool BSP_NAND_Ready(void)
{
    return(SEMC_IsNandReady(NAND_SEMC));
}


//*****************************************************************************
//!
//! \brief Returns the status value of the NAND Flash
//!
//! \param  None
//!
//! \return Value of status register
//!
//*****************************************************************************
uint8_t BSP_NAND_Read_Status(void)
{
    status_t status = kStatus_Success;
    uint32_t readoutData;
    uint16_t commandCode;
    uint32_t slaveAddress;

    // Note: If there is only one plane per target, the READ STATUS (70h) command can be used
    //   to return status following any NAND command.
    // Note: In devices that have more than one plane per target, during and following interleaved
    //  die (multi-plane) operations, the READ STATUS ENHANCED command must be used to select the
    //  die (LUN) that should report status.

    // READ STATUS command is accepted by device even when it is busy (RDY = 0).
    commandCode = SEMC_BuildNandIPCommand(0x70, kSEMC_NANDAM_ColumnRow, kSEMC_NANDCM_CommandRead);

    // Note: For those command without address, the address should be valid as well,
    //  it shouldn't be out of IPG memory space, or SEMC IP will ignore this command.
    slaveAddress = CONFIG_SYS_NAND_BASE;

    status = SEMC_SendIPCommand(NAND_SEMC, kSEMC_MemType_NAND, slaveAddress, commandCode, 0, &readoutData);
    if (status != kStatus_Success)
    {
        printf("fsl_nand, NAND_Read_Status, SEMC_SendIPCommand Failed!\n");
    }

    return((uint8_t)(readoutData & 0xFF));
}


//*****************************************************************************
//!
//! \brief Resets NAND Flash
//!
//! \param  None
//!
//! \return \c void
//!
//*****************************************************************************
void BSP_NAND_Reset(void)
{
    status_t status = kStatus_Success;
    uint32_t dummyData = 0;

    // The RESET command may be executed with the target in any state.
    uint16_t commandCode = SEMC_BuildNandIPCommand(0xFF, kSEMC_NANDAM_ColumnRow, kSEMC_NANDCM_CommandHold);
    status = SEMC_SendIPCommand(NAND_SEMC, kSEMC_MemType_NAND, CONFIG_SYS_NAND_BASE, commandCode, 0, &dummyData);
    // wait
    while(BSP_NAND_Ready() != true);
}


//*****************************************************************************
//!
//! \brief Reads the NAND flash ID
//!
//! \param  buf     Buffer the ID information is written
//!
//! \return \c void
//!
//*****************************************************************************
void BSP_NAND_ReadID(uint8_t *buf)
{
    status_t status = kStatus_Success;
    uint32_t dummyData = 0;
    uint32_t slaveAddress;
    uint16_t commandCode;

    // READ PAGE command is accepted by the device when it is ready (RDY = 1, ARDY = 1).
    while(BSP_NAND_Ready() != true);

    commandCode = SEMC_BuildNandIPCommand(0x90U, kSEMC_NANDAM_ColumnCA0, kSEMC_NANDCM_CommandAddressHold);

    // Note: For those command without address, the address should be valid as well,
    //  it shouldn't be out of IPG memory space, or SEMC IP will ignore this command.
    slaveAddress = CONFIG_SYS_NAND_BASE;
    status = SEMC_SendIPCommand(NAND_SEMC, kSEMC_MemType_NAND, slaveAddress, commandCode, 0, &dummyData);

    if (status != kStatus_Success) {
        printf("fsl_nand, NAND_ReadID, SEMC_SendIPCommand Failed!\n");
        return;
    }

    while(BSP_NAND_Ready() != true);

    // Get ID Bytes
    status = SEMC_IPCommandNandRead(NAND_SEMC, slaveAddress, buf, 5);
#if(DEBUG_NAND_CONFIG == 1)
    int idx;
    debug("NAND Read ID: ");
    for(idx = 0; idx < 5; idx++) {
        debug(" %02X", buf[idx]);
    }
    debug("\n");
#endif
    if (status != kStatus_Success) {
        printf("fsl_nand, NAND_ReadID, SEMC_SendIPCommand Failed!\n");
        return;
    }
}


void BSP_NAND_ReadPageDataOOB(uint32_t pageAddress, uint8_t *buf)
{
    uint32_t slaveAddress;
    uint32_t dummyData = 0;
    uint16_t commandCode;
    status_t status = kStatus_Success;


    while(BSP_NAND_Ready() != true);

    /* Load Page to Buffer */
    commandCode = SEMC_BuildNandIPCommand(
                    0x00U,
                    kSEMC_NANDAM_ColumnRow,
                    kSEMC_NANDCM_CommandAddressHold);
    slaveAddress = CONFIG_SYS_NAND_BASE + (pageAddress * CONFIG_SYS_NAND_PAGE_SIZE);
    status = SEMC_SendIPCommand(NAND_SEMC, kSEMC_MemType_NAND, slaveAddress, commandCode, 0, &dummyData);
    if(status != kStatus_Success) {
        printf("fsl_nand, NAND_ReadPageDataOOB, SEMC_SendIPCommand Failed!\n");
        return;
    }
    commandCode = SEMC_BuildNandIPCommand(
                    0x30U,
                    kSEMC_NANDAM_ColumnRow,
                    kSEMC_NANDCM_CommandHold);
    status = SEMC_SendIPCommand(NAND_SEMC, kSEMC_MemType_NAND, slaveAddress, commandCode, 0, &dummyData);
    if(status != kStatus_Success) {
        printf("fsl_nand, NAND_ReadPageDataOOB, SEMC_SendIPCommand Failed!\n");
        return;
    }

    while(BSP_NAND_Ready() != true);
    status = SEMC_IPCommandNandRead(NAND_SEMC, slaveAddress, buf, CONFIG_SYS_NAND_PAGE_SIZE + CONFIG_SYS_NAND_OOBSIZE);
    if(status != kStatus_Success) {
        printf("fsl_nand, NAND_ReadPageDataOOB, SEMC_SendIPCommand Failed!\n");
        return;
    }
}


void BSP_NAND_Erase(uint8_t command, int32_t page_addr)
{
    uint16_t commandCode;
    uint32_t slaveAddress;
    uint32_t dummyData = 0;
    status_t status = kStatus_Success;

    if(page_addr >= 0) {
        commandCode = SEMC_BuildNandIPCommand(
                            command,
                            kSEMC_NANDAM_RawRA0RA1,
                            kSEMC_NANDCM_CommandAddress);
        slaveAddress = page_addr / CONFIG_SYS_NAND_PAGE_COUNT;  // block
        slaveAddress = slaveAddress * CONFIG_SYS_NAND_PAGE_COUNT * CONFIG_SYS_NAND_PAGE_SIZE;
#if(DEBUG_NAND_CONFIG == 1)
        debug("fsl_nand, NAND_Erase, page_addr:%08X, slaveAddr:%08X\n", page_addr, slaveAddress);
#endif /* #if(DEBUG_NAND_CONFIG == 1) */
    } else {
        commandCode = SEMC_BuildNandIPCommand(
                            command,
                            kSEMC_NANDAM_ColumnRow,
                            kSEMC_NANDCM_CommandHold);
        slaveAddress = CONFIG_SYS_NAND_BASE; // dummy
    }
    status = SEMC_SendIPCommand(NAND_SEMC, kSEMC_MemType_NAND, slaveAddress, commandCode, 0, &dummyData);
    if(status != kStatus_Success) {
        printf("fsl_nand, NAND_Erase, SEMC_SendIPCommand Failed! (commandCode:%04X)\n", commandCode);
        return;
    }
    while(BSP_NAND_Ready() != true);
}


void BSP_NAND_ProgramPage(int32_t page_addr, int32_t column, uint32_t len, uint8_t *buf)
{
    uint32_t slaveAddress;
    uint32_t dummyData = 0;
    uint16_t commandCode;
    status_t status = kStatus_Success;

    if((page_addr < 0) || (column < 0) || (buf == (uint8_t *)0)) {
        printf("fsl_nand, NAND_ProgramPage, invalid argument page:%d, col:%d, buf:%X\n", page_addr, column, buf);
        return;
    }

    while(BSP_NAND_Ready() != true);

    commandCode = SEMC_BuildNandIPCommand(
                        0x80,
                        kSEMC_NANDAM_ColumnRow,
                        kSEMC_NANDCM_CommandAddressHold);
    slaveAddress = (page_addr * CONFIG_SYS_NAND_PAGE_SIZE) + CONFIG_SYS_NAND_BASE;
    status = SEMC_SendIPCommand(NAND_SEMC, kSEMC_MemType_NAND, slaveAddress, commandCode, 0, &dummyData);
    if(status != kStatus_Success) {
        printf("fsl_nand, NAND_ProgramPage, SEMC_SendIPCommand Failed! (commandCode:%04X)\n", commandCode);
        return;
    }

    SEMC_IPCommandNandWrite(NAND_SEMC, slaveAddress, buf, len);

    commandCode = SEMC_BuildNandIPCommand(
                        0x10,
                        kSEMC_NANDAM_ColumnRow,
                        kSEMC_NANDCM_CommandHold);
    status = SEMC_SendIPCommand(NAND_SEMC, kSEMC_MemType_NAND, slaveAddress, commandCode, 0, &dummyData);

    if(status != kStatus_Success) {
        printf("fsl_nand, NAND_ProgramPage, SEMC_SendIPCommand Failed! (commandCode:%04X)\n", commandCode);
        return;
    }

    while(BSP_NAND_Ready() != true);
}



