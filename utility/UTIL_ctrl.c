//*****************************************************************************
//!
//! \file UTIL_ctrl.c
//!
//! \brief This is the implementation of PID utility.
//!
//! $Author: sicris $
//! $Date: 2018-10-08 10:21:25 +0800 (Mon, 08 Oct 2018) $
//! $Revision: 117 $
//!
//*****************************************************************************

//*****************************************************************************
// File dependencies.
//*****************************************************************************
#include "UTIL_ctrl.h"

//*****************************************************************************
// Private definitions.
//*****************************************************************************

//*****************************************************************************
// Private member declarations.
//*****************************************************************************

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
//! \brief This function initializes the PI control structure.
//!
//! \param  pCtrlRec  Pointer to a PI control structure.
//!
//! \return \c void
//!
//*****************************************************************************
void UTIL_CTRL_PI_Init(PI_RECORD_T *pCtrlRec)
{
    if((PI_RECORD_T *)0 != pCtrlRec) {
        pCtrlRec->f32_error = 0.0f;
        pCtrlRec->f32_u = 0.0f;
        GFLIB_CtrlPIpAWInit_FLT(0.0f, &pCtrlRec->ctrlInfo);
    }
}

//*****************************************************************************
//!
//! \brief This function performs PI operation.
//!
//! \param  pCtrlRec  Pointer to a PI control structure.
//!
//! \return \c void
//!
//*****************************************************************************
void UTIL_CTRL_PI_Exec(PI_RECORD_T *pCtrlRec)
{
    if((PI_RECORD_T *)0 != pCtrlRec) {
        // calculate error
        pCtrlRec->f32_error = pCtrlRec->f32_reference - pCtrlRec->f32_feedback;
        // calculate output command
        pCtrlRec->f32_u = GFLIB_CtrlPIpAW_FLT(
                            pCtrlRec->f32_error,
                            &pCtrlRec->bStopIntegFlag,
                            &pCtrlRec->ctrlInfo);
    }
}


//*****************************************************************************
//!
//! \brief This function initializes the P control structure.
//!
//! \param  pCtrlRec  Pointer to a P control structure.
//!
//! \return \c void
//!
//*****************************************************************************
void UTIL_CTRL_P_Init(P_RECORD_T *pCtrlRec)
{
    if((P_RECORD_T *)0 != pCtrlRec) {
        pCtrlRec->f32_error = 0.0f;
        pCtrlRec->f32_u = 0.0f;
    }
}


//*****************************************************************************
//!
//! \brief This function performs P operation.
//!
//! \param  pCtrlRec  Pointer to a P control structure.
//!
//! \return \c void
//!
//*****************************************************************************
void UTIL_CTRL_P_Exec(P_RECORD_T *pCtrlRec)
{
    if((P_RECORD_T *)0 != pCtrlRec) {
        // calculate error
        pCtrlRec->f32_error = pCtrlRec->f32_reference - pCtrlRec->f32_feedback;
        // calculate output command
        pCtrlRec->f32_u = MLIB_Mul_FLT(pCtrlRec->f32_error, pCtrlRec->f32_PGain);
        // Limit Output
        if(pCtrlRec->f32_u > pCtrlRec->f32_upperLimit) {
            pCtrlRec->f32_u = pCtrlRec->f32_upperLimit;
        } else if(pCtrlRec->f32_u < pCtrlRec->f32_lowerLimit) {
            pCtrlRec->f32_u = pCtrlRec->f32_lowerLimit;
        }
    }
}


//*****************************************************************************
// Private function implementations.
//*****************************************************************************

