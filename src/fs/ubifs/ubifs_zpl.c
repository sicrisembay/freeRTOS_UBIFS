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
    uint32_t idx = 0;

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

    for(idx = 0; idx < 10; idx++) {
        /* Mount UBIFS volume */
        if(bUbiFsInited == true) {
            if(!uboot_ubifs_mount(VOLUME_NAME_DEFAULT)) {
                bUbiFsMounted = true;
            }
        }

#if(ENABLE_UBIFS_LOAD_TEST == 1)
        if(bUbiFsMounted == true) {
            char *filename = "/firmware/project_tres.bin";
            char filename2[256] = "/test";
            char dirname[256] = "/test_dir";
            char fileContent[1024*1024];
            char fileContent2[1024*1024];
            loff_t actread, off;
            int i, j;

            for (i = 0; i < 10; i++) {
                snprintf(dirname, sizeof(dirname), "/test_dir%02i", i);
                ubifs_mkdir(dirname);

                for (j = 0; j < 10; j++) {
                    snprintf(filename2, sizeof(filename2), "%s/file%02i", dirname, j);
                    memset(fileContent, 0x0, sizeof(fileContent));
                    off = 4096*j;
                    memset(&fileContent[0] + off, 'j', 10*(j + 1));
                    printf("writing %i bytes to %s at offset 0x%08x\n", 10*(j + 1), filename2, off);
                    if(ubifs_write(filename2, (void *)(&fileContent[0] + off), (loff_t)off, (loff_t)10*(j + 1), (loff_t*)&actread)) {
                        printf("write error\n");
                    }
                }
            }
            printf("ls /\n");
            ubifs_ls("/");
            for (i = 0; i < 10; i++) {
                snprintf(dirname, sizeof(dirname), "/test_dir%02i", i);
                printf("%s\n", dirname);
                ubifs_ls(dirname);

                for (j = 0; j < 10; j++) {
                    snprintf(filename2, sizeof(filename2), "%s/file%02i", dirname, j);
                    memset(fileContent, 0x0, sizeof(fileContent));
                    off = 4096*j;
                    memset(&fileContent[0] + off, 'j', 10*(j + 1));
                    memset(fileContent2, 0x0, sizeof(fileContent2));
                    if(ubifs_read(filename2, (void *)(&fileContent2[0]), (loff_t)0, (loff_t)off + 10*(j+1), (loff_t*)&actread)) {
                        printf("%s read error\n", filename2);
                    }
                    if (memcmp(fileContent2, fileContent, off + 10*(j + 1))) {
                        printf("%s content error\n", filename2);
                    }
        			if (ubifs_unlink(filename2)) {
        				printf("%s unlink error\n", filename2);
        			}
                }
        		printf("%s\n", dirname);
        		ubifs_ls(dirname);
        		if (ubifs_rmdir(dirname)) {
        			printf("%s rmdir error\n", dirname);
        		}
            }
        	printf("ls /\n");
        	ubifs_ls("/");
            uboot_ubifs_umount();
            bUbiFsMounted = false;
        } else {
            printf("UBI volume not mounted!");
        }
#endif /* (ENABLE_UBIFS_LOAD_TEST == 1) */
    }

    ubi_exit();

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

