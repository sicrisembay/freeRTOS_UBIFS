//*****************************************************************************
// File dependencies.
//*****************************************************************************
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "stream_buffer.h"
#include "BSP_uart.h"
#include "fsl_gpio.h"
#include "fsl_common.h"
#include "fsl_iomuxc.h"
#include "fsl_dmamux.h"
#include "fsl_edma.h"
#include "fsl_lpuart.h"
#include "UTIL_ringbuf.h"

//*****************************************************************************
// Private definitions.
//*****************************************************************************
#define configTASK_PRIORITY_UART0           2
#define configTASK_PRIORITY_UART1           2
#define BSP_UART_PRIO                       configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY

/* UART Instance0 Peripheral Definition **************************************/
#define BSP_UART_0_BASEADDR                 LPUART1
#define BSP_UART_0_BAUDRATE                 (115200)
#define BSP_UART_0_DMAMUX_BASEADDR          DMAMUX
#define BSP_UART_0_TX_DMA_CHANNEL           1U
#define BSP_UART_0_TX_DMA_REQUEST           kDmaRequestMuxLPUART1Tx
#define BSP_UART_0_TX_DMA_BASEADDR          DMA0
#define BSP_UART_0_IRQn                     LPUART1_IRQn
#define BSP_UART_0_DMA_IRQn                 DMA1_DMA17_IRQn
#define BSP_UART_0_IRQHandler               LPUART1_IRQHandler
#define BSP_UART_0_PAD_SETTING              (0x10B0u)
/* GPIO_AD_B0_12 is configured as LPUART1_TX */
#define BSP_UART_0_TX_MUX                   IOMUXC_GPIO_AD_B0_12_LPUART1_TX
/* GPIO_AD_B0_13 is configured as LPUART1_RX */
#define BSP_UART_0_RX_MUX                   IOMUXC_GPIO_AD_B0_13_LPUART1_RX

/* UART Instance1 Peripheral Definition **************************************/
#define BSP_UART_1_BASEADDR                 LPUART2
#define BSP_UART_1_BAUDRATE                 (115200)
#define BSP_UART_1_DMAMUX_BASEADDR          DMAMUX
#define BSP_UART_1_TX_DMA_CHANNEL           4U
#define BSP_UART_1_TX_DMA_REQUEST           kDmaRequestMuxLPUART2Tx
#define BSP_UART_1_TX_DMA_BASEADDR          DMA0
#define BSP_UART_1_IRQn                     LPUART2_IRQn
#define BSP_UART_1_DMA_IRQn                 DMA4_DMA20_IRQn
#define BSP_UART_1_IRQHandler               LPUART2_IRQHandler
#define BSP_UART_1_PAD_SETTING              (0x10B0u)
/* GPIO_SD_B1_11 is configured as LPUART2_TX */
#define BSP_UART_1_TX_MUX                   IOMUXC_GPIO_SD_B1_11_LPUART2_TX
/* GPIO_SD_B1_10 is configured as LPUART2_RX */
#define BSP_UART_1_RX_MUX                   IOMUXC_GPIO_SD_B1_10_LPUART2_RX

#define BSP_UART_0_TX_PACKET_MAX_LEN        (64)
#define BSP_UART_1_TX_PACKET_MAX_LEN        (64)

#define BSP_UART_TX_DONE_BIT                (0x01)
#define BSP_UART_TX_DATA_RDY_BIT            (0x02)

typedef enum {
    BSP_UART_TX_STATE_IDLE = 0,
    BSP_UART_TX_STATE_BUSY,

    N_BSP_UART_TX_STATE
} BSP_UART_TX_STATE_T;

/* Forward declaration of the handle typedef. */
typedef struct BSP_UART_TX_DMA_HANDLE BSP_UART_TX_DMA_HANDLE_T;

typedef void (*dbgTx_dma_transfer_callback_t)(LPUART_Type *base,
                                                BSP_UART_TX_DMA_HANDLE_T *handle,
                                                status_t status,
                                                void *userData);

struct BSP_UART_TX_DMA_HANDLE
{
    void *userData;                             /*!< callback function parameter.*/
    size_t txDataSizeAll;                       /*!< Size of the data to send out. */
    edma_handle_t *txEdmaHandle;                /*!< The eDMA TX channel used. */
    uint8_t nbytes;                             /*!< eDMA minor byte transfer count
                                                 * initially configured. */
    BSP_UART_ID_T uartInstance;                 /*!< UART Instance */
    volatile BSP_UART_TX_STATE_T txState;       /*!< TX transfer state. */
};

typedef struct {
    LPUART_Type *base;
    BSP_UART_TX_DMA_HANDLE_T *handle;
} BSP_UART_TX_DMA_PRIVATE_HANDLE_T;


//*****************************************************************************
// Private member declarations.
//*****************************************************************************
static uint8_t bInit[N_BSP_UART] = {0};

static uint8_t tx0Buffer[BSP_UART_0_TX_PACKET_MAX_LEN];
static BSP_UART_TX_DMA_HANDLE_T uart0EdmaHandle;
static edma_handle_t uart0TxEdmaHandle;
static BSP_UART_TX_DMA_PRIVATE_HANDLE_T uart0PrivateEdmaHandle;

static uint8_t tx1Buffer[BSP_UART_1_TX_PACKET_MAX_LEN];
static BSP_UART_TX_DMA_HANDLE_T uart1EdmaHandle;
static edma_handle_t uart1TxEdmaHandle;
static BSP_UART_TX_DMA_PRIVATE_HANDLE_T uart1PrivateEdmaHandle;


/* UART Instance0 Gatekeeper Task */
static StaticTask_t xTaskUART0;
static StackType_t xTaskStackUART0[256];
static TaskHandle_t xTaskHandleUART0 = NULL;

/* UART Instance1 Gatekeeper Task */
static StaticTask_t xTaskUART1;
static StackType_t xTaskStackUART1[256];
static TaskHandle_t xTaskHandleUART1 = NULL;

/* Semaphore Handle for Frame Received */
static SemaphoreHandle_t xSemFrameRcv[N_BSP_UART] = {NULL};
static uint8_t delimiterValue[N_BSP_UART] = {0};

/* Mutex used in UART Instance0 */
static SemaphoreHandle_t xMutexTx0 = NULL;
static StaticSemaphore_t xMutexTx0Buffer;
static SemaphoreHandle_t xMutexRx0 = NULL;
static StaticSemaphore_t xMutexRx0Buffer;

/* Mutex used in UART Instance1 */
static SemaphoreHandle_t xMutexTx1 = NULL;
static StaticSemaphore_t xMutexTx1Buffer;
static SemaphoreHandle_t xMutexRx1 = NULL;
static StaticSemaphore_t xMutexRx1Buffer;

/* Transmit and Receive Ring Buffer used in UART Instance0*/
static tRingBufObject rbRx0Buffer;
static uint8_t rbRx0BufSto[BSP_UART_0_RX_BUFFER_SIZE];
static tRingBufObject rbTx0Buffer;
static uint8_t rbTx0BufSto[BSP_UART_0_TX_BUFFER_SIZE];

/* Transmit and Receive Ring Buffer used in UART Instance1*/
static tRingBufObject rbRx1Buffer;
static uint8_t rbRx1BufSto[BSP_UART_1_RX_BUFFER_SIZE];
static tRingBufObject rbTx1Buffer;
static uint8_t rbTx1BufSto[BSP_UART_1_TX_BUFFER_SIZE];

//*****************************************************************************
// Public / Internal member external declarations.
//*****************************************************************************

//*****************************************************************************
// Private function prototypes.
//*****************************************************************************
static void _UART0_Task(void *pxParam);
static void _UART1_Task(void *pxParam);
static uint32_t _UART_GetSrcFreq(void);
static void _UART0_SendEDMACallback(
                    edma_handle_t *handle,
                    void *param,
                    bool transferDone,
                    uint32_t tcds
                    );
static void _UART1_SendEDMACallback(
                    edma_handle_t *handle,
                    void *param,
                    bool transferDone,
                    uint32_t tcds
                    );

//*****************************************************************************
// Public function implementations
//*****************************************************************************

//*****************************************************************************
//!
//! \brief This function initializes the underlying hardware.
//!
//! \param  None
//!
//! \return Refer to ::BSP_UART_RETVAL_T
//!
//*****************************************************************************
BSP_UART_RETVAL_T BSP_UART_Init(void)
{
    BSP_UART_RETVAL_T retval = BSP_UART_NO_ERROR;

    if(0 == bInit[BSP_UART_ID_0]) {
        // Configure Pin Routing
        CLOCK_EnableClock(kCLOCK_Iomuxc);
        // UART Instance0
        IOMUXC_SetPinMux(BSP_UART_0_TX_MUX, 0U);
        IOMUXC_SetPinConfig(BSP_UART_0_TX_MUX, BSP_UART_0_PAD_SETTING);
        IOMUXC_SetPinMux(BSP_UART_0_RX_MUX, 0U);
        IOMUXC_SetPinConfig(BSP_UART_0_RX_MUX, BSP_UART_0_PAD_SETTING);

        /* Create Gatekeeper Tasks */
        if(NULL == xTaskHandleUART0) {
            xTaskHandleUART0 = xTaskCreateStatic(
                    _UART0_Task,
                    "UART0",
                    sizeof(xTaskStackUART0) / sizeof(xTaskStackUART0[0]),
                    (void *)0,
                    configTASK_PRIORITY_UART0,
                    xTaskStackUART0,
                    &xTaskUART0);
            if(NULL == xTaskHandleUART0) {
                retval = BSP_UART_ERROR;
            } else {
                xMutexRx0 = xSemaphoreCreateMutexStatic(&xMutexRx0Buffer);
                xMutexTx0 = xSemaphoreCreateMutexStatic(&xMutexTx0Buffer);
                /* Initialize Ring Buffer */
                UTIL_RingBufInit(&rbRx0Buffer, rbRx0BufSto, BSP_UART_0_RX_BUFFER_SIZE);
                UTIL_RingBufInit(&rbTx0Buffer, rbTx0BufSto, BSP_UART_0_TX_BUFFER_SIZE);
            }
        }
    }

    if((0 == bInit[BSP_UART_ID_1]) && (retval == BSP_UART_NO_ERROR)){
        // Configure Pin Routing
        CLOCK_EnableClock(kCLOCK_Iomuxc);
        // UART Instance1
        IOMUXC_SetPinMux(BSP_UART_1_TX_MUX, 0U);
        IOMUXC_SetPinConfig(BSP_UART_1_TX_MUX, BSP_UART_1_PAD_SETTING);
        IOMUXC_SetPinMux(BSP_UART_1_RX_MUX, 0U);
        IOMUXC_SetPinConfig(BSP_UART_1_RX_MUX, BSP_UART_1_PAD_SETTING);

        /* Create Gatekeeper Tasks */
        if(NULL == xTaskHandleUART1) {
            xTaskHandleUART1 = xTaskCreateStatic(
                    _UART1_Task,
                    "UART1",
                    sizeof(xTaskStackUART1) / sizeof(xTaskStackUART1[0]),
                    (void *)0,
                    configTASK_PRIORITY_UART1,
                    xTaskStackUART1,
                    &xTaskUART1);
            if(NULL == xTaskHandleUART1) {
                retval = BSP_UART_ERROR;
            } else {
                xMutexRx1 = xSemaphoreCreateMutexStatic(&xMutexRx1Buffer);
                xMutexTx1 = xSemaphoreCreateMutexStatic(&xMutexTx1Buffer);
                /* Initialize Ring Buffer */
                UTIL_RingBufInit(&rbRx1Buffer, rbRx1BufSto, BSP_UART_1_RX_BUFFER_SIZE);
                UTIL_RingBufInit(&rbTx1Buffer, rbTx1BufSto, BSP_UART_1_TX_BUFFER_SIZE);
            }
        }
    }

    BSP_UART_SetIntPriority(BSP_UART_ID_0, BSP_UART_PRIO);
    BSP_UART_SetIntPriority(BSP_UART_ID_1, BSP_UART_PRIO);

    return(retval);
}


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
void BSP_UART_SetIntPriority(BSP_UART_ID_T uartId, uint32_t prioNum)
{
    if(uartId < N_BSP_UART) {
        switch(uartId) {
        case BSP_UART_ID_0:
            NVIC_SetPriority(BSP_UART_0_IRQn, prioNum);
            NVIC_SetPriority(BSP_UART_0_DMA_IRQn, prioNum);
            break;
        case BSP_UART_ID_1:
            NVIC_SetPriority(BSP_UART_1_IRQn, prioNum);
            NVIC_SetPriority(BSP_UART_1_DMA_IRQn, prioNum);
            break;
        default:
            break;
        }
    }
}


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
BSP_UART_RETVAL_T BSP_UART_RegisterFrameReceiveSempahore(BSP_UART_ID_T uartId, SemaphoreHandle_t semHdlr, uint8_t delimiter)
{
    BSP_UART_RETVAL_T retval = BSP_UART_NO_ERROR;
    if(!(uartId < N_BSP_UART) || (semHdlr == NULL)) {
        retval = BSP_UART_INVALID_ARG;
    } else {
        xSemFrameRcv[uartId] = semHdlr;
        delimiterValue[uartId] = delimiter;
    }
    return (retval);
}


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
BSP_UART_RETVAL_T BSP_UART_Send(BSP_UART_ID_T uartId, const uint8_t *pData, const uint32_t size)
{
    BSP_UART_RETVAL_T retval = BSP_UART_NO_ERROR;
    BaseType_t xHigherPriorityTaskWoken;
    tRingBufObject *pRbTxBuffer;
    SemaphoreHandle_t mutexTx = NULL;
    BSP_UART_TX_DMA_HANDLE_T *pUartEdmaHandle;
    TaskHandle_t taskHandleUart = NULL;

    if((pData == (uint8_t*)0) || !(uartId < N_BSP_UART)) {
        retval = BSP_UART_INVALID_ARG;
    } else {
        xHigherPriorityTaskWoken = pdFALSE;

        switch(uartId) {
        case BSP_UART_ID_0:
            pRbTxBuffer = &rbTx0Buffer;
            mutexTx = xMutexTx0;
            pUartEdmaHandle = &uart0EdmaHandle;
            taskHandleUart = xTaskHandleUART0;
            break;
        case BSP_UART_ID_1:
            pRbTxBuffer = &rbTx1Buffer;
            mutexTx = xMutexTx1;
            pUartEdmaHandle = &uart1EdmaHandle;
            taskHandleUart = xTaskHandleUART1;
            break;
        default:
            break;
        }

        if(UTIL_RingBufFree(pRbTxBuffer) >= size) {
            /* Get Mutex */
            if(pdTRUE == xPortIsInsideInterrupt()) {
                /* Execution is in ISR Context */
                if(pdTRUE == xSemaphoreTakeFromISR(mutexTx, &xHigherPriorityTaskWoken)) {
                    /* Write to Trasmit Ring Buffer */
                    if(size > 0) {
                        UTIL_RingBufWrite(pRbTxBuffer, (uint8_t*)pData, (uint32_t)size);
                        if(pUartEdmaHandle->txState == BSP_UART_TX_STATE_IDLE) {
                            /* Trigger Transmit */
                            xTaskNotifyFromISR(taskHandleUart, BSP_UART_TX_DATA_RDY_BIT, eSetBits, &xHigherPriorityTaskWoken);
                        }
                    }
                    /* Release Mutex */
                    xSemaphoreGiveFromISR(mutexTx, &xHigherPriorityTaskWoken);
                    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
                } else {
                    retval = BSP_UART_MUTEX_LOCK_FAILED;
                }
            } else {
                /* User Context */
                if(pdTRUE == xSemaphoreTake(mutexTx, 20)) {
                    /* Write to Trasmit Ring Buffer */
                    if(size > 0) {
                        UTIL_RingBufWrite(pRbTxBuffer, (uint8_t*)pData, (uint32_t)size);
                        if(pUartEdmaHandle->txState == BSP_UART_TX_STATE_IDLE) {
                            /* Trigger Transmit */
                            xTaskNotify(taskHandleUart, BSP_UART_TX_DATA_RDY_BIT, eSetBits);
                        }
                    }
                    /* Release Mutex */
                    xSemaphoreGive(mutexTx);
                } else {
                    retval = BSP_UART_MUTEX_LOCK_FAILED;
                }
            }
        } else {
            retval = BSP_UART_INSUFFICIENT_BUFF;
        }
    }
    return (retval);
}

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
BSP_UART_RETVAL_T BSP_UART_Receive(BSP_UART_ID_T uartId, uint8_t *pData, uint32_t *pSize)
{
    BSP_UART_RETVAL_T retval = BSP_UART_NO_ERROR;
    uint32_t count = 0;
    tRingBufObject *pRbRxBuffer = (tRingBufObject *)0;

    if((pData == NULL) || (pSize == NULL) ||
       !(uartId < N_BSP_UART)) {
        retval = BSP_UART_INVALID_ARG;
    } else {
        switch(uartId) {
        case BSP_UART_ID_0:
            pRbRxBuffer = &rbRx0Buffer;
            break;
        case BSP_UART_ID_1:
            pRbRxBuffer = &rbRx1Buffer;
            break;
        default:
            pRbRxBuffer = &rbRx0Buffer;
            break;
        }
        while ((count < *pSize) && (UTIL_RingBufUsed(pRbRxBuffer) > 0)) {
            *pData = UTIL_RingBufReadOne(pRbRxBuffer);
            count++;
            pData++;
        }
        *pSize = count;
    }
    return(retval);
}


//*****************************************************************************
//!
//! \brief This function returns the number of bytes in the receive buffer.
//!
//! \param  uartId  Uart Instance ID (::BSP_UART_ID_T)
//!
//! \return Number of bytes in the receive buffer
//!
//*****************************************************************************
uint32_t BSP_UART_GetRcvByteCount(BSP_UART_ID_T uartId)
{
    uint32_t retval = 0;
    if(uartId < N_BSP_UART) {
        switch(uartId) {
        case BSP_UART_ID_0:
            retval = UTIL_RingBufUsed(&rbRx0Buffer);
            break;
        case BSP_UART_ID_1:
            retval = UTIL_RingBufUsed(&rbRx1Buffer);
            break;
        default:
            break;
        }
    }
    return(retval);
}


//*****************************************************************************
// Private function implementations.
//*****************************************************************************
static void _UART0_Task(void *pxParam)
{
    uint32_t u32NotificationValue;
    uint32_t nByteAvailable;
    uint32_t u32TempData;

    edma_transfer_config_t xferConfig;
    lpuart_config_t lp_uart_config;

    /* Configure UART */
    LPUART_GetDefaultConfig(&lp_uart_config);
    lp_uart_config.baudRate_Bps = BSP_UART_0_BAUDRATE;
    LPUART_Init(BSP_UART_0_BASEADDR, &lp_uart_config, _UART_GetSrcFreq());
    /* Empty the Data Register */
    while(BSP_UART_0_BASEADDR->STAT & LPUART_STAT_RDRF_MASK) {
        u32TempData = BSP_UART_0_BASEADDR->DATA;
        (void)u32TempData;
    }
    /* Enable Interrupt */
    LPUART_EnableInterrupts(BSP_UART_0_BASEADDR, kLPUART_RxDataRegFullInterruptEnable);
    /* Enable Rx and Tx */
    LPUART_EnableRx(BSP_UART_0_BASEADDR, true);
    LPUART_EnableTx(BSP_UART_0_BASEADDR, true);
    /* Set channel for LPUART */
    DMAMUX_SetSource(BSP_UART_0_DMAMUX_BASEADDR, BSP_UART_0_TX_DMA_CHANNEL, BSP_UART_0_TX_DMA_REQUEST);
    DMAMUX_EnableChannel(BSP_UART_0_DMAMUX_BASEADDR, BSP_UART_0_TX_DMA_CHANNEL);
    /* Init the EDMA module */
    EDMA_CreateHandle(&uart0TxEdmaHandle, BSP_UART_0_TX_DMA_BASEADDR, BSP_UART_0_TX_DMA_CHANNEL);
    uart0PrivateEdmaHandle.base = BSP_UART_0_BASEADDR;
    uart0PrivateEdmaHandle.handle = &uart0EdmaHandle;
    uart0EdmaHandle.txState = BSP_UART_TX_STATE_IDLE;
    uart0EdmaHandle.txEdmaHandle = &uart0TxEdmaHandle;
    uart0EdmaHandle.userData = NULL;
    EDMA_SetCallback(uart0EdmaHandle.txEdmaHandle, _UART0_SendEDMACallback, &uart0PrivateEdmaHandle);
    EnableIRQ(BSP_UART_0_IRQn);

    bInit[BSP_UART_ID_0] = 1;

    while(1) {
        switch(uart0EdmaHandle.txState) {
            case BSP_UART_TX_STATE_IDLE: {
                if(UTIL_RingBufEmpty(&rbTx0Buffer)) {
                    /* Nothing to Transmit.. Wait for Notification */
                    if(pdPASS == xTaskNotifyWait(
                            pdFALSE,        /* Don't clear bits on entry */
                            0xFFFFFFFF,     /* Clear all bits on exit */
                            &u32NotificationValue,
                            10)) {
                    }
                } else {
                    /* Get bytes from Ring Buffer */
                    nByteAvailable = UTIL_RingBufUsed(&rbTx0Buffer);
                    if(nByteAvailable > BSP_UART_0_TX_PACKET_MAX_LEN) {
                        nByteAvailable = BSP_UART_0_TX_PACKET_MAX_LEN;
                    }
                    UTIL_RingBufRead(&rbTx0Buffer, tx0Buffer, nByteAvailable);

                    /* Prepare DMA Transfer */
                    uart0EdmaHandle.txState = BSP_UART_TX_STATE_BUSY;
                    uart0EdmaHandle.txDataSizeAll = nByteAvailable;
                    uart0EdmaHandle.nbytes = sizeof(uint8_t);
                    EDMA_PrepareTransfer(
                                    &xferConfig,
                                    (void *)tx0Buffer,
                                    sizeof(uint8_t),
                                    (void *)LPUART_GetDataRegisterAddress(BSP_UART_0_BASEADDR),
                                    sizeof(uint8_t),
                                    sizeof(uint8_t),
                                    nByteAvailable,
                                    kEDMA_MemoryToPeripheral
                                    );
                    /* Submit Transfer */
                    if(kStatus_Success == EDMA_SubmitTransfer(uart0EdmaHandle.txEdmaHandle, &xferConfig)) {
                        /* Start transfer */
                        EDMA_StartTransfer(uart0EdmaHandle.txEdmaHandle);
                        /* Enable UART TX DMA */
                        LPUART_EnableTxDMA(BSP_UART_0_BASEADDR, true);
                    } else {
                        uart0EdmaHandle.txState = BSP_UART_TX_STATE_IDLE;
                    }
                }
                break;
            }
            case BSP_UART_TX_STATE_BUSY: {
                if(pdPASS == xTaskNotifyWait(
                                            pdFALSE,        /* Don't clear bits on entry */
                                            0xFFFFFFFF,     /* Clear all bits on exit */
                                            &u32NotificationValue,
                                            10)) {
                }
                break;
            }
            default:
                break;
        }
    }
}

static void _UART1_Task(void *pxParam)
{
    uint32_t u32NotificationValue;
    uint32_t nByteAvailable;
    uint32_t u32TempData;

    edma_transfer_config_t xferConfig;
    lpuart_config_t lp_uart_config;

    /* Configure UART */
    LPUART_GetDefaultConfig(&lp_uart_config);
    lp_uart_config.baudRate_Bps = BSP_UART_1_BAUDRATE;
    LPUART_Init(BSP_UART_1_BASEADDR, &lp_uart_config, _UART_GetSrcFreq());
    /* Empty the Data Register */
    while(BSP_UART_1_BASEADDR->STAT & LPUART_STAT_RDRF_MASK) {
        u32TempData = BSP_UART_1_BASEADDR->DATA;
        (void)u32TempData;
    }
    /* Enable Interrupt */
    LPUART_EnableInterrupts(BSP_UART_1_BASEADDR, kLPUART_RxDataRegFullInterruptEnable);
    /* Enable Rx and Tx */
    LPUART_EnableRx(BSP_UART_1_BASEADDR, true);
    LPUART_EnableTx(BSP_UART_1_BASEADDR, true);
    /* Set channel for LPUART */
    DMAMUX_SetSource(BSP_UART_1_DMAMUX_BASEADDR, BSP_UART_1_TX_DMA_CHANNEL, BSP_UART_1_TX_DMA_REQUEST);
    DMAMUX_EnableChannel(BSP_UART_1_DMAMUX_BASEADDR, BSP_UART_1_TX_DMA_CHANNEL);
    /* Init the EDMA module */
    EDMA_CreateHandle(&uart1TxEdmaHandle, BSP_UART_1_TX_DMA_BASEADDR, BSP_UART_1_TX_DMA_CHANNEL);
    uart1PrivateEdmaHandle.base = BSP_UART_1_BASEADDR;
    uart1PrivateEdmaHandle.handle = &uart1EdmaHandle;
    uart1EdmaHandle.txState = BSP_UART_TX_STATE_IDLE;
    uart1EdmaHandle.txEdmaHandle = &uart1TxEdmaHandle;
    uart1EdmaHandle.userData = NULL;
    EDMA_SetCallback(uart1EdmaHandle.txEdmaHandle, _UART1_SendEDMACallback, &uart1PrivateEdmaHandle);
    EnableIRQ(BSP_UART_1_IRQn);

    bInit[BSP_UART_ID_1] = 1;

    while(1) {
        switch(uart1EdmaHandle.txState) {
            case BSP_UART_TX_STATE_IDLE: {
                if(UTIL_RingBufEmpty(&rbTx1Buffer)) {
                    /* Nothing to Transmit.. Wait for Notification */
                    if(pdPASS == xTaskNotifyWait(
                            pdFALSE,        /* Don't clear bits on entry */
                            0xFFFFFFFF,     /* Clear all bits on exit */
                            &u32NotificationValue,
                            10)) {
                    }
                } else {
                    /* Get bytes from Ring Buffer */
                    nByteAvailable = UTIL_RingBufUsed(&rbTx1Buffer);
                    if(nByteAvailable > BSP_UART_1_TX_PACKET_MAX_LEN) {
                        nByteAvailable = BSP_UART_1_TX_PACKET_MAX_LEN;
                    }
                    UTIL_RingBufRead(&rbTx1Buffer, tx1Buffer, nByteAvailable);

                    /* Prepare DMA Transfer */
                    uart1EdmaHandle.txState = BSP_UART_TX_STATE_BUSY;
                    uart1EdmaHandle.txDataSizeAll = nByteAvailable;
                    uart1EdmaHandle.nbytes = sizeof(uint8_t);
                    EDMA_PrepareTransfer(
                                    &xferConfig,
                                    (void *)tx1Buffer,
                                    sizeof(uint8_t),
                                    (void *)LPUART_GetDataRegisterAddress(BSP_UART_1_BASEADDR),
                                    sizeof(uint8_t),
                                    sizeof(uint8_t),
                                    nByteAvailable,
                                    kEDMA_MemoryToPeripheral
                                    );
                    /* Submit Transfer */
                    if(kStatus_Success == EDMA_SubmitTransfer(uart1EdmaHandle.txEdmaHandle, &xferConfig)) {
                        /* Start transfer */
                        EDMA_StartTransfer(uart1EdmaHandle.txEdmaHandle);
                        /* Enable UART TX DMA */
                        LPUART_EnableTxDMA(BSP_UART_1_BASEADDR, true);
                    } else {
                        uart1EdmaHandle.txState = BSP_UART_TX_STATE_IDLE;
                    }
                }
                break;
            }
            case BSP_UART_TX_STATE_BUSY: {
                if(pdPASS == xTaskNotifyWait(
                                            pdFALSE,        /* Don't clear bits on entry */
                                            0xFFFFFFFF,     /* Clear all bits on exit */
                                            &u32NotificationValue,
                                            10)) {
                }
                break;
            }
            default:
                break;
        }
    }
}

static uint32_t _UART_GetSrcFreq(void)
{
    uint32_t freq;

    /* To make it simple, we assume default PLL and divider settings, and the only variable
       from application is use PLL3 source or OSC source */
    if (CLOCK_GetMux(kCLOCK_UartMux) == 0) /* PLL3 div6 80M */
    {
        freq = (CLOCK_GetPllFreq(kCLOCK_PllUsb1) / 6U) / (CLOCK_GetDiv(kCLOCK_UartDiv) + 1U);
    }
    else
    {
        freq = CLOCK_GetOscFreq() / (CLOCK_GetDiv(kCLOCK_UartDiv) + 1U);
    }

    return freq;
}

static void _UART0_SendEDMACallback(edma_handle_t *handle, void *param, bool transferDone, uint32_t tcds)
{
    assert(param);
    BaseType_t xHigherPriorityTaskWoken;
    BSP_UART_TX_DMA_PRIVATE_HANDLE_T *pxPrivateHandle = (BSP_UART_TX_DMA_PRIVATE_HANDLE_T *)param;

    /* Avoid the warning for unused variables. */
    handle = handle;
    tcds = tcds;

    if (transferDone)
    {
        if((pxPrivateHandle->base != NULL) && (pxPrivateHandle->handle != NULL)) {
            /* Disable LPUART TX EDMA and Stop Transfer */
            LPUART_EnableTxDMA(pxPrivateHandle->base, false);
            EDMA_AbortTransfer(pxPrivateHandle->handle->txEdmaHandle);
            pxPrivateHandle->handle->txState = BSP_UART_TX_STATE_IDLE;
            xHigherPriorityTaskWoken = pdFALSE;
            xTaskNotifyFromISR(xTaskHandleUART0, BSP_UART_TX_DONE_BIT, eSetBits, &xHigherPriorityTaskWoken);
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        }
    }
}

static void _UART1_SendEDMACallback(edma_handle_t *handle, void *param, bool transferDone, uint32_t tcds)
{
    assert(param);
    BaseType_t xHigherPriorityTaskWoken;
    BSP_UART_TX_DMA_PRIVATE_HANDLE_T *pxPrivateHandle = (BSP_UART_TX_DMA_PRIVATE_HANDLE_T *)param;

    /* Avoid the warning for unused variables. */
    handle = handle;
    tcds = tcds;

    if (transferDone)
    {
        if((pxPrivateHandle->base != NULL) && (pxPrivateHandle->handle != NULL)) {
            /* Disable LPUART TX EDMA and Stop Transfer */
            LPUART_EnableTxDMA(pxPrivateHandle->base, false);
            EDMA_AbortTransfer(pxPrivateHandle->handle->txEdmaHandle);
            pxPrivateHandle->handle->txState = BSP_UART_TX_STATE_IDLE;
            xHigherPriorityTaskWoken = pdFALSE;
            xTaskNotifyFromISR(xTaskHandleUART1, BSP_UART_TX_DONE_BIT, eSetBits, &xHigherPriorityTaskWoken);
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        }
    }
}


void BSP_UART_0_IRQHandler(void)
{
    uint8_t rxData;
    BaseType_t xHigherPriorityTaskWoken;
    while(kLPUART_RxDataRegFullFlag & LPUART_GetStatusFlags(BSP_UART_0_BASEADDR)) {
        rxData = (uint8_t)(BSP_UART_0_BASEADDR->DATA & 0x00FF);
        if(UTIL_RingBufFree(&rbRx0Buffer) > 0) {
            UTIL_RingBufWriteOne(&rbRx0Buffer, rxData);
        } else {
            /* Overwrite oldest data */
            UTIL_RingBufReadOne(&rbRx0Buffer);
            UTIL_RingBufWriteOne(&rbRx0Buffer, rxData);
        }
        if(delimiterValue[BSP_UART_ID_0] == rxData) {
            if(xSemFrameRcv[BSP_UART_ID_0] != NULL) {
                xHigherPriorityTaskWoken = pdFALSE;
                xSemaphoreGiveFromISR(xSemFrameRcv[BSP_UART_ID_0], &xHigherPriorityTaskWoken);
                portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
            }
        }
    }

    if(kLPUART_RxOverrunFlag & LPUART_GetStatusFlags(BSP_UART_0_BASEADDR)) {
        LPUART_ClearStatusFlags(BSP_UART_0_BASEADDR, kLPUART_RxOverrunFlag);
    }

    /* Add empty instructions for correct interrupt flag clearing */
    __DSB();
    __ISB();
}

void BSP_UART_1_IRQHandler(void)
{
    uint8_t rxData;
    BaseType_t xHigherPriorityTaskWoken;
    while(kLPUART_RxDataRegFullFlag & LPUART_GetStatusFlags(BSP_UART_1_BASEADDR)) {
        rxData = (uint8_t)(BSP_UART_1_BASEADDR->DATA & 0x00FF);
        if(UTIL_RingBufFree(&rbRx1Buffer) > 0) {
            UTIL_RingBufWriteOne(&rbRx1Buffer, rxData);
        } else {
            /* Overwrite oldest data */
            UTIL_RingBufReadOne(&rbRx1Buffer);
            UTIL_RingBufWriteOne(&rbRx1Buffer, rxData);
        }
        if(delimiterValue[BSP_UART_ID_1] == rxData) {
            if(xSemFrameRcv[BSP_UART_ID_1] != NULL) {
                xHigherPriorityTaskWoken = pdFALSE;
                xSemaphoreGiveFromISR(xSemFrameRcv[BSP_UART_ID_1], &xHigherPriorityTaskWoken);
                portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
            }
        }
    }

    if(kLPUART_RxOverrunFlag & LPUART_GetStatusFlags(BSP_UART_1_BASEADDR)) {
        LPUART_ClearStatusFlags(BSP_UART_1_BASEADDR, kLPUART_RxOverrunFlag);
    }

    /* Add empty instructions for correct interrupt flag clearing */
    __DSB();
    __ISB();
}


