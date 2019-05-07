#ifndef BSP_UART_H
#define BSP_UART_H

//*****************************************************************************
// File dependencies.
//*****************************************************************************
#include "stdint-gcc.h"
#include "semphr.h"

//*****************************************************************************
// Public / Internal definitions.
//*****************************************************************************
#define BSP_UART_0_TX_BUFFER_SIZE             (512)
#define BSP_UART_0_RX_BUFFER_SIZE             (512)

#define BSP_UART_1_TX_BUFFER_SIZE             (512)
#define BSP_UART_1_RX_BUFFER_SIZE             (512)

/*!
 * \enum BSP_UART_ID_T
 */
typedef enum {
    BSP_UART_ID_0,                  /*!< ID of 1st instance of UART */
    BSP_UART_ID_1,                  /*!< ID of 2nd instance of UART */

    N_BSP_UART                      /*!< Number of Uart supported */
} BSP_UART_ID_T;

/*!
 * \enum BSP_UART_RETVAL_T
 */
typedef enum {
    BSP_UART_NO_ERROR = 0,          /*! No Error */
    BSP_UART_ERROR,                 /*! Error in Task Creation */
    BSP_UART_INVALID_ARG,           /*! Invalid Argument */
    BSP_UART_MUTEX_LOCK_FAILED,     /*! Failed to Lock Mutex */
    BSP_UART_INSUFFICIENT_BUFF,     /*! Insufficient space in UART Transmit buffer */

    N_BSP_UART_RETVAL
} BSP_UART_RETVAL_T;

//*****************************************************************************
// Public function prototypes.
//*****************************************************************************
#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus*/

//*****************************************************************************
//!
//! \brief This function initializes the underlying hardware.
//!
//! \param  None
//!
//! \return Refer to BSP_UART_RETVAL_T
//!
//*****************************************************************************
extern BSP_UART_RETVAL_T BSP_UART_Init(void);

//*****************************************************************************
//!
//! \brief This function sets the BSP UART underlying hardware interrupt priority
//!
//! \param  uartId      Refer to ::BSP_UART_ID_T
//! \param  prioNum     Priority Value
//!
//! \return \c void
//!
//*****************************************************************************
extern void BSP_UART_SetIntPriority(BSP_UART_ID_T uartId, uint32_t prioNum);

//*****************************************************************************
//!
//! \brief This function registers a semaphore handle to signal that frame
//! is received.
//!
//! \param  uartId      Refer to ::BSP_UART_ID_T
//! \param  semHdlr     Semaphore Handle
//! \param  delimiter   Frame delimiter value
//!
//! \return Refer to ::BSP_UART_RETVAL_T
//!
//*****************************************************************************
extern BSP_UART_RETVAL_T BSP_UART_RegisterFrameReceiveSempahore(BSP_UART_ID_T uartId, SemaphoreHandle_t semHdlr, uint8_t delimiter);

//*****************************************************************************
//!
//! \brief This function is used to send data to underlying hardware.
//!
//! \param  uartId  Uart Instance ID (::BSP_UART_ID_T)
//! \param  pData   pointer to buffer that contains the data
//! \param  size    number of bytes to send
//!
//! \return Refer to ::BSP_UART_RETVAL_T
//!
//*****************************************************************************
extern BSP_UART_RETVAL_T BSP_UART_Send(BSP_UART_ID_T uartId,const uint8_t *pData, const uint32_t size);

//*****************************************************************************
//!
//! \brief This function is used to read bytes from underlying hardware.
//!
//! \param  uartId  Uart Instance ID (::BSP_UART_ID_T)
//! \param  pData   pointer to buffer that receives the data
//! \param  pSize   number of bytes to read.  On function return, this indicates\n
//!                 the number of bytes read.
//!
//! \return Refer to ::BSP_UART_RETVAL_T
//!
//*****************************************************************************
extern BSP_UART_RETVAL_T BSP_UART_Receive(BSP_UART_ID_T uartId, uint8_t *pData, uint32_t *pSize);

//*****************************************************************************
//!
//! \brief This function returns the number of bytes in the receive buffer.
//!
//! \param  uartId  Uart Instance ID (::BSP_UART_ID_T)
//!
//! \return Number of bytes in the receive buffer
//!
//*****************************************************************************
extern uint32_t BSP_UART_GetRcvByteCount(BSP_UART_ID_T uartId);

#if defined(__cplusplus)
}
#endif /* __cplusplus*/


#endif // End BSP_UART_H
