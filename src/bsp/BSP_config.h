/*! \file BSP_config.h
*
* Module Description:
* -------------------
* Configuration of Board Support Package
*
* $Author: ToanDang $
* $Date: 2018-09-24 18:38:36 +0800 (Mon, 24 Sep 2018) $
* $Revision: 74 $
*
*/

#ifndef BSP_CONFIG_H
#define BSP_CONFIG_H

//*****************************************************************************
// File dependencies.
//*****************************************************************************

//*****************************************************************************
// Public / Internal definitions.
//*****************************************************************************
#define USE_BOARD_EA_SOM    (0)   /* Using Embedded Artists' SOM board
                                   * Interface is a 200-pin SODIMM
                                   */
#define USE_BOARD_POC       (1)   /* Rev1 POC Board */

#define USE_BOARD           USE_BOARD_POC



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

#endif // End BSP_CONFIG_H
