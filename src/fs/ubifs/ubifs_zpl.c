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
#include "queue.h"
#include "semphr.h"
#include "ubiFsConfig.h"
#include "ubi_uboot.h"
#include "jffs2/load_kernel.h"
#include "BSP_uart.h"
#include "test/ubifs_zpl_test.h"

//*****************************************************************************
// Private definitions.
//*****************************************************************************
#define ENABLE_MTD_READ_TEST            (0)
#define ENABLE_MTD_PAGE_TEST            (0)
#define ENABLE_UBIFS_LOAD_TEST          (0)
#define ENABLE_DEBUG_PRINTF             (1)
#define configTASK_STACK_UBI_FS         (32768)

typedef enum {
    UBI_ZPL_FILE_EXIST = 0,
    UBI_ZPL_FILE_WRITE,
    UBI_ZPL_FILE_READ,
    UBI_ZPL_FILE_GET_SIZE,
    UBI_ZPL_FILE_REMOVE,
    UBI_ZPL_DIR_MAKE,
    UBI_ZPL_DIR_REMOVE,
} ubi_zpl_ops_t;

typedef struct {
    ubi_zpl_ops_t op;
    void * param1;
    void * param2;
    void * param3;
    uint32_t param4;
    uint32_t param5;
    ubi_zpl_cb_fcn cb;
} ubi_zpl_req_t;

#define UBI_Q_LEN                       (10)
#define UBI_Q_ITEM_SZ                   (sizeof(ubi_zpl_req_t))

#define ubifs_zpl_debug(fmt, args...)    debug_cond(ENABLE_DEBUG_PRINTF, fmt, ##args)

//*****************************************************************************
// Private member declarations.
//*****************************************************************************

/* Gatekeeper Task */
static StaticTask_t xTaskUbiFs;
static StackType_t xTaskStackUbiFs[configTASK_STACK_UBI_FS];
static TaskHandle_t xTaskHandleUbiFs = NULL;
/* Gatekeeper Transaction Queue */
static StaticQueue_t xStaticQueueUbi;
static uint8_t ucQueueStorageUbi[UBI_Q_LEN * UBI_Q_ITEM_SZ];
static QueueHandle_t xQueueHandleUbi = NULL;

static bool bUbiPartMounted = false;
static bool bUbiFsInited = false;
static bool bUbiFsMounted = false;
static bool bInitDone = false;

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
static void _Ubi_Task(void *pxParam);

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
UBI_ZPL_RET_T UBI_ZPL_Init(void)
{
    UBI_ZPL_RET_T retval = UBI_ZPL_NOERROR;

    /* Create Gatekeeper Tasks */
    if(NULL == xTaskHandleUbiFs) {
        ubifs_zpl_debug("Info: Creating UBIFS Test Task...\n");
        xTaskHandleUbiFs = xTaskCreateStatic(
                _Ubi_Task,
                "UBIFS",
                configTASK_STACK_UBI_FS,
                (void *)0,
                configTASK_PRIORITY_UBI_FS,
                xTaskStackUbiFs,
                &xTaskUbiFs);

        xQueueHandleUbi = xQueueCreateStatic(
                            UBI_Q_LEN,
                            UBI_Q_ITEM_SZ,
                            ucQueueStorageUbi,
                            &xStaticQueueUbi
                            );
    }

    return (retval);
}

//*****************************************************************************
// Private function implementations.
//*****************************************************************************
static void _Ubi_Task(void *pxParam)
{
    ubi_zpl_req_t ubiZplReq;
    int sts = 0;
    int err = 0;
    int opCnt = 0;
    loff_t temp64;

    bUbiPartMounted = false;
    bUbiFsInited = false;
    bUbiFsMounted = false;
    bInitDone = false;

    /* Initialize NAND chip */
    ubifs_zpl_debug("Info: Initializing NAND flash...");
    nand_init();
    /* Initialize MTD partitions */
    ubifs_zpl_debug("Info: Initializing MTD...");
    err = mtdparts_init();
    if(err != 0) {
        ubifs_zpl_debug("Error: MTD init failed(Err:%d)", err);
        vTaskSuspend(NULL);
    }
#if(ENABLE_MTD_READ_TEST == 1)
    mtd_readtest_init();
#endif

#if(ENABLE_MTD_PAGE_TEST == 1)
    mtd_pagetest_init();
#endif
    /* Initialize the default UBI partition */
    ubifs_zpl_debug("Info: Initializing UBI partition...");
    err = ubi_part(PARTITION_NAME_DEFAULT, NULL);
    if(!err) {
        bUbiPartMounted = true;
    } else {
        ubifs_zpl_debug("Error: UBI Part init failed(Err:%d)", err);
        bUbiPartMounted = false;
        vTaskSuspend(NULL);
    }

    /* Initialize UBIFS */
    ubifs_zpl_debug("Info: Initializing UBIFS...");
    err = ubifs_init();
    if(!err) {
        bUbiFsInited = true;
    } else {
        bUbiFsInited = false;
        ubifs_zpl_debug("Error: UBIFS init failed(Err:%d)", err);
        vTaskSuspend(NULL);
    }

    if(bUbiFsInited) {
        ubifs_zpl_debug("Info: Mounting UBIFS...");
        err = uboot_ubifs_mount(VOLUME_NAME_DEFAULT);
        if(!err) {
            bUbiFsMounted = true;
        } else {
            bUbiFsMounted = false;
            ubifs_zpl_debug("Error: UBIFS mount failed(Err:%d)", err);
            vTaskSuspend(NULL);
        }
    }

    bInitDone = bUbiPartMounted && bUbiFsInited && bUbiFsMounted;

#if((ENABLE_UBIFS_ZPL_TEST == 1) && (ENABLE_FS_TEST == 1))
    UBI_ZPL_FsTestInit();
#endif /* #if((ENABLE_UBIFS_ZPL_TEST == 1) && (ENABLE_FS_TEST == 1)) */

    while(1) {
        if(xQueueReceive(xQueueHandleUbi, &ubiZplReq, portMAX_DELAY)) {
            switch(ubiZplReq.op) {
            case UBI_ZPL_FILE_EXIST: {
                if(bUbiFsMounted == false) {
                    err = uboot_ubifs_mount(VOLUME_NAME_DEFAULT);
                    if(!err) {
                        bUbiFsMounted = true;
                    } else {
                        ubifs_zpl_debug("Error: UBIFS mount failed(Err:%d)", err);
                        bUbiFsMounted = false;
                    }
                }
                if((bUbiFsMounted != 0) && (ubiZplReq.param1 != NULL) &&
                   (ubiZplReq.param2 != NULL)){
                    opCnt++;
                    /* File system operation */
                    *((int *)ubiZplReq.param2) = ubifs_exists((char *)ubiZplReq.param1);
                    sts = true;
                } else {
                    sts = false;
                }
                if(ubiZplReq.cb != NULL) {
                    ubiZplReq.cb(sts);
                }
                break;
            }
            case UBI_ZPL_FILE_WRITE: {
                if(bUbiFsMounted == false) {
                    err = uboot_ubifs_mount(VOLUME_NAME_DEFAULT);
                    if(!err) {
                        bUbiFsMounted = true;
                    } else {
                        ubifs_zpl_debug("Error: UBIFS mount failed(Err:%d)", err);
                        bUbiFsMounted = false;
                    }
                }
                if(bUbiFsMounted) {
                    opCnt++;
                    /* File system operation */
                    err = ubifs_write((char *)ubiZplReq.param1,     // filename
                                    (void *)ubiZplReq.param2,       // buf
                                    (loff_t)(ubiZplReq.param4),     // offset
                                    (loff_t)(ubiZplReq.param5),     // size
                                    (loff_t *)(&temp64)             // actual written bytes
                                    );
                    if(!err) {
                        sts = true;
                        *((uint32_t *)(ubiZplReq.param3)) = (uint32_t)temp64;
                    } else {
                        ubifs_zpl_debug("Error: ubifs_write() fail (Err:%d)", err);
                        sts = false;
                    }
                } else {
                    sts = false;
                }
                if(ubiZplReq.cb != NULL) {
                    ubiZplReq.cb(sts);
                }
                break;
            }
            case UBI_ZPL_FILE_READ: {
                if(bUbiFsMounted == false) {
                    err = uboot_ubifs_mount(VOLUME_NAME_DEFAULT);
                    if(!err) {
                        bUbiFsMounted = true;
                    } else {
                        ubifs_zpl_debug("Error: UBIFS mount failed(Err:%d)", err);
                        bUbiFsMounted = false;
                    }
                }
                if(bUbiFsMounted) {
                    opCnt++;
                    /* File system operation */
                    err = ubifs_read((char *)ubiZplReq.param1,     // filename
                                   (void *)ubiZplReq.param2,       // buf
                                   (loff_t)(ubiZplReq.param4),     // offset
                                   (loff_t)(ubiZplReq.param5),     // size
                                   (loff_t *)(&temp64)             // actual bytes read
                                   );
                    if(!err) {
                        sts = true;
                        *((uint32_t *)(ubiZplReq.param3)) = (uint32_t)temp64;
                    } else {
                        ubifs_zpl_debug("Error: ubifs_read() fail(Err:%d)", err);
                        sts = false;
                    }
                } else {
                    sts = false;
                }
                if(ubiZplReq.cb != NULL) {
                    ubiZplReq.cb(sts);
                }
                break;
            }
            case UBI_ZPL_FILE_GET_SIZE: {
                if(bUbiFsMounted == false) {
                    err = uboot_ubifs_mount(VOLUME_NAME_DEFAULT);
                    if(!err) {
                        bUbiFsMounted = true;
                    } else {
                        ubifs_zpl_debug("Error: UBIFS mount failed(Err:%d)", err);
                        bUbiFsMounted = false;
                    }
                }
                if(bUbiFsMounted) {
                    opCnt++;
                    /* File system operation */
                    err = ubifs_size((char *)ubiZplReq.param1,    // filename
                                   (loff_t *)(&temp64)            // size
                                   );
                    if(!err) {
                        sts = true;
                        *((uint32_t *)(ubiZplReq.param2)) = (uint32_t)temp64;
                    } else {
                        ubifs_zpl_debug("Error: ubifs_size() fail(Err:%d)", err);
                        sts = false;
                    }
                } else {
                    sts = false;
                }
                if(ubiZplReq.cb != NULL) {
                    ubiZplReq.cb(sts);
                }
                break;
            }
            case UBI_ZPL_FILE_REMOVE: {
                if(bUbiFsMounted == false) {
                    err = uboot_ubifs_mount(VOLUME_NAME_DEFAULT);
                    if(!err) {
                        bUbiFsMounted = true;
                    } else {
                        ubifs_zpl_debug("Error: UBIFS mount failed(Err:%d)", err);
                        bUbiFsMounted = false;
                    }
                }
                if(bUbiFsMounted) {
                    opCnt++;
                    /* File system operation */
                    err = ubifs_unlink((char *)ubiZplReq.param1);
                    if(!err) {
                        sts = true;
                    } else {
                        ubifs_zpl_debug("Error: ubifs_unlink() fail (Err:%d)", err);
                        sts = false;
                    }
                } else {
                    sts = false;
                }
                if(ubiZplReq.cb != NULL) {
                    ubiZplReq.cb(sts);
                }
                break;
            }
            case UBI_ZPL_DIR_MAKE: {
                if(bUbiFsMounted == false) {
                    err = uboot_ubifs_mount(VOLUME_NAME_DEFAULT);
                    if(!err) {
                        bUbiFsMounted = true;
                    } else {
                        ubifs_zpl_debug("Error: UBIFS mount failed(Err:%d)", err);
                        bUbiFsMounted = false;
                    }
                }
                if(bUbiFsMounted) {
                    opCnt++;
                    /* File system operation */
                    err = ubifs_mkdir((char *)ubiZplReq.param1);
                    if(!err) {
                        sts = true;
                    } else {
                        ubifs_zpl_debug("Error: ubifs_mkdir() fail(Err:%d)", err);
                        sts = false;
                    }
                } else {
                    sts = false;
                }
                if(ubiZplReq.cb != NULL) {
                    ubiZplReq.cb(sts);
                }
                break;
            }
            case UBI_ZPL_DIR_REMOVE: {
                if(bUbiFsMounted == false) {
                    err = uboot_ubifs_mount(VOLUME_NAME_DEFAULT);
                    if(!err) {
                        bUbiFsMounted = true;
                    } else {
                        ubifs_zpl_debug("Error: UBIFS mount failed(Err:%d)", err);
                        bUbiFsMounted = false;
                    }
                }
                if(bUbiFsMounted) {
                    opCnt++;
                    /* File system operation */
                    err = ubifs_rmdir((char *)ubiZplReq.param1);
                    if(!err) {
                        sts = true;
                    } else {
                        ubifs_zpl_debug("Error: ubifs_rmdir() fail(Err:%d)", err);
                        sts = false;
                    }
                } else {
                    sts = false;
                }
                if(ubiZplReq.cb != NULL) {
                    ubiZplReq.cb(sts);
                }
                break;
            }
            default: {
                break;
            }
            }
        }
    }
    ubi_exit();
    vTaskDelete(NULL);
}

UBI_ZPL_RET_T UBI_ZPL_FileWrite(char * filename, void *buf, uint32_t offset, uint32_t size, uint32_t *actwritten, ubi_zpl_cb_fcn cb)
{
    UBI_ZPL_RET_T retval = UBI_ZPL_NOERROR;
    ubi_zpl_req_t req;

    if(bInitDone != true) {
        return UBI_ZPL_NOT_INITED;
    }

    if((filename == NULL) || (buf == NULL) || (actwritten == NULL)) {
        retval = UBI_ZPL_INVALID_ARG;
    } else {
        req.op = UBI_ZPL_FILE_WRITE;
        req.param1 = (void *)filename;
        req.param2 = buf;
        req.param4 = offset;
        req.param5 = size;
        req.param3 = (void *)actwritten;
        req.cb = cb;

        /* Send Request to Queue without Blocking*/
        if(pdTRUE != xQueueSend(xQueueHandleUbi, &req, (TickType_t)0)) {
            retval = UBI_ZPL_QUEUE_FULL;
        }
    }

    return(retval);
}

UBI_ZPL_RET_T UBI_ZPL_FileRead(char * filename, void *buf, uint32_t offset, uint32_t size, uint32_t *actread, ubi_zpl_cb_fcn cb)
{
    UBI_ZPL_RET_T retval = UBI_ZPL_NOERROR;
    ubi_zpl_req_t req;

    if(bInitDone != true) {
        return UBI_ZPL_NOT_INITED;
    }

    if((filename == NULL) || (buf == NULL) || (actread == NULL)) {
        retval = UBI_ZPL_INVALID_ARG;
    } else {
        req.op = UBI_ZPL_FILE_READ;
        req.param1 = (void *)filename;
        req.param2 = buf;
        req.param4 = offset;
        req.param5 = size;
        req.param3 = (void *)actread;
        req.cb = cb;

        /* Send Request to Queue without Blocking*/
        if(pdTRUE != xQueueSend(xQueueHandleUbi, &req, (TickType_t)0)) {
            retval = UBI_ZPL_QUEUE_FULL;
        }
    }

    return(retval);
}


UBI_ZPL_RET_T UBI_ZPL_FileGetSize(const char * filename, uint32_t *size, ubi_zpl_cb_fcn cb)
{
    UBI_ZPL_RET_T retval = UBI_ZPL_NOERROR;
    ubi_zpl_req_t req = {0};

    if(bInitDone != true) {
        return UBI_ZPL_NOT_INITED;
    }

    if(filename == NULL) {
        retval = UBI_ZPL_INVALID_ARG;
    } else {
        req.op = UBI_ZPL_FILE_GET_SIZE;
        req.param1 = (void *)filename;
        req.param2 = (void *)size;
        req.cb = cb;

        /* Send Request to Queue without Blocking*/
        if(pdTRUE != xQueueSend(xQueueHandleUbi, &req, (TickType_t)0)) {
            retval = UBI_ZPL_QUEUE_FULL;
        }
    }

    return(retval);
}

UBI_ZPL_RET_T UBI_ZPL_FileExist(
        const char * filename,
        int * bExist,
        ubi_zpl_cb_fcn cb)
{
    UBI_ZPL_RET_T retval = UBI_ZPL_NOERROR;
    ubi_zpl_req_t req = {0};

    if(bInitDone != true) {
        return UBI_ZPL_NOT_INITED;
    }

    if((filename == NULL) || (bExist == NULL)) {
        retval = UBI_ZPL_INVALID_ARG;
    } else {
        req.op = UBI_ZPL_FILE_EXIST;
        req.param1 = (void *)filename;
        req.param2 = (void *)bExist;
        req.cb = cb;

        /* Send Request to Queue without Blocking*/
        if(pdTRUE != xQueueSend(xQueueHandleUbi, &req, (TickType_t)0)) {
            retval = UBI_ZPL_QUEUE_FULL;
        }
    }

    return(retval);
}

UBI_ZPL_RET_T UBI_ZPL_RmFile(
        const char * filename,
        ubi_zpl_cb_fcn cb)
{
    UBI_ZPL_RET_T retval = UBI_ZPL_NOERROR;
    ubi_zpl_req_t req = {0};

    if(bInitDone != true) {
        return UBI_ZPL_NOT_INITED;
    }

    if(filename == NULL) {
        retval = UBI_ZPL_INVALID_ARG;
    } else {
        req.op = UBI_ZPL_FILE_REMOVE;
        req.param1 = (void *)filename;
        req.cb = cb;

        /* Send Request to Queue without Blocking*/
        if(pdTRUE != xQueueSend(xQueueHandleUbi, &req, (TickType_t)0)) {
            retval = UBI_ZPL_QUEUE_FULL;
        }
    }

    return(retval);
}

UBI_ZPL_RET_T UBI_ZPL_MkDir(
        const char * dirname,
        ubi_zpl_cb_fcn cb)
{
    UBI_ZPL_RET_T retval = UBI_ZPL_NOERROR;
    ubi_zpl_req_t req = {0};

    if(bInitDone != true) {
        return UBI_ZPL_NOT_INITED;
    }

    if(dirname == NULL) {
        retval = UBI_ZPL_INVALID_ARG;
    } else {
        req.op = UBI_ZPL_DIR_MAKE;
        req.param1 = (void *)dirname;
        req.cb = cb;

        /* Send Request to Queue without Blocking*/
        if(pdTRUE != xQueueSend(xQueueHandleUbi, &req, (TickType_t)0)) {
            retval = UBI_ZPL_QUEUE_FULL;
        }
    }

    return(retval);
}

UBI_ZPL_RET_T UBI_ZPL_RmDir(
        const char * dirname,
        ubi_zpl_cb_fcn cb)
{
    UBI_ZPL_RET_T retval = UBI_ZPL_NOERROR;
    ubi_zpl_req_t req = {0};

    if(bInitDone != true) {
        return UBI_ZPL_NOT_INITED;
    }

    if(dirname == NULL) {
        retval = UBI_ZPL_INVALID_ARG;
    } else {
        req.op = UBI_ZPL_DIR_REMOVE;
        req.param1 = (void *)dirname;
        req.cb = cb;

        /* Send Request to Queue without Blocking*/
        if(pdTRUE != xQueueSend(xQueueHandleUbi, &req, (TickType_t)0)) {
            retval = UBI_ZPL_QUEUE_FULL;
        }
    }

    return(retval);
}
