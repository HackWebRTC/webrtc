/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "forward_error_correction_internal.h"
#include "fec_private_tables.h"

#include <cassert>
#include <cstring>

namespace {

// Allow for two different modes of protection for residual packets.
// The residual packets are the remaining packets beyond the important ones.
enum ResidualProtectionMode
{
    kModeNoOverlap,
    kModeOverlap,
};


/**
  * Fits an input mask (subMask) to an output mask.
  * The mask is a matrix where the rows are the FEC packets,
  * and the columns are the source packets the FEC is applied to.
  * Each row of the mask is represented by a number of mask bytes.
  *
  * \param[in]  numMaskBytes    The number of mask bytes of output mask.
  * \param[in]  numSubMaskBytes The number of mask bytes of input mask.
  * \param[in]  numRows         The number of rows of the input mask.
  * \param[in]  subMask         A pointer to hold the input mask, of size
  *                             [0, numRows * numSubMaskBytes]
  * \param[out] packetMask      A pointer to hold the output mask, of size
  *                             [0, x * numMaskBytes], where x >= numRows.
  */
void FitSubMask(const WebRtc_UWord16 numMaskBytes,
                const WebRtc_UWord16 numSubMaskBytes,
                const WebRtc_UWord16 numRows,
                const WebRtc_UWord8* subMask,
                WebRtc_UWord8* packetMask)
{
    if (numMaskBytes == numSubMaskBytes)
    {

        memcpy(packetMask,subMask,
               numRows * numSubMaskBytes);
    }
    else
    {
        for (WebRtc_UWord32 i = 0; i < numRows; i++)
        {
            WebRtc_UWord32 pktMaskIdx = i * numMaskBytes;
            WebRtc_UWord32 pktMaskIdx2 = i * numSubMaskBytes;
            for (WebRtc_UWord32 j = 0; j < numSubMaskBytes; j++)
            {
                packetMask[pktMaskIdx] = subMask[pktMaskIdx2];
                pktMaskIdx++;
                pktMaskIdx2++;
            }
        }
    }
}

/**
  * Shifts a mask by number of columns (bits), and fits it to an output mask.
  * The mask is a matrix where the rows are the FEC packets,
  * and the columns are the source packets the FEC is applied to.
  * Each row of the mask is represented by a number of mask bytes.
  *
  * \param[in]  numMaskBytes     The number of mask bytes of output mask.
  * \param[in]  numSubMaskBytes  The number of mask bytes of input mask.
  * \param[in]  numColumnShift   The number columns to be shifted, and
  *                              the starting row for the output mask.
  * \param[in]  endRow           The ending row for the output mask.
  * \param[in]  subMask          A pointer to hold the input mask, of size
  *                              [0, (endRowFEC - startRowFec) * numSubMaskBytes]
  * \param[out] packetMask       A pointer to hold the output mask, of size
  *                              [0, x * numMaskBytes], where x >= endRowFEC.
  */
// TODO (marpan): This function is doing three things at the same time:
// shift within a byte, byte shift and resizing.
// Split up into subroutines.
void ShiftFitSubMask(const WebRtc_UWord16 numMaskBytes,
                     const WebRtc_UWord16 resMaskBytes,
                     const WebRtc_UWord16 numColumnShift,
                     const WebRtc_UWord16 endRow,
                     const WebRtc_UWord8* subMask,
                     WebRtc_UWord8* packetMask)
{

    // Number of bit shifts within a byte
    const WebRtc_UWord8 numBitShifts = (numColumnShift % 8);
    const WebRtc_UWord8 numByteShifts = numColumnShift >> 3;

    // Modify new mask with sub-mask21.

    // Loop over the remaining FEC packets.
    for (WebRtc_UWord32 i = numColumnShift; i < endRow; i++)
    {
        // Byte index of new mask, for row i and column resMaskBytes,
        // offset by the number of bytes shifts
        WebRtc_UWord32 pktMaskIdx = i * numMaskBytes + resMaskBytes - 1
            + numByteShifts;
        // Byte index of subMask, for row i and column resMaskBytes
        WebRtc_UWord32 pktMaskIdx2 =
            (i - numColumnShift) * resMaskBytes + resMaskBytes - 1;

        WebRtc_UWord8  shiftRightCurrByte = 0;
        WebRtc_UWord8  shiftLeftPrevByte = 0;
        WebRtc_UWord8  combNewByte = 0;

        // Handle case of numMaskBytes > resMaskBytes:
        // For a given row, copy the rightmost "numBitShifts" bits
        // of the last byte of subMask into output mask.
        if (numMaskBytes > resMaskBytes)
        {
            shiftLeftPrevByte =
                (subMask[pktMaskIdx2] << (8 - numBitShifts));
            packetMask[pktMaskIdx + 1] = shiftLeftPrevByte;
        }

        // For each row i (FEC packet), shift the bit-mask of the subMask.
        // Each row of the mask contains "resMaskBytes" of bytes.
        // We start from the last byte of the subMask and move to first one.
        for (WebRtc_Word32 j = resMaskBytes - 1; j > 0; j--)
        {
            // Shift current byte of sub21 to the right by "numBitShifts".
            shiftRightCurrByte =
                subMask[pktMaskIdx2] >> numBitShifts;

            // Fill in shifted bits with bits from the previous (left) byte:
            // First shift the previous byte to the left by "8-numBitShifts".
            shiftLeftPrevByte =
                (subMask[pktMaskIdx2 - 1] << (8 - numBitShifts));

            // Then combine both shifted bytes into new mask byte.
            combNewByte = shiftRightCurrByte | shiftLeftPrevByte;

            // Assign to new mask.
            packetMask[pktMaskIdx] = combNewByte;
            pktMaskIdx--;
            pktMaskIdx2--;
        }
        // For the first byte in the row (j=0 case).
        shiftRightCurrByte = subMask[pktMaskIdx2] >> numBitShifts;
        packetMask[pktMaskIdx] = shiftRightCurrByte;

    }
}

} //namespace

namespace webrtc {
namespace internal {

// Residual protection for remaining packets
void ResidualPacketProtection(const WebRtc_UWord16 numMediaPackets,
                              const WebRtc_UWord16 numFecPackets,
                              const WebRtc_UWord16 numImpPackets,
                              const WebRtc_UWord16 numMaskBytes,
                              const ResidualProtectionMode mode,
                              WebRtc_UWord8* packetMask)
{
    if (mode == kModeNoOverlap)
    {
        // subMask21

        const WebRtc_UWord8 lBit =
            (numMediaPackets - numImpPackets) > 16 ? 1 : 0;

        const WebRtc_UWord16 resMaskBytes =
            (lBit == 1)? kMaskSizeLBitSet : kMaskSizeLBitClear;

        const WebRtc_UWord8* packetMaskSub21 =
            packetMaskTbl[numMediaPackets - numImpPackets - 1]
                         [numFecPackets - numImpPackets - 1];

        ShiftFitSubMask(numMaskBytes, resMaskBytes,
                        numImpPackets, numFecPackets,
                        packetMaskSub21, packetMask);
    }
    else if (mode == kModeOverlap)
    {
        // subMask22

        const WebRtc_UWord16 numFecForResidual =
            numFecPackets - numImpPackets;

        const WebRtc_UWord8* packetMaskSub22 =
            packetMaskTbl[numMediaPackets - 1]
                             [numFecForResidual - 1];

        FitSubMask(numMaskBytes, numMaskBytes,
                   numFecForResidual,
                   packetMaskSub22,
                   &packetMask[numImpPackets * numMaskBytes]);
    }
    else
    {
        assert(false);
    }

}

// Higher protection for numImpPackets
void ImportantPacketProtection(const WebRtc_UWord16 numFecPackets,
                               const WebRtc_UWord16 numImpPackets,
                               const WebRtc_UWord16 numMaskBytes,
                               WebRtc_UWord8* packetMask)
{
    const WebRtc_UWord8 lBit = numImpPackets > 16 ? 1 : 0;
    const WebRtc_UWord16 numImpMaskBytes =
    (lBit == 1)? kMaskSizeLBitSet : kMaskSizeLBitClear;

    WebRtc_UWord32 numFecForImpPackets = numImpPackets;
    if (numFecPackets < numImpPackets)
    {
        numFecForImpPackets = numFecPackets;
    }

    // Get subMask1 from table
    const WebRtc_UWord8* packetMaskSub1 =
    packetMaskTbl[numImpPackets - 1][numFecForImpPackets - 1];

    FitSubMask(numMaskBytes, numImpMaskBytes,
               numFecForImpPackets,
               packetMaskSub1,
               packetMask);

}

// Modification for UEP: reuse the tables (designed for equal protection).
// First version is to build mask from two sub-masks.
// Longer-term, may add another set of tables for UEP cases for more
// flexibility in protection between important and residual packets.

// UEP scheme:
// First subMask is for higher protection for important packets.
// Other subMask is the residual protection for remaining packets.

// Mask is characterized as (#packets_to_protect, #fec_for_protection).
// Protection defined as: (#fec_for_protection / #packets_to_protect).

// So if k = numMediaPackets, n=total#packets, (n-k)=numFecPackets,
// and m=numImpPackets, then we will have the following:

// For important packets:
// subMask1 = (m, t): protection = m/(t), where t=min(m,n-k).

// For the residual protection, we currently have two options:

// Mode 0: subMask21 = (k-m,n-k-m): protection = (n-k-m)/(k-m):
// no protection overlap between the two partitions.

// Mode 1: subMask22 = (k, n-k-m), with protection (n-k-m)/(k):
// some protection overlap between the two partitions.

void UnequalProtectionMask(const WebRtc_UWord16 numMediaPackets,
                           const WebRtc_UWord16 numFecPackets,
                           const WebRtc_UWord16 numImpPackets,
                           const WebRtc_UWord16 numMaskBytes,
                           const ResidualProtectionMode mode,
                           WebRtc_UWord8* packetMask)
{

    //
    // Generate subMask1: higher protection for numImpPackets:
    //
    ImportantPacketProtection(numFecPackets, numImpPackets,
                              numMaskBytes, packetMask);

    //
    // Generate subMask2: left-over protection (for remaining partition data),
    // if we still have some some FEC packets
    //
    if (numFecPackets > numImpPackets)
    {

        ResidualPacketProtection(numMediaPackets,numFecPackets,
                                 numImpPackets, numMaskBytes,
                                 mode, packetMask);
    }

}

void GeneratePacketMasks(const WebRtc_UWord32 numMediaPackets,
                         const WebRtc_UWord32 numFecPackets,
                         const WebRtc_UWord32 numImpPackets,
                         const bool useUnequalProtection,
                         WebRtc_UWord8* packetMask)
{
    assert(numMediaPackets <= sizeof(packetMaskTbl)/sizeof(*packetMaskTbl) &&
        numMediaPackets > 0);
    assert(numFecPackets <= numMediaPackets && numFecPackets > 0);
    assert(numImpPackets <= numMediaPackets && numImpPackets >= 0);

    WebRtc_UWord8 lBit = numMediaPackets > 16 ? 1 : 0;
    const WebRtc_UWord16 numMaskBytes =
        (lBit == 1)? kMaskSizeLBitSet : kMaskSizeLBitClear;

    // Default: use overlap mode for residual protection.
    const ResidualProtectionMode kResidualProtectionMode = kModeOverlap;

    // Force equal-protection for these cases.
    // Equal protection is also used for: (numImpPackets == 1 && numFecPackets == 1).
    // UEP=off would generally be more efficient than the UEP=on for this case.
    // TODO (marpan): check/test this condition.
    if (!useUnequalProtection || numImpPackets == 0 ||
        (numImpPackets == 1 && numFecPackets == 1))
    {
        // Retrieve corresponding mask table directly: for equal-protection case.
        // Mask = (k,n-k), with protection factor = (n-k)/k,
        // where k = numMediaPackets, n=total#packets, (n-k)=numFecPackets.
        memcpy(packetMask, packetMaskTbl[numMediaPackets - 1][numFecPackets - 1],
            numFecPackets * numMaskBytes);
    }
    else  //UEP case
    {
        UnequalProtectionMask(numMediaPackets, numFecPackets, numImpPackets,
                              numMaskBytes, kResidualProtectionMode,
                              packetMask);

    } // End of UEP modification

} //End of GetPacketMasks

}  // namespace internal
}  // namespace webrtc
