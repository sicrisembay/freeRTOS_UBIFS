#ifndef UTIL_RING_BUFFER_H
#define UTIL_RING_BUFFER_H

#include <stdint.h>

//*****************************************************************************
//
// The structure used for encapsulating all the items associated with a
// ring buffer.
//
//*****************************************************************************
typedef struct {
    //
    // The ring buffer size.
    //
    uint32_t ulSize;

    //
    // The ring buffer write index.
    //
    volatile uint32_t ulWriteIndex;

    //
    // The ring buffer read index.
    //
    volatile uint32_t ulReadIndex;

    //
    // The ring buffer.
    //
    uint8_t *pucBuf;

} tRingBufObject;

//*****************************************************************************
//
// API Function prototypes
//
//*****************************************************************************
extern uint8_t UTIL_RingBufFull(tRingBufObject *ptRingBuf);
extern uint8_t UTIL_RingBufEmpty(tRingBufObject *ptRingBuf);
extern void UTIL_RingBufFlush(tRingBufObject *ptRingBuf);
extern uint32_t UTIL_RingBufUsed(tRingBufObject *ptRingBuf);
extern uint32_t UTIL_RingBufFree(tRingBufObject *ptRingBuf);
extern uint32_t UTIL_RingBufContigUsed(tRingBufObject *ptRingBuf);
extern uint32_t UTIL_RingBufContigFree(tRingBufObject *ptRingBuf);
extern uint32_t UTIL_RingBufSize(tRingBufObject *ptRingBuf);
extern uint8_t UTIL_RingBufReadOne(tRingBufObject *ptRingBuf);
extern void UTIL_RingBufRead(tRingBufObject *ptRingBuf, uint8_t *pucData,
                        uint32_t ulLength);
extern void UTIL_RingBufWriteOne(tRingBufObject *ptRingBuf, uint8_t ucData);
extern void UTIL_RingBufWrite(tRingBufObject *ptRingBuf, uint8_t *pucData,
                         uint32_t ulLength);
extern void UTIL_RingBufAdvanceWrite(tRingBufObject *ptRingBuf,
                                uint32_t ulNumBytes);
extern void UTIL_RingBufAdvanceRead(tRingBufObject *ptRingBuf,
                                uint32_t ulNumBytes);
extern void UTIL_RingBufInit(tRingBufObject *ptRingBuf, uint8_t *pucBuf,
                        uint32_t ulSize);

//*****************************************************************************
//
// Mark the end of the C bindings section for C++ compilers.
//
//*****************************************************************************
#ifdef __cplusplus
}
#endif

#endif //  UTIL_RING_BUFFER_H
