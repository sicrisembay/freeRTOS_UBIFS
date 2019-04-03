#include "util_ringbuf.h"
#include "cmsis_gcc.h"
#include "FreeRTOS.h"

//*****************************************************************************
//
// Define NULL, if not already defined.
//
//*****************************************************************************
#ifndef NULL
#define NULL                    ((void *)0)
#endif

#ifndef ASSERT
#define ASSERT(x)               // disable assert checks for now.
#endif

//*****************************************************************************
//
// Change the value of a variable atomically.
//
// \param pulVal points to the index whose value is to be modified.
// \param ulDelta is the number of bytes to increment the index by.
// \param ulSize is the size of the buffer the index refers to.
//
// This function is used to increment a read or write buffer index that may be
// written in various different contexts. It ensures that the read/modify/write
// sequence is not interrupted and, hence, guards against corruption of the
// variable. The new value is adjusted for buffer wrap.
//
// \return None.
//
//*****************************************************************************
static void UpdateIndexAtomic(volatile uint32_t *pulVal, uint32_t ulDelta,
                  uint32_t ulSize)
{
    uint8_t sr;
    /* Enter Critical Section */
    if(pdTRUE == xPortIsInsideInterrupt()) {
        sr = portSET_INTERRUPT_MASK_FROM_ISR();
    } else {
        portENTER_CRITICAL();
    }

    //
    // Update the variable value.
    //
    *pulVal += ulDelta;

    //
    // Correct for wrap. We use a loop here since we don't want to use a
    // modulus operation with interrupts off but we don't want to fail in
    // case ulDelta is greater than ulSize (which is extremely unlikely but...)
    //
    while(*pulVal >= ulSize)
    {
        *pulVal -= ulSize;
    }

    /* Exit Critical Section */
    if(pdTRUE == xPortIsInsideInterrupt()) {
        portCLEAR_INTERRUPT_MASK_FROM_ISR(sr);
    } else {
        portEXIT_CRITICAL();
    }
}

//*****************************************************************************
//
//! Determines whether the ring buffer whose pointers and size are provided
//! is full or not.
//!
//! \param ptRingBuf is the ring buffer object to empty.
//!
//! This function is used to determine whether or not a given ring buffer is
//! full.  The structure is specifically to ensure that we do not see
//! warnings from the compiler related to the order of volatile accesses
//! being undefined.
//!
//! \return Returns \b (uint8_t)1 if the buffer is full or \b (uint8_t)0 otherwise.
//
//*****************************************************************************
uint8_t UTIL_RingBufFull(tRingBufObject *ptRingBuf)
{
    uint32_t ulWrite;
    uint32_t ulRead;

    //
    // Check the arguments.
    //
    ASSERT(ptRingBuf != NULL);

    //
    // Copy the Read/Write indices for calculation.
    //
    ulWrite = ptRingBuf->ulWriteIndex;
    ulRead = ptRingBuf->ulReadIndex;

    //
    // Return the full status of the buffer.
    //
    return((((ulWrite + 1) % ptRingBuf->ulSize) == ulRead) ? (uint8_t)1 : (uint8_t)0);
}

//*****************************************************************************
//
//! Determines whether the ring buffer whose pointers and size are provided
//! is empty or not.
//!
//! \param ptRingBuf is the ring buffer object to empty.
//!
//! This function is used to determine whether or not a given ring buffer is
//! empty.  The structure is specifically to ensure that we do not see
//! warnings from the compiler related to the order of volatile accesses
//! being undefined.
//!
//! \return Returns \b (uint8_t)1 if the buffer is empty or \b (uint8_t)0 otherwise.
//
//*****************************************************************************
uint8_t UTIL_RingBufEmpty(tRingBufObject *ptRingBuf)
{
    uint32_t ulWrite;
    uint32_t ulRead;

    //
    // Check the arguments.
    //
    ASSERT(ptRingBuf != NULL);

    //
    // Copy the Read/Write indices for calculation.
    //
    ulWrite = ptRingBuf->ulWriteIndex;
    ulRead = ptRingBuf->ulReadIndex;

    //
    // Return the empty status of the buffer.
    //
    return((ulWrite == ulRead) ? (uint8_t)1 : (uint8_t)0);
}

//*****************************************************************************
//
//! Empties the ring buffer.
//!
//! \param ptRingBuf is the ring buffer object to empty.
//!
//! Discards all data from the ring buffer.
//!
//! \return None.
//
//*****************************************************************************
void UTIL_RingBufFlush(tRingBufObject *ptRingBuf)
{
    uint32_t isrContext = __get_IPSR() & 0x3F;
    //
    // Check the arguments.
    //
    ASSERT(ptRingBuf != NULL);

    //
    // Set the Read/Write pointers to be the same. Do this with interrupts
    // disabled to prevent the possibility of corruption of the read index.
    //
    if(isrContext == 0) {
        __disable_irq();
    }
    ptRingBuf->ulReadIndex = ptRingBuf->ulWriteIndex;
    if(isrContext == 0) {
        __enable_irq();
    }
}

//*****************************************************************************
//
//! Returns number of bytes stored in ring buffer.
//!
//! \param ptRingBuf is the ring buffer object to check.
//!
//! This function returns the number of bytes stored in the ring buffer.
//!
//! \return Returns the number of bytes stored in the ring buffer.
//
//*****************************************************************************
uint32_t UTIL_RingBufUsed(tRingBufObject *ptRingBuf)
{
    uint32_t ulWrite;
    uint32_t ulRead;

    //
    // Check the arguments.
    //
    ASSERT(ptRingBuf != NULL);

    //
    // Copy the Read/Write indices for calculation.
    //
    ulWrite = ptRingBuf->ulWriteIndex;
    ulRead = ptRingBuf->ulReadIndex;

    //
    // Return the number of bytes contained in the ring buffer.
    //
    return((ulWrite >= ulRead) ? (ulWrite - ulRead) :
           (ptRingBuf->ulSize - (ulRead - ulWrite)));
}

//*****************************************************************************
//
//! Returns number of bytes available in a ring buffer.
//!
//! \param ptRingBuf is the ring buffer object to check.
//!
//! This function returns the number of bytes available in the ring buffer.
//!
//! \return Returns the number of bytes available in the ring buffer.
//
//*****************************************************************************
uint32_t UTIL_RingBufFree(tRingBufObject *ptRingBuf)
{
    //
    // Check the arguments.
    //
    ASSERT(ptRingBuf != NULL);

    //
    // Return the number of bytes available in the ring buffer.
    //
    return((ptRingBuf->ulSize - 1) - UTIL_RingBufUsed(ptRingBuf));
}

//*****************************************************************************
//
//! Returns number of contiguous bytes of data stored in ring buffer ahead of
//! the current read pointer.
//!
//! \param ptRingBuf is the ring buffer object to check.
//!
//! This function returns the number of contiguous bytes of data available in
//! the ring buffer ahead of the current read pointer. This represents the
//! largest block of data which does not straddle the buffer wrap.
//!
//! \return Returns the number of contiguous bytes available.
//
//*****************************************************************************
uint32_t UTIL_RingBufContigUsed(tRingBufObject *ptRingBuf)
{
    uint32_t ulWrite;
    uint32_t ulRead;

    //
    // Check the arguments.
    //
    ASSERT(ptRingBuf != NULL);

    //
    // Copy the Read/Write indices for calculation.
    //
    ulWrite = ptRingBuf->ulWriteIndex;
    ulRead = ptRingBuf->ulReadIndex;

    //
    // Return the number of contiguous bytes available.
    //
    return((ulWrite >= ulRead) ? (ulWrite - ulRead) :
           (ptRingBuf->ulSize - ulRead));
}

//*****************************************************************************
//
//! Returns number of contiguous free bytes available in a ring buffer.
//!
//! \param ptRingBuf is the ring buffer object to check.
//!
//! This function returns the number of contiguous free bytes ahead of the
//! current write pointer in the ring buffer.
//!
//! \return Returns the number of contiguous bytes available in the ring
//! buffer.
//
//*****************************************************************************
uint32_t UTIL_RingBufContigFree(tRingBufObject *ptRingBuf)
{
    uint32_t ulWrite;
    uint32_t ulRead;

    //
    // Check the arguments.
    //
    ASSERT(ptRingBuf != NULL);

    //
    // Copy the Read/Write indices for calculation.
    //
    ulWrite = ptRingBuf->ulWriteIndex;
    ulRead = ptRingBuf->ulReadIndex;

    //
    // Return the number of contiguous bytes available.
    //
    if(ulRead > ulWrite)
    {
        //
        // The read pointer is above the write pointer so the amount of free
        // space is the difference between the two indices minus 1 to account
        // for the buffer full condition (write index one behind read index).
        //
        return((ulRead - ulWrite) - 1);
    }
    else
    {
        //
        // If the write pointer is above the read pointer, the amount of free
        // space is the size of the buffer minus the write index. We need to
        // add a special-case adjustment if the read index is 0 since we need
        // to leave 1 byte empty to ensure we can tell the difference between
        // the buffer being full and empty.
        //
        return(ptRingBuf->ulSize - ulWrite - ((ulRead == 0) ? 1 : 0));
    }
}

//*****************************************************************************
//
//! Return size in bytes of a ring buffer.
//!
//! \param ptRingBuf is the ring buffer object to check.
//!
//! This function returns the size of the ring buffer.
//!
//! \return Returns the size in bytes of the ring buffer.
//
//*****************************************************************************
uint32_t UTIL_RingBufSize(tRingBufObject *ptRingBuf)
{
    //
    // Check the arguments.
    //
    ASSERT(ptRingBuf != NULL);

    //
    // Return the number of bytes available in the ring buffer.
    //
    return(ptRingBuf->ulSize);
}

//*****************************************************************************
//
//! Reads a single byte of data from a ring buffer.
//!
//! \param ptRingBuf points to the ring buffer to be written to.
//!
//! This function reads a single byte of data from a ring buffer.
//!
//! \return The byte read from the ring buffer.
//
//*****************************************************************************
uint8_t UTIL_RingBufReadOne(tRingBufObject *ptRingBuf)
{
    uint8_t ucTemp;

    //
    // Check the arguments.
    //
    ASSERT(ptRingBuf != NULL);

    //
    // Verify that space is available in the buffer.
    //
    ASSERT(UTIL_RingBufUsed(ptRingBuf) != 0);

    //
    // Write the data byte.
    //
    ucTemp = ptRingBuf->pucBuf[ptRingBuf->ulReadIndex];

    //
    // Increment the read index.
    //
    UpdateIndexAtomic(&ptRingBuf->ulReadIndex, 1, ptRingBuf->ulSize);

    //
    // Return the character read.
    //
    return(ucTemp);
}

//*****************************************************************************
//
//! Reads data from a ring buffer.
//!
//! \param ptRingBuf points to the ring buffer to be read from.
//! \param pucData points to where the data should be stored.
//! \param ulLength is the number of bytes to be read.
//!
//! This function reads a sequence of bytes from a ring buffer.
//!
//! \return None.
//
//*****************************************************************************
void UTIL_RingBufRead(tRingBufObject *ptRingBuf, uint8_t *pucData,
               uint32_t ulLength)
{
    uint32_t ulTemp;

    //
    // Check the arguments.
    //
    ASSERT(ptRingBuf != NULL);
    ASSERT(pucData != NULL);
    ASSERT(ulLength != 0);

    //
    // Verify that data is available in the buffer.
    //
    ASSERT(ulLength <= UTIL_RingBufUsed(ptRingBuf));

    //
    // Read the data from the ring buffer.
    //
    for(ulTemp = 0; ulTemp < ulLength; ulTemp++)
    {
        pucData[ulTemp] = UTIL_RingBufReadOne(ptRingBuf);
    }
}

//*****************************************************************************
//
//! Remove bytes from the ring buffer by advancing the read index.
//!
//! \param ptRingBuf points to the ring buffer from which bytes are to be
//! removed.
//! \param ulNumBytes is the number of bytes to be removed from the buffer.
//!
//! This function advances the ring buffer read index by a given number of
//! bytes, removing that number of bytes of data from the buffer. If \e
//! ulNumBytes is larger than the number of bytes currently in the buffer, the
//! buffer is emptied.
//!
//! \return None.
//
//*****************************************************************************
void UTIL_RingBufAdvanceRead(tRingBufObject *ptRingBuf,
                      uint32_t ulNumBytes)
{
    uint32_t ulCount;

    //
    // Check the arguments.
    //
    ASSERT(ptRingBuf != NULL);

    //
    // Make sure that we are not being asked to remove more data than is
    // there to be removed.
    //
    ulCount = UTIL_RingBufUsed(ptRingBuf);
    ulCount =  (ulCount < ulNumBytes) ? ulCount : ulNumBytes;

    //
    // Advance the buffer read index by the required number of bytes.
    //
    UpdateIndexAtomic(&ptRingBuf->ulReadIndex, ulCount,
                      ptRingBuf->ulSize);
}

//*****************************************************************************
//
//! Add bytes to the ring buffer by advancing the write index.
//!
//! \param ptRingBuf points to the ring buffer to which bytes have been added.
//! \param ulNumBytes is the number of bytes added to the buffer.
//!
//! This function should be used by clients who wish to add data to the buffer
//! directly rather than via calls to UTIL_RingBufWrite() or UTIL_RingBufWriteOne().
//! It advances the write index by a given number of bytes.  If the \e ulNumBytes
//! parameter is larger than the amount of free space in the buffer, the
//! read pointer will be advanced to cater for the addition.  Note that this
//! will result in some of the oldest data in the buffer being discarded.
//!
//! \return None.
//
//*****************************************************************************
void UTIL_RingBufAdvanceWrite(tRingBufObject *ptRingBuf,
                       uint32_t ulNumBytes)
{
    uint32_t ulCount;
    uint32_t isrContext = __get_IPSR() & 0x3F;

    //
    // Check the arguments.
    //
    ASSERT(ptRingBuf != NULL);

    //
    // Make sure we were not asked to add a silly number of bytes.
    //
    ASSERT(ulNumBytes <= ptRingBuf->ulSize);

    //
    // Determine how much free space we currently think the buffer has.
    //
    ulCount = UTIL_RingBufFree(ptRingBuf);

    //
    // Advance the buffer write index by the required number of bytes and
    // check that we have not run past the read index. Note that we must do
    // this within a critical section (interrupts disabled) to prevent
    // race conditions that could corrupt one or other of the indices.
    //
    if(isrContext == 0) {
        __disable_irq();
    }

    //
    // Update the write pointer.
    //
    ptRingBuf->ulWriteIndex += ulNumBytes;

    //
    // Check and correct for wrap.
    //
    if(ptRingBuf->ulWriteIndex >= ptRingBuf->ulSize)
    {
        ptRingBuf->ulWriteIndex -= ptRingBuf->ulSize;
    }

    //
    // Did the client add more bytes than the buffer had free space for?
    //
    if(ulCount < ulNumBytes)
    {
        //
        // Yes - we need to advance the read pointer to ahead of the write
        // pointer to discard some of the oldest data.
        //
        ptRingBuf->ulReadIndex = ptRingBuf->ulWriteIndex + 1;

        //
        // Correct for buffer wrap if necessary.
        //
        if(ptRingBuf->ulReadIndex >= ptRingBuf->ulSize)
        {
            ptRingBuf->ulReadIndex -= ptRingBuf->ulSize;
        }
    }

    //
    // Restore interrupts if we turned them off earlier.
    //
    if(isrContext == 0) {
        __enable_irq();
    }
}

//*****************************************************************************
//
//! Writes a single byte of data to a ring buffer.
//!
//! \param ptRingBuf points to the ring buffer to be written to.
//! \param ucData is the byte to be written.
//!
//! This function writes a single byte of data into a ring buffer.
//!
//! \return None.
//
//*****************************************************************************
void UTIL_RingBufWriteOne(tRingBufObject *ptRingBuf, uint8_t ucData)
{
    //
    // Check the arguments.
    //
    ASSERT(ptRingBuf != NULL);

    //
    // Verify that space is available in the buffer.
    //
    ASSERT(UTIL_RingBufFree(ptRingBuf) != 0);

    //
    // Write the data byte.
    //
    ptRingBuf->pucBuf[ptRingBuf->ulWriteIndex] = ucData;

    //
    // Increment the write index.
    //
    UpdateIndexAtomic(&ptRingBuf->ulWriteIndex, 1, ptRingBuf->ulSize);
}

//*****************************************************************************
//
//! Writes data to a ring buffer.
//!
//! \param ptRingBuf points to the ring buffer to be written to.
//! \param pucData points to the data to be written.
//! \param ulLength is the number of bytes to be written.
//!
//! This function write a sequence of bytes into a ring buffer.
//!
//! \return None.
//
//*****************************************************************************
void UTIL_RingBufWrite(tRingBufObject *ptRingBuf, uint8_t *pucData,
                uint32_t ulLength)
{
    uint32_t ulTemp;

    //
    // Check the arguments.
    //
    ASSERT(ptRingBuf != NULL);
    ASSERT(pucData != NULL);
    ASSERT(ulLength != 0);

    //
    // Verify that space is available in the buffer.
    //
    ASSERT(ulLength <= UTIL_RingBufFree(ptRingBuf));

    //
    // Write the data into the ring buffer.
    //
    for(ulTemp = 0; ulTemp < ulLength; ulTemp++)
    {
        UTIL_RingBufWriteOne(ptRingBuf, pucData[ulTemp]);
    }
}

//*****************************************************************************
//
//! Initialize a ring buffer object.
//!
//! \param ptRingBuf points to the ring buffer to be initialized.
//! \param pucBuf points to the data buffer to be used for the ring buffer.
//! \param ulSize is the size of the buffer in bytes.
//!
//! This function initializes a ring buffer object, preparing it to store data.
//!
//! \return None.
//
//*****************************************************************************
void UTIL_RingBufInit(tRingBufObject *ptRingBuf, uint8_t *pucBuf,
               uint32_t ulSize)
{
    //
    // Check the arguments.
    //
    ASSERT(ptRingBuf != NULL);
    ASSERT(pucBuf != NULL);
    ASSERT(ulSize != 0);

    //
    // Initialize the ring buffer object.
    //
    ptRingBuf->ulSize = ulSize;
    ptRingBuf->pucBuf = pucBuf;
    ptRingBuf->ulWriteIndex = ptRingBuf->ulReadIndex = 0;
}

//*****************************************************************************
//
// Close the Doxygen group.
//! @}
//
//*****************************************************************************
