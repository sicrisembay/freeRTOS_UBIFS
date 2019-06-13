/*! \file ubifs_zpl_test.h
*
* $Author: sicrisre001 $
* $Date: 2014-01-29 20:21:46 +0800 (Wed, 29 Jan 2014) $
* $Revision: 386 $
*
*/

#ifndef UBIFS_ZPL_TEST_H
#define UBIFS_ZPL_TEST_H

#define ENABLE_UBIFS_ZPL_TEST           (1)

#if (ENABLE_UBIFS_ZPL_TEST == 1)
//*****************************************************************************
// File dependencies.
//*****************************************************************************

//*****************************************************************************
// Public / Internal definitions.
//*****************************************************************************
#define ENABLE_ENV_TEST                 (0)
#define ENABLE_FS_TEST                  (1)


//*****************************************************************************
// Public function prototypes.
//*****************************************************************************
#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus*/

#if (ENABLE_ENV_TEST == 1)
void UBI_ZPL_EnvTestInit(void);
#endif /* #if (ENABLE_ENV_TEST == 1) */

#if (ENABLE_FS_TEST == 1)
void UBI_ZPL_FsTestInit(void);
#endif /* #if (ENABLE_FS_TEST == 1) */

#if defined(__cplusplus)
}
#endif /* __cplusplus*/

#endif /* #if (ENABLE_UBIFS_ZPL_TEST == 1) */

#endif // End UBIFS_ZPL_TEST_H
