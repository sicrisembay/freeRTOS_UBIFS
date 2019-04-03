//*****************************************************************************
//!
//! \file ubifs_zpl.c
//!
//! $Author: sicrisre001 $
//! $Date: 2016-06-08 19:06:50 +0800 (Wed, 08 Jun 2016) $
//! $Revision: 677 $
//!
//*****************************************************************************

//*****************************************************************************
// File dependencies.
//*****************************************************************************
#include "stdio.h"
#include "ubifs_zpl.h"
#include "zplCompat.h"
#include "linux/list.h"
#include "nand.h"
#include "FreeRTOS.h"
#include "task.h"
#include "ubiFsConfig.h"
#include "ubi_uboot.h"
#include "jffs2/load_kernel.h"

//*****************************************************************************
// Private definitions.
//*****************************************************************************
#define ENABLE_MTD_READ_TEST            (0)
#define ENABLE_MTD_PAGE_TEST            (0)
#define ENABLE_UBIFS_LOAD_TEST          (1)
#define configTASK_STACK_UBI_FS         (4096)
#define configTASK_PRIORITY_UBI_FS      (tskIDLE_PRIORITY + 1)

//*****************************************************************************
// Private member declarations.
//*****************************************************************************

/* UART Instance0 Gatekeeper Task */
static StaticTask_t xTaskUbiFs;
static StackType_t xTaskStackUbiFs[configTASK_STACK_UBI_FS];
static TaskHandle_t xTaskHandleUbiFs = NULL;

static bool bUbiFsInited = false;
static bool bUbiFsMounted = false;


//*****************************************************************************
// Public / Internal member external declarations.
//*****************************************************************************
extern void nand_init(void);
#if(ENABLE_MTD_READ_TEST == 1)
extern int mtd_readtest_init(void);
#endif
#if(ENABLE_MTD_PAGE_TEST == 1)
extern int mtd_pagetest_init(void);
#endif

//*****************************************************************************
// Private function prototypes.
//*****************************************************************************
static void _UbiFs_Task(void *pxParam);

//*****************************************************************************
// Public function implementations
//*****************************************************************************

//*****************************************************************************
//!
//! \brief Insert brief description here.
//!
//! Insert detailed description here.
//!
//! \param  None
//!
//! \return \c void
//!
//*****************************************************************************
void UBIFS_ZPL_Init(void)
{
    /* Initialize NAND chip */
    nand_init();
    /* Initialize MTD partitions */
    mtdparts_init();
#if(ENABLE_MTD_READ_TEST == 1)
    mtd_readtest_init();
#endif

#if(ENABLE_MTD_PAGE_TEST == 1)
    mtd_pagetest_init();
#endif
    /* Initialize the default UBI partition */
    ubi_part(PARTITION_NAME_DEFAULT, NULL);

    /* Initialize UBIFS */
    if(bUbiFsInited != true) {
        if(!ubifs_init()) {
            bUbiFsInited = true;
        }
    }

    /* Mount UBIFS volume */
    if(bUbiFsInited == true) {
        if(!uboot_ubifs_mount(VOLUME_NAME_DEFAULT)) {
            bUbiFsMounted = true;
        }
    }

#if(ENABLE_UBIFS_LOAD_TEST == 1)
    if(bUbiFsMounted == true) {
        char *filename = "/firmware/freeRTOS_ubiFS.bin";
        char fileContent[64] = {0};
        loff_t actread;
        if(ubifs_ls(filename)) {
            printf("** File not found %s **\n", filename);
        } else {
            if(!ubifs_read(filename, (void *)(&fileContent[0]), (loff_t)0, (loff_t)(sizeof(fileContent)), (loff_t*)&actread)) {
                printf("Contents of %s: %s\n", filename, fileContent);
            } else {
                printf("Error in loading %s", filename);
            }
        }
    } else {
        printf("UBI volume not mounted!");
    }
#endif /* (ENABLE_UBIFS_LOAD_TEST == 1) */

    /* Create Gatekeeper Tasks */
    if(NULL == xTaskHandleUbiFs) {
        xTaskHandleUbiFs = xTaskCreateStatic(
                _UbiFs_Task,
                "UBIFS",
                configTASK_STACK_UBI_FS,
                (void *)0,
                configTASK_PRIORITY_UBI_FS,
                xTaskStackUbiFs,
                &xTaskUbiFs);
    }
}

//*****************************************************************************
// Private function implementations.
//*****************************************************************************

static void _UbiFs_Task(void *pxParam)
{
    while(1) {
        vTaskDelay(10);
    }
}

