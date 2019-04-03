/*! \file UTIL_ctrl.h
*
* Module Description:
* -------------------
* This is the interface for PID utility.
*
* $Author: sicris $
* $Date: 2018-09-17 18:52:47 +0800 (Mon, 17 Sep 2018) $
* $Revision: 57 $
* $Source: $
*
*/

#ifndef UTIL_CTRL_H
#define UTIL_CTRL_H

//*****************************************************************************
// File dependencies.
//*****************************************************************************
#include "mlib_FP.h"
#include "gflib_FP.h"

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus*/

//*****************************************************************************
// Public / Internal definitions.
//*****************************************************************************
/*!
 *  Control Record Structure. This structure contains the necessary variables
 *  used in PID control.
 */
typedef struct {
    float_t f32_reference;
    float_t f32_feedback;
    float_t f32_error;
    float_t f32_u;
    bool_t bStopIntegFlag;
    GFLIB_CTRL_PI_P_AW_T_FLT ctrlInfo;
} PI_RECORD_T;

typedef struct {
    float_t f32_reference;
    float_t f32_feedback;
    float_t f32_error;
    float_t f32_u;
    float_t f32_PGain;
    float_t f32_upperLimit;
    float_t f32_lowerLimit;
} P_RECORD_T;

//*****************************************************************************
// Public function prototypes.
//*****************************************************************************

//*****************************************************************************
//!
//! \brief This function initializes the PI control structure.
//!
//! \param  pCtrlRec  Pointer to a PI control structure.
//!
//! \return \c void
//!
//*****************************************************************************
extern void UTIL_CTRL_PI_Init(PI_RECORD_T *pCtrlRec);

//*****************************************************************************
//!
//! \brief This function performs PI operation.
//!
//! \param  pCtrlRec  Pointer to a PI control structure.
//!
//! \return \c void
//!
//*****************************************************************************
extern void UTIL_CTRL_PI_Exec(PI_RECORD_T *pCtrlRec);

//*****************************************************************************
//!
//! \brief This function initializes the P control structure.
//!
//! \param  pCtrlRec  Pointer to a P control structure.
//!
//! \return \c void
//!
//*****************************************************************************
extern void UTIL_CTRL_P_Init(P_RECORD_T *pCtrlRec);

//*****************************************************************************
//!
//! \brief This function performs P operation.
//!
//! \param  pCtrlRec  Pointer to a P control structure.
//!
//! \return \c void
//!
//*****************************************************************************
extern void UTIL_CTRL_P_Exec(P_RECORD_T *pCtrlRec);

#if defined(__cplusplus)
}
#endif /* __cplusplus*/

#endif // End UTIL_CTRL_H
