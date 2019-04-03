/*! \file template.h
*
* Module Description:
* -------------------
* This is the application interface for ....
*
* $Author: sicrisre001 $
* $Date: 2014-01-29 20:21:46 +0800 (Wed, 29 Jan 2014) $
* $Revision: 386 $
*
*/

#ifndef _UBI_FS_CONFIG_
#define _UBI_FS_CONFIG_

//*****************************************************************************
// File dependencies.
//*****************************************************************************

//*****************************************************************************
// Public / Internal definitions.
//*****************************************************************************
#define NAND_TOSHIBA                                (0)
#define NAND_WINBOND                                (1)
#define NAND_PN                                     NAND_WINBOND

#define CONFIG_SYS_MAX_NAND_DEVICE                  (1)
#define CONFIG_SYS_NAND_MAX_CHIPS                   (1)
#define CONFIG_SYS_NAND_BASE                        (0x00000000)
/* number of pages per block */
#define CONFIG_SYS_NAND_PAGE_COUNT                  (64)
/* number of main bytes in NAND page */
#define CONFIG_SYS_NAND_PAGE_SIZE                   (2048)
/* number of OOB bytes in NAND page */
#define CONFIG_SYS_NAND_OOBSIZE                     (64)
/* number of bytes in NAND erase-block */
#define CONFIG_SYS_NAND_BLOCK_SIZE                  (CONFIG_SYS_NAND_PAGE_COUNT * CONFIG_SYS_NAND_PAGE_SIZE)
/* data bytes per ECC step */
#define CONFIG_SYS_NAND_ECCSIZE                     (512)
/* ECC bytes per step */
#define CONFIG_SYS_NAND_ECCBYTES                    (5)

#define CONFIG_SYS_NAND_BLOCK_COUNT                 (1024)
#define CONFIG_SYS_NAND_PLANE_COUNT                 (1)
#define CONFIG_SYS_NAND_SIZE                        (CONFIG_SYS_NAND_BLOCK_SIZE * CONFIG_SYS_NAND_BLOCK_COUNT * CONFIG_SYS_NAND_PLANE_COUNT)
#define CONFIG_SYS_NAND_SIZE_KB                     (CONFIG_SYS_NAND_SIZE >> 10)

#define CONFIG_MTD_PARTITIONS
#define CONFIG_MTD_DEVICE
#define MTDIDS_DEFAULT                              "nand0=gpmi-nand"
#define MTDPARTS_DEFAULT                            "mtdparts=gpmi-nand:" \
                                                    "512k(bcb),"          \
                                                    "2m(u-boot1)ro,"      \
                                                    "2m(u-boot2)ro,"      \
                                                    "-(rootfs)"
#define PARTITION_DEFAULT                           "nand0,3"
#define PARTITION_NAME_DEFAULT                      "rootfs"
#define VOLUME_NAME_DEFAULT                         "ubi0:fs"

#define CONFIG_MTD_UBI_WL_THRESHOLD                 (256)

#define CONFIG_SYS_LOAD_ADDR                        (0x20200000)

//*****************************************************************************
// Public function prototypes.
//*****************************************************************************
#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus*/

/* Insert here the function prototypes */

#if defined(__cplusplus)
}
#endif /* __cplusplus*/

#endif // End _UBI_FS_CONFIG_
