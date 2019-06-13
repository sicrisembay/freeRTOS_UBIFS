//*****************************************************************************
//!
//! \file ubifs_zpl_test.c
//!
//! \brief Insert brief description.
//!
//! $Author: sicrisre001 $
//! $Date: 2016-06-08 19:06:50 +0800 (Wed, 08 Jun 2016) $
//! $Revision: 677 $
//!
//*****************************************************************************
#include "ubifs_zpl_test.h"

#if (ENABLE_UBIFS_ZPL_TEST == 1)
//*****************************************************************************
// File dependencies.
//*****************************************************************************
#include "stdlib.h"
#include "zplCompat.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "../ubifs_zpl.h"

//*****************************************************************************
// Private definitions.
//*****************************************************************************
#define ubifs_zpl_test_debug(fmt, args...)    debug_cond(1, fmt, ##args)

//*****************************************************************************
// Private member declarations.
//*****************************************************************************
#if (ENABLE_ENV_TEST == 1)
#define configTASK_STACK_ENV_TEST       (8192)
static StaticTask_t xTaskEnvTest;
static StackType_t xTaskStackEnvTest[configTASK_STACK_ENV_TEST];
static TaskHandle_t xTaskHandleEnvTest = NULL;
static SemaphoreHandle_t semHdlEnvOpDone = NULL;
static StaticSemaphore_t semBuffEnvOpDone;
#endif /* #if (ENABLE_ENV_TEST == 1) */

#if (ENABLE_FS_TEST == 1)
#define configTASK_STACK_FS_TEST        (8192)
static StaticTask_t xTaskFsTest;
static StackType_t xTaskStackFsTest[configTASK_STACK_FS_TEST];
static TaskHandle_t xTaskHandleFsTest = NULL;
static SemaphoreHandle_t semHdlFsOpDone = NULL;
static StaticSemaphore_t semBuffFsOpDone;
#endif /* #if (ENABLE_FS_TEST == 1) */

//*****************************************************************************
// Public / Internal member external declarations.
//*****************************************************************************

//*****************************************************************************
// Private function prototypes.
//*****************************************************************************

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


//*****************************************************************************
// Private function implementations.
//*****************************************************************************

#if(ENABLE_ENV_TEST == 1)
static void _EnvTest_cb(bool sts)
{
    xSemaphoreGive(semHdlEnvOpDone);
}

#define ENV_TEST_BUF_SZ     (128)
static char varname[ENV_TEST_BUF_SZ+1] = {0};
static char valString[ENV_TEST_BUF_SZ+1] = {0};
const char * const appFileKey = "appfile";
const char * const bootFileOne = "/firmware/EnvTestBinaryOne.uImage";
const char * const bootFileTwo = "/firmware/EnvTestBinaryTwo.uImage";

static void _EnvTest_Task(void *pxParam)
{
    semHdlEnvOpDone = xSemaphoreCreateBinaryStatic(&semBuffEnvOpDone);

    /* Read Environment Variable */
    UBI_ZPL_EnvGet((char * const)appFileKey, strlen(appFileKey), valString, ENV_TEST_BUF_SZ, _EnvTest_cb);
    xSemaphoreTake(semHdlEnvOpDone, portMAX_DELAY);
    ubifs_zpl_test_debug("%s=%s", appFileKey, valString);

    /* Write Environment Var */
    if(strcmp(valString, bootFileOne) == 0) {
        UBI_ZPL_EnvSet((char * const)appFileKey, strlen(appFileKey), (char * const)bootFileTwo, strlen(bootFileTwo), _EnvTest_cb);
        xSemaphoreTake(semHdlEnvOpDone, portMAX_DELAY);
        ubifs_zpl_test_debug("Written: %s=%s", appFileKey, bootFileTwo);
    } else {
        UBI_ZPL_EnvSet((char * const)appFileKey, strlen(appFileKey), (char * const)bootFileOne, strlen(bootFileOne), _EnvTest_cb);
        xSemaphoreTake(semHdlEnvOpDone, portMAX_DELAY);
        ubifs_zpl_test_debug("Written: %s=%s", appFileKey, bootFileOne);
    }

    /* Save to Flash */
    UBI_ZPL_EnvSave(_EnvTest_cb);
    xSemaphoreTake(semHdlEnvOpDone, portMAX_DELAY);
    ubifs_zpl_test_debug("Save Done");

    vTaskSuspend(NULL);
}

void UBI_ZPL_EnvTestInit(void)
{
    xTaskHandleEnvTest = xTaskCreateStatic(
            _EnvTest_Task,
            "ENV",
            configTASK_STACK_ENV_TEST,
            (void *)0,
            configTASK_PRIORITY_UBI,
            xTaskStackEnvTest,
            &xTaskEnvTest);
}
#endif /* (ENABLE_ENV_TEST == 1) */

#if (ENABLE_FS_TEST == 1)
static char * const testFile = "/fsTest_dir/fsTest.bin";
static char * const dirname = "/fsTest_dir";
#define MAX_FILE_SZ     65536
static char testDataOne[MAX_FILE_SZ];
static char testDataTwo[MAX_FILE_SZ];
static char * const iterationFile = "/iterationCount";

static void _FsTest_cb(bool sts)
{
    xSemaphoreGive(semHdlFsOpDone);
}

static void _FsTest_Task(void *pxParam)
{
    uint32_t actread;
    uint32_t actwritten;
    uint32_t iterationCount;
    uint32_t fileLen;
    uint32_t fileOffset;
    uint32_t idx;
    uint32_t fileSz;
    bool exist = false;

    semHdlFsOpDone = xSemaphoreCreateBinaryStatic(&semBuffFsOpDone);
    iterationCount = 0;
    srand(xTaskGetTickCount());

    while(1) {
        vTaskDelay(5000);
        /* Get Test Iteration Count */
        ubifs_zpl_test_debug("FsTest: Free Heap = %d, Min Free = %d", xPortGetFreeHeapSize(), xPortGetMinimumEverFreeHeapSize());
        UBI_ZPL_FileRead(iterationFile, (void*)&iterationCount, 0, 4, &actread, _FsTest_cb);
        xSemaphoreTake(semHdlFsOpDone, portMAX_DELAY);
        ubifs_zpl_test_debug("FsTest: Iteration Count = %d", iterationCount);
        iterationCount++;

        /* Create Folder */
        ubifs_zpl_test_debug("FsTest: Free Heap = %d, Min Free = %d", xPortGetFreeHeapSize(), xPortGetMinimumEverFreeHeapSize());
        ubifs_zpl_test_debug("FsTest: Creating Test Directory, %s", dirname);
        UBI_ZPL_MkDir(dirname, _FsTest_cb);
        xSemaphoreTake(semHdlFsOpDone, portMAX_DELAY);
        ubifs_zpl_test_debug("FsTest: Done");

        /* Generate Random File with Random Length */
        fileLen = rand() % MAX_FILE_SZ;
        if(fileLen == 0) fileLen++;
        ubifs_zpl_test_debug("FsTest: Generating %d random bytes", fileLen);
        for(idx = 0; idx < fileLen; idx++) {
            testDataOne[idx] = rand() % 256;
        }

        /* Write to file */
        fileOffset = (rand() % 4096) * 4096;
        ubifs_zpl_test_debug("FsTest: Free Heap = %d, Min Free = %d", xPortGetFreeHeapSize(), xPortGetMinimumEverFreeHeapSize());
        ubifs_zpl_test_debug("FsTest: Writing random data to file at random offset (len: %d, offset: %d)", fileLen, fileOffset);
        UBI_ZPL_FileWrite(testFile, (void *)testDataOne, fileOffset, fileLen, &actwritten, _FsTest_cb);
        xSemaphoreTake(semHdlFsOpDone, portMAX_DELAY);
        ubifs_zpl_test_debug("FsTest: Done (%d bytes written)", actwritten);

        /* Check file size */
        ubifs_zpl_test_debug("FsTest: Free Heap = %d, Min Free = %d", xPortGetFreeHeapSize(), xPortGetMinimumEverFreeHeapSize());
        UBI_ZPL_FileGetSize(testFile, &fileSz, _FsTest_cb);
        xSemaphoreTake(semHdlFsOpDone, portMAX_DELAY);
        ubifs_zpl_test_debug("FsTest: Size of %s: %d bytes", testFile, fileSz);

        /* Read file and compare */
        ubifs_zpl_test_debug("FsTest: Free Heap = %d, Min Free = %d", xPortGetFreeHeapSize(), xPortGetMinimumEverFreeHeapSize());
        ubifs_zpl_test_debug("FsTest: Reading from %s", testFile);
        UBI_ZPL_FileRead(testFile, (void *)testDataTwo, fileOffset, fileLen, &actread, _FsTest_cb);
        xSemaphoreTake(semHdlFsOpDone, portMAX_DELAY);
        ubifs_zpl_test_debug("FsTest: Verifying file contents");
        if(memcmp(testDataOne, testDataTwo, fileLen) == 0) {
            ubifs_zpl_test_debug("FsTest: Verification OK!");
        } else {
            ubifs_zpl_test_debug("FsTest: ERROR on Verification");
        }

        /* Delete File */
        ubifs_zpl_test_debug("FsTest: Free Heap = %d, Min Free = %d", xPortGetFreeHeapSize(), xPortGetMinimumEverFreeHeapSize());
        ubifs_zpl_test_debug("FsTest: Deleting file %s", testFile);
        UBI_ZPL_RmFile(testFile, _FsTest_cb);
        xSemaphoreTake(semHdlFsOpDone, portMAX_DELAY);
        ubifs_zpl_test_debug("FsTest: Done");

        /* Delete Folder */
        ubifs_zpl_test_debug("FsTest: Free Heap = %d, Min Free = %d", xPortGetFreeHeapSize(), xPortGetMinimumEverFreeHeapSize());
        ubifs_zpl_test_debug("FsTest: Deleting Test Directory, %s", dirname);
        UBI_ZPL_RmDir(dirname, _FsTest_cb);
        xSemaphoreTake(semHdlFsOpDone, portMAX_DELAY);
        ubifs_zpl_test_debug("FsTest: Done");

        /* Store Iteration Count */
        ubifs_zpl_test_debug("FsTest: Free Heap = %d, Min Free = %d", xPortGetFreeHeapSize(), xPortGetMinimumEverFreeHeapSize());
        UBI_ZPL_FileWrite(iterationFile, (void*)&iterationCount, 0, 4, &actwritten, _FsTest_cb);
        xSemaphoreTake(semHdlFsOpDone, portMAX_DELAY);
        ubifs_zpl_test_debug("FsTest: written updated iteration count");
    }
    vTaskSuspend(NULL);
}

void UBI_ZPL_FsTestInit(void)
{
    xTaskHandleFsTest = xTaskCreateStatic(
            _FsTest_Task,
            "FsTest",
            configTASK_STACK_FS_TEST,
            (void *)0,
            configTASK_PRIORITY_UBI_FS,
            xTaskStackFsTest,
            &xTaskFsTest);
}
#endif /* #if (ENABLE_FS_TEST == 1) */

#endif /* #if (ENABLE_UBIFS_ZPL_TEST == 1) */
