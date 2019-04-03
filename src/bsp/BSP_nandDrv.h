/*! \file BSP_nandDrv.h
*
* Module Description:
* -------------------
* This is the application interface for NAND Flash Driver
*
* $Author: toan $
* $Date: Oct 2, 2018 $
* $Revision:  $
*
*/

#ifndef BSP_NANDDRV_H_
#define BSP_NANDDRV_H_

//*****************************************************************************
// File dependencies.
//*****************************************************************************
#include "stdint-gcc.h"
#include "BSP_config.h"
#include "fsl_semc.h"

//*****************************************************************************
// Public / Internal definitions.
//*****************************************************************************

//! \enum BSP_NAND_RET_T
typedef enum {
    BSP_NAND_SUCCESS = 0,    				//!< Value: 0, No Error
	BSP_NAND_GENERIC_FAIL, 					//!< Value: 1, Operation failed without more details
	BSP_NAND_BAD_PARAM_PAGE, 				//!< Value: 2, Content of the parameter page is invalid
	BSP_NAND_INVALID_ADDRESS, 				//!< Value: 3, Address is invalid
	BSP_NAND_INVALID_LENGTH, 				//!< Value: 4, Length is invalid
	BSP_NAND_ERASE_FAILED, 					//!< Value: 5, ERASE failed
	BSP_NAND_ERASE_FAILED_WRITE_PROTECT,	//!< Value: 6, ERASE failed due to write protect
	BSP_NAND_PROGRAM_FAILED,				//!< Value: 7, PROGRAME failed
	BSP_NAND_PROGRAM_FAILED_WRITE_PROTECT,  //!< Value: 8, PROGRAME failed due to write protect
	BSP_NAND_READ_FAILED,					//!< Value: 9, READ failed
	BSP_NAND_TIMEOUT,						//!< Value: 10, The operation ended due to timeout
	BSP_NAND_BAD_PARAM,                     //!< Value: 11, Invalid input parameters
	BSP_NAND_INIT_ERROR,                    //!< Value: 12, NAND FLash initialization failed

    N_BSP_NAND_DRV_RET
} BSP_NAND_RET_T;

//*****************************************************************************
// Public function prototypes.
//*****************************************************************************
#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus*/

//*****************************************************************************
//!
//! \brief This function initializes the NAND Flash
//!
//! \param  None
//!
//! \return \c ::BSP_NAND_GENERIC_FAIL  Failed in NAND Flash initialization
//! \return \c ::BSP_NAND_SUCCESS       Success in NAND Flash initialization
//!
//*****************************************************************************
extern BSP_NAND_RET_T BSP_NAND_Init(void);

//*****************************************************************************
//!
//! \brief Returns the RDY/BSY Pin Status of the NAND Flash
//!
//! \param  None
//!
//! \return \c true when Ready, false when Busy
//!
//*****************************************************************************
extern bool BSP_NAND_Ready(void);


//*****************************************************************************
//!
//! \brief Returns the status value of the NAND Flash
//!
//! \param  None
//!
//! \return Value of status register
//!
//*****************************************************************************
extern uint8_t BSP_NAND_Read_Status(void);


//*****************************************************************************
//!
//! \brief Resets NAND Flash
//!
//! \param  None
//!
//! \return \c void
//!
//*****************************************************************************
extern void BSP_NAND_Reset(void);


//*****************************************************************************
//!
//! \brief Reads the NAND flash ID
//!
//! \param  buf     Buffer the ID information is written
//!
//! \return \c void
//!
//*****************************************************************************
extern void BSP_NAND_ReadID(uint8_t *buf);

//*****************************************************************************
//!
//! \brief Perform a PAGE READ (Data + OOB) operation
//!
//! \param  pageAddress     Page address
//! \param  buf             Read buffer
//!
//! \return \c void
//!
//*****************************************************************************
extern void BSP_NAND_ReadPageDataOOB(uint32_t pageAddress, uint8_t *buf);


//*****************************************************************************
//!
//! \brief Perform a BLOCK ERASE operation
//!
//! \param  command     NAND Flash Command ID
//! \param  page_addr   Page address
//!
//! \return void
//!
//*****************************************************************************
extern void BSP_NAND_Erase(uint8_t command, int32_t page_addr);

//*****************************************************************************
//!
//! \brief Perform a PAGE PROGRAM operation
//!
//! \param  page_addr       Page address
//! \param  column          Column address
//! \param  len             Size of buffer
//! \param  buf             Writing buffer
//!
//! \return \c void
//!
//*****************************************************************************
extern void BSP_NAND_ProgramPage(int32_t page_addr, int32_t column, uint32_t len, uint8_t *buf);

#if defined(__cplusplus)
}
#endif /* __cplusplus*/



#endif /* BSP_NANDDRV_H_ */
