/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_H263_INFORMATION_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_H263_INFORMATION_H_

#include "typedefs.h"

#include "video_codec_information.h"

#define MAX_NUMBER_OF_H263_GOB 32  // 5 bits

namespace webrtc {
class H263Info
{
public:
    H263Info()
        :
        uiH263PTypeFmt(0),
        codecBits(0),
        pQuant(0),
        numOfGOBs(0),
        totalNumOfMBs(0),
        cpmBit(0),
        fType(0)
        {
            memset(ptrGOBbuffer,     0, sizeof(ptrGOBbuffer));
            memset(ptrGOBbufferSBit, 0, sizeof(ptrGOBbufferSBit));
            memset(ptrGQuant,        0, sizeof(ptrGQuant));
            memset(ptrNumOfMBs,      0, sizeof(ptrNumOfMBs));
            memset(ptrGroupNum,      0, sizeof(ptrGroupNum));
        }

    WebRtc_Word32 CalculateMBOffset(const WebRtc_UWord8 numOfGOB) const;

    WebRtc_UWord8     uiH263PTypeFmt;     // Defines frame size
    WebRtc_UWord8     codecBits;
    WebRtc_UWord8     pQuant;
    WebRtc_UWord8     numOfGOBs;          // Total number of GOBs of current frame

    WebRtc_UWord16    totalNumOfMBs;
    WebRtc_UWord8     cpmBit;
    WebRtc_UWord8     fType;              // 0 - intra frame, 1 - inter frame

    WebRtc_UWord16    ptrNumOfMBs[MAX_NUMBER_OF_H263_GOB];        // Total number of MBs of current GOB
    WebRtc_UWord32    ptrGOBbuffer[MAX_NUMBER_OF_H263_GOB];       // GOB buffer (start byte of GOBs)
    WebRtc_UWord8     ptrGroupNum[MAX_NUMBER_OF_H263_GOB];
    WebRtc_UWord8     ptrGOBbufferSBit[MAX_NUMBER_OF_H263_GOB];   // sBit buffer (number of start bits to ignore for corresponding GOB)
    WebRtc_UWord8     ptrGQuant[MAX_NUMBER_OF_H263_GOB];          // quantizer information for GOBs
};

struct H263MBInfo
{
    H263MBInfo()
        :
        bufferSize(0),
        ptrBuffer(0),
        ptrBufferHMV(0),
        ptrBufferVMV(0)
        {
        }

    WebRtc_UWord32          bufferSize;     // Size of MB buffer
    WebRtc_UWord32*         ptrBuffer;      // MB buffer
    WebRtc_UWord8*          ptrBufferHMV;   // Horizontal motion vector for corresponding MB
    WebRtc_UWord8*          ptrBufferVMV;   // Vertical motion vector for corresponding MB
};

class H263Information : public VideoCodecInformation
{
public:
    H263Information();
    ~H263Information();

    /*******************************************************************************
    * void Reset();
    *
    * Resets the members to zero.
    *
    */
    virtual void Reset();

    virtual RtpVideoCodecTypes Type();

   /*******************************************************************************
    * WebRtc_Word32 GetInfo(WebRtc_UWord8* ptrEncodedBuffer,
    *             WebRtc_UWord32 length,
    *             const H263Info*& ptrInfo);
    *
    * Gets information from an encoded stream.
    *
    * Input:
    *          - ptrEncodedBuffer  : PoWebRtc_Word32er to encoded stream.
    *          - length            : Length in bytes of encoded stream.
    *
    * Output:
    *          - ptrInfo           : PoWebRtc_Word32er to struct with H263 info.
    *
    * Return value:
    *          - 0                 : ok
    *          - (-1)              : Error
    */
    virtual WebRtc_Word32 GetInfo(const WebRtc_UWord8* ptrEncodedBuffer,
                                const WebRtc_UWord32 length,
                                const H263Info*& ptrInfo);

    /*******************************************************************************
    * WebRtc_Word32 GetMBInfo(const WebRtc_UWord8* ptrEncodedBuffer,
    *               WebRtc_UWord32 length,
    *               WebRtc_Word32 numOfGOB,
    *               const H263MBInfo*& ptrInfoMB);
    *
    * Gets macroblock positions for a GOB.
    * Also, the horizontal and vertical motion vector for each MB are returned.
    *
    * Input:
    *          - ptrEncodedBuffer     : Pointer to encoded stream.
    *          - length               : Length in bytes of encoded stream.
    *          - numOfGOB             : Group number of current GOB.
    *
    * Output:
    *          - ptrInfoMB            : Pointer to struct with MB positions in bits for a GOB.
    *                                   Horizontal and vertical motion vector for each MB.
    *
    * Return value:
    *          - 0                    : ok
    *          - (-1)                 : Error
    */
    WebRtc_Word32 GetMBInfo(const WebRtc_UWord8* ptrEncodedBuffer,
                  const WebRtc_UWord32 length,
                  const WebRtc_UWord8 numOfGOB,
                  const H263MBInfo*& ptrInfoMB);

protected:
    bool HasInfo(const WebRtc_UWord32 length);
    WebRtc_Word32  FindInfo(const WebRtc_UWord8* ptrEncodedBuffer, const WebRtc_UWord32 length);

    bool PictureStartCode();
    WebRtc_Word32  FindPTypeFMT();
    void FindFType();
    void FindCodecBits();
    void FindPQUANT();
    void FindCPMbit();
    WebRtc_Word32 SetNumOfMBs();

    WebRtc_Word32  FindGOBs(const WebRtc_UWord32 length);

    // MB info
    WebRtc_Word32  VerifyAndAllocateMB();
    bool HasMBInfo(const WebRtc_UWord8 numOfGOB);
    WebRtc_Word32  FindMBs(const WebRtc_UWord8* ptrEncodedBuffer,
                         const WebRtc_UWord8 numOfGOB,
                         const WebRtc_UWord32 length);

    void FindGQUANT(WebRtc_Word32 numOfGOB);
    WebRtc_Word32  FindMCBPC(WebRtc_Word32 &mbType, char *cbp);
    WebRtc_Word32  FindCBPY(WebRtc_Word32 mbType, char *cbp);
    WebRtc_Word32  FindMVD(WebRtc_Word32 numOfMB, WebRtc_Word32 verORhor, WebRtc_UWord8 *hmv1, WebRtc_UWord8 *vmv1);
    WebRtc_Word32  FindTCOEF(WebRtc_Word32 &last);
    bool IsGBSC();
    WebRtc_UWord8 IsBitOne(const WebRtc_Word32 bitCnt) const;
    void ByteAlignData(WebRtc_Word32 numOfBytes);
    void OutputBits(WebRtc_Word32 length);

private:
    WebRtc_Word32           _bitCnt;
    const WebRtc_UWord8*    _ptrData;
    WebRtc_UWord8           _dataShifted[5];

    H263Info          _info;
    H263MBInfo        _infoMB;
};
} // namespace webrtc

#endif // WEBRTC_MODULES_RTP_RTCP_SOURCE_H263_INFORMATION_H_
