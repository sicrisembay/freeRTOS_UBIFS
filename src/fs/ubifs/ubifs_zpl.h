/*! \file ubifs_zpl.h
* Module Description:
* -------------------
* This is the application interface for UBIFS
*
* $Author: sicrisre001 $
* $Date: 2014-01-29 20:21:46 +0800 (Wed, 29 Jan 2014) $
* $Revision: 386 $
*
*/

#ifndef _UBI_FS_ZPL_
#define _UBI_FS_ZPL_

//*****************************************************************************
// File dependencies.
//*****************************************************************************
#define configTASK_PRIORITY_UBI_FS      (tskIDLE_PRIORITY + 1)

//*****************************************************************************
// Public / Internal definitions.
//*****************************************************************************

/*!
 * \page page_ubi_zpl
 * \section section_ubi_zpl_def Definition
 * \subsection subsect_ubi_zpl_ret UBI ZPL Return Definition
 * Table below shows the return definition when calling the UBI_ZPL API.
 * <table>
 * <tr><td>UBI_ZPL_NOERROR      <td>Function return without error
 * <tr><td>UBI_ZPL_INVALID_ARG  <td>Invalid Arguments
 * <tr><td>UBI_ZPL_NOT_INITED   <td>Module is not initialized
 * <tr><td>UBI_ZPL_QUEUE_FULL   <td>Request queue is full.
 * </table>
 *
 * \enum UBI_ZPL_RET_T
 */
typedef enum {
    UBI_ZPL_NOERROR = 0, /*!< Function return without Error */
    UBI_ZPL_INVALID_ARG, /*!< Invalid arguments */
    UBI_ZPL_NOT_INITED,  /*!< Module is not initialized */
    UBI_ZPL_QUEUE_FULL,  /*!< Request queue is full */

    N_UBI_ZPL_RET        /*!< Total number of defined return values */
} UBI_ZPL_RET_T;

typedef void (*ubi_zpl_cb_fcn)(int sts);

//*****************************************************************************
// Public function prototypes.
//*****************************************************************************
#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus*/

UBI_ZPL_RET_T UBI_ZPL_Init(void);

UBI_ZPL_RET_T UBI_ZPL_FileWrite(
        char * filename,
        void *buf,
        uint32_t offset,
        uint32_t size,
        uint32_t *actwritten,
        ubi_zpl_cb_fcn cb);

UBI_ZPL_RET_T UBI_ZPL_FileRead(
        char * filename,
        void *buf,
        uint32_t offset,
        uint32_t size,
        uint32_t *actread,
        ubi_zpl_cb_fcn cb);

UBI_ZPL_RET_T UBI_ZPL_FileGetSize(
        const char * filename,
        uint32_t *size,
        ubi_zpl_cb_fcn cb);

UBI_ZPL_RET_T UBI_ZPL_FileExist(
        const char * filename,
        int * bExist,
        ubi_zpl_cb_fcn cb);

UBI_ZPL_RET_T UBI_ZPL_RmFile(
        const char * filename,
        ubi_zpl_cb_fcn cb);

UBI_ZPL_RET_T UBI_ZPL_MkDir(
        const char * dirname,
        ubi_zpl_cb_fcn cb);

UBI_ZPL_RET_T UBI_ZPL_RmDir(
        const char * dirname,
        ubi_zpl_cb_fcn cb);

#if defined(__cplusplus)
}
#endif /* __cplusplus*/

#endif // End _UBI_FS_ZPL_
