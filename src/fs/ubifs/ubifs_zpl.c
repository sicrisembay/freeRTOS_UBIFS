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
#include "BSP_uart.h"

//*****************************************************************************
// Private definitions.
//*****************************************************************************
#define ENABLE_MTD_READ_TEST            (0)
#define ENABLE_MTD_PAGE_TEST            (0)
#define ENABLE_UBIFS_LOAD_TEST          (1)
#define configTASK_STACK_UBI_FS         (16384)
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

char logData[MAX_LOG_LEN+1];

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
    debug("Info: Initializing NAND flash...");
    nand_init();
    /* Initialize MTD partitions */
    debug("Info: Initializing MTD...");
    mtdparts_init();
#if(ENABLE_MTD_READ_TEST == 1)
    mtd_readtest_init();
#endif

#if(ENABLE_MTD_PAGE_TEST == 1)
    mtd_pagetest_init();
#endif
    /* Initialize the default UBI partition */
    debug("Info: Initializing UBI partition...");
    ubi_part(PARTITION_NAME_DEFAULT, NULL);

    /* Initialize UBIFS */
    if(bUbiFsInited != true) {
        debug("Info: Initializing UBIFS...");
        if(!ubifs_init()) {
            bUbiFsInited = true;
        }
    }

    /* Create Gatekeeper Tasks */
    if(NULL == xTaskHandleUbiFs) {
        debug("Info: Creating UBIFS Test Task...");
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
static char *filename = "/firmware/project_tres.bin";
static char filename2[256] = "/test";
static char dirname[256] = "/test_dir";
static char fileContent[1024*1024];
static char fileContent2[1024*1024];

static void _UbiFs_Task(void *pxParam)
{
    uint32_t idx = 0;
    loff_t actread, off;
    int i, j;

    while(1) {
        debug("Free Heap before mount: %d, Min. Heap: %d\n", xPortGetFreeHeapSize(), xPortGetMinimumEverFreeHeapSize());

        /* Mount UBIFS volume */
        if(bUbiFsInited == true) {
            debug("Info: Mounting UBIFS...\n");
            if(!uboot_ubifs_mount(VOLUME_NAME_DEFAULT)) {
                bUbiFsMounted = true;
            } else {
                debug("ERROR: ubifs_mount failed!\n");
            }
        }
        if(bUbiFsMounted == true) {
            if(ubifs_read("/iterationCount", (void *)(&idx), (loff_t)0, (loff_t)4, (loff_t*)&actread)) {
                idx = 0;
                debug("Info: First test iteration.");
            }
            idx++;
            debug("Test Iteration %d ...", idx);
            ubifs_ls("/");
            debug("Info: Write File/Directory Test");
            for (i = 0; i < 10; i++) {
                snprintf(dirname, sizeof(dirname), "/test_dir%02i", i);
                ubifs_mkdir(dirname);

                debug("Info: Writing files in %s", dirname);
                for (j = 0; j < 10; j++) {
                    snprintf(filename2, sizeof(filename2), "%s/file%02i", dirname, j);
                    memset(fileContent, 0x0, sizeof(fileContent));
                    off = 4096*j;
                    memset(&fileContent[0] + off, 'j', 10*(j + 1));
                    if(ubifs_write(filename2, (void *)(&fileContent[0] + off), (loff_t)off, (loff_t)10*(j + 1), (loff_t*)&actread)) {
                        debug("ERROR: ubifs_write-iteration:%d, filename:%s count:%d offset:0x08X\n", idx, filename2, 10*(j+1), (uint32_t)off);
                    }
                }
            }
            debug("Done\n");
            ubifs_ls("/");

            for (i = 0; i < 10; i++) {
                snprintf(dirname, sizeof(dirname), "/test_dir%02i", i);
                debug("Info: Verifying Files in %s ...", dirname);
                for (j = 0; j < 10; j++) {
                    snprintf(filename2, sizeof(filename2), "%s/file%02i", dirname, j);
                    memset(fileContent, 0x0, sizeof(fileContent));
                    off = 4096*j;
                    memset(&fileContent[0] + off, 'j', 10*(j + 1));
                    memset(fileContent2, 0x0, sizeof(fileContent2));
                    if(ubifs_read(filename2, (void *)(&fileContent2[0]), (loff_t)0, (loff_t)off + 10*(j+1), (loff_t*)&actread)) {
                        debug("ERROR: ubifs_read-iteration:%d, filename:%s\n", idx, filename2);
                    }
                    if (memcmp(fileContent2, fileContent, off + 10*(j + 1))) {
                        debug("ERROR: content error-iteration:%d, filename:%s\n", idx, filename2);
                    }
                    if (ubifs_unlink(filename2)) {
                        debug("EROOR: ubifs_unlink-iteration:%d, filename:%s\n", filename2);
                    }
                }
                debug("Done.");
                debug("Deleting %s... ", dirname);
                if (ubifs_rmdir(dirname)) {
                    debug("ERROR: ubifs_rmdir-iteration:%d, dir: %s\n", idx, dirname);
                }

                ubifs_ls("/");
            }
            if(ubifs_write("/iterationCount", (void *)(&idx), (loff_t)0, (loff_t)4, (loff_t*)&actread)) {
                debug("ERROR: ubifs_write-iteration:%d, file:iteration", idx);
            }
            debug("Info: Unmounting UBIFS...");
            uboot_ubifs_umount();
            bUbiFsMounted = false;
            debug("Free Heap after umount: %d, Min. Heap: %d\n", xPortGetFreeHeapSize(), xPortGetMinimumEverFreeHeapSize());
            debug(".... done Iteration %d\n\n\n", idx);
        } else {
            debug("ERROR: UBI volume not mounted!");
        }
    }
    ubi_exit();
    vTaskDelete(NULL);
}

