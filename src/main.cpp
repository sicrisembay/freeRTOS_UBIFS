#include "FreeRTOS.h"
#include "task.h"
#include "ubifs/ubifs_zpl.h"
#include "BSP_uart.h""
#include "fsl_clock.h"
#include "stdio.h"

#define BOARD_BOOTCLOCKRUN_CORE_CLOCK             600000000U  /*!< Core clock frequency: 600000000Hz */

#define __writeMemory32(v, addr, x) *((volatile uint32_t *)(addr)) = v
#define __readMemory32(addr, x) *((volatile uint32_t *)(addr))

const clock_arm_pll_config_t armPllConfig_BOARD_BootClockRUN = {
    .loopDivider = 100, /* PLL loop divider, Fout = Fin * 50 */
};
const clock_sys_pll_config_t sysPllConfig_BOARD_BootClockRUN = {
    .loopDivider = 1,                         /* PLL loop divider, Fout = Fin * ( 20 + loopDivider*2 + numerator / denominator ) */
    .numerator = 0,                           /* 30 bit numerator of fractional loop divider */
    .denominator = 1,                         /* 30 bit denominator of fractional loop divider */
};
const clock_usb_pll_config_t usb1PllConfig_BOARD_BootClockRUN = {
    .loopDivider = 0, /* PLL loop divider, Fout = Fin * 20 */
};

static void BOARD_ConfigMPU(void)
{
    /* Disable I cache and D cache */
    if (SCB_CCR_IC_Msk == (SCB_CCR_IC_Msk & SCB->CCR)) {
        SCB_DisableICache();
    }
    if (SCB_CCR_DC_Msk == (SCB_CCR_DC_Msk & SCB->CCR)) {
        SCB_DisableDCache();
    }

    /* Disable MPU */
    ARM_MPU_Disable();

    /* MPU configure:
     * Use ARM_MPU_RASR(DisableExec, AccessPermission, TypeExtField, IsShareable, IsCacheable, IsBufferable, SubRegionDisable, Size)
     * API in core_cm7.h.
     * param DisableExec       Instruction access (XN) disable bit,0=instruction fetches enabled, 1=instruction fetches disabled.
     * param AccessPermission  Data access permissions, allows you to configure read/write access for User and Privileged mode.
     *      Use MACROS defined in core_cm7.h: ARM_MPU_AP_NONE/ARM_MPU_AP_PRIV/ARM_MPU_AP_URO/ARM_MPU_AP_FULL/ARM_MPU_AP_PRO/ARM_MPU_AP_RO
     * Combine TypeExtField/IsShareable/IsCacheable/IsBufferable to configure MPU memory access attributes.
     *  TypeExtField  IsShareable  IsCacheable  IsBufferable   Memory Attribtue    Shareability        Cache
     *     0             x           0           0             Strongly Ordered    shareable
     *     0             x           0           1              Device             shareable
     *     0             0           1           0              Normal             not shareable   Outer and inner write through no write allocate
     *     0             0           1           1              Normal             not shareable   Outer and inner write back no write allocate
     *     0             1           1           0              Normal             shareable       Outer and inner write through no write allocate
     *     0             1           1           1              Normal             shareable       Outer and inner write back no write allocate
     *     1             0           0           0              Normal             not shareable   outer and inner noncache
     *     1             1           0           0              Normal             shareable       outer and inner noncache
     *     1             0           1           1              Normal             not shareable   outer and inner write back write/read acllocate
     *     1             1           1           1              Normal             shareable       outer and inner write back write/read acllocate
     *     2             x           0           0              Device              not shareable
     *  Above are normal use settings, if your want to see more details or want to config different inner/outter cache policy.
     *  please refer to Table 4-55 /4-56 in arm cortex-M7 generic user guide <dui0646b_cortex_m7_dgug.pdf>
     * param SubRegionDisable  Sub-region disable field. 0=sub-region is enabled, 1=sub-region is disabled.
     * param Size              Region size of the region to be configured. use ARM_MPU_REGION_SIZE_xxx MACRO in core_cm7.h.
     */

    /*
     * 16MB SDRAM is divided into two regions for instruction code and data
     * 8MB Instruction code region starts at 0x80000000
     * 8MB Data region starts at 0x80800000
     */
    ARM_MPU_SetRegionEx(1, 0x80000000, ARM_MPU_RASR(0, ARM_MPU_AP_FULL, 1, 0, 1, 1, 0, ARM_MPU_REGION_SIZE_8MB));
    ARM_MPU_SetRegionEx(2, 0x80800000, ARM_MPU_RASR(1, ARM_MPU_AP_FULL, 1, 1, 0, 0, 0, ARM_MPU_REGION_SIZE_8MB));


    /* Enable MPU */
    ARM_MPU_Enable(MPU_CTRL_PRIVDEFENA_Msk);

    /* Enable I cache and D cache */
    SCB_EnableDCache();
    SCB_EnableICache();
}

void BOARD_BootClockRUN(void)
{
    /* Init RTC OSC clock frequency. */
//    CLOCK_SetRtcXtalFreq(32768U);
    /* Set XTAL 24MHz clock frequency. */
    CLOCK_SetXtalFreq(24000000U);
#if 0 // Done in U-boot
    /* Setting PeriphClk2Mux and PeriphMux to provide stable clock before PLLs are initialed */
    CLOCK_SetMux(kCLOCK_PeriphClk2Mux, 1); /* Set PERIPH_CLK2 MUX to OSC */
    CLOCK_SetMux(kCLOCK_PeriphMux, 1);     /* Set PERIPH_CLK MUX to PERIPH_CLK2 */
#if(USE_BOARD == USE_BOARD_EA_SOM)
    /* Setting the VDD_SOC to 1.5V. It is necessary to config AHB to 600Mhz. */
    DCDC->REG3 = (DCDC->REG3 & (~DCDC_REG3_TRG_MASK)) | DCDC_REG3_TRG(0x12);
    /* Waiting for DCDC_STS_DC_OK bit is asserted */
    while (DCDC_REG0_STS_DC_OK_MASK != (DCDC_REG0_STS_DC_OK_MASK & DCDC->REG0))
    {
    }
#elif(USE_BOARD == USE_BOARD_POC)
    // DCDC is from external source
#endif
    /* Init ARM PLL. */
    CLOCK_InitArmPll(&armPllConfig_BOARD_BootClockRUN);  /* Configure ARM PLL to 1200MHz */
    /* Init System PLL. */
    CLOCK_InitSysPll(&sysPllConfig_BOARD_BootClockRUN);
#endif /* Done in U-boot */
    CLOCK_InitUsb1Pll(&usb1PllConfig_BOARD_BootClockRUN); /* Configure USB1 PLL to 480M */
#if 0 // Done in U-boot
    CLOCK_SetDiv(kCLOCK_ArmDiv, 0x1); /* Set ARM PODF to 1, divide by 2 */
    CLOCK_SetDiv(kCLOCK_AhbDiv, 0x0); /* Set AHB PODF to 0, divide by 1 */
    CLOCK_SetDiv(kCLOCK_IpgDiv, 0x3); /* Set IPG PODF to 3, divede by 4 */
    CLOCK_SetMux(kCLOCK_PrePeriphMux, 0x3); /* Set PRE_PERIPH_CLK to PLL1, 1200M */
    CLOCK_SetMux(kCLOCK_PeriphMux, 0x0);    /* Set PERIPH_CLK MUX to PRE_PERIPH_CLK */
#endif // #if 0 // Done in U-boot
    /* Power down all unused PLL */
    CLOCK_DeinitAudioPll();
    CLOCK_DeinitVideoPll();
    CLOCK_DeinitEnetPll();
    CLOCK_DeinitUsb2Pll();

    /* Configure UART divider to default */
    CLOCK_SetMux(kCLOCK_UartMux, 0); /* Set UART source to PLL3 80M */
    CLOCK_SetDiv(kCLOCK_UartDiv, 0); /* Set UART divider to 1 */

    /* Update core clock */
    SystemCoreClockUpdate();
}

void SystemInitHook (void) {
    for(int i = 0; i < 160; i++) {
        DisableIRQ((IRQn_Type)i);
    }
}

//*****************************************************************************
//!
//! \brief Application entry function.
//!
//! \param  None
//!
//! \return \c void
//!
//*****************************************************************************
int main(void)
{
    BOARD_ConfigMPU();
    BOARD_BootClockRUN();

    BSP_UART_Init();

    UBI_ZPL_Init();
    vTaskStartScheduler();
    return 0;
}
