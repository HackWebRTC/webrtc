/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "system_wrappers/interface/cpu_features_wrapper.h"
#include "test_util.h"
#include "test_macros.h"
#include "rtp_dump.h"
#include <cmath>

using namespace webrtc;

/******************************
 *  VCMEncodeCompleteCallback
 *****************************/
// Basic callback implementation
// passes the encoded frame directly to the encoder
// Packetization callback implmentation
VCMEncodeCompleteCallback::VCMEncodeCompleteCallback(FILE* encodedFile):
    _encodedFile(encodedFile),
    _encodedBytes(0),
    _VCMReceiver(NULL),
    _seqNo(0),
    _encodeComplete(false),
    _width(0),
    _height(0),
    _codecType(kRTPVideoNoVideo)
{
    //
}
VCMEncodeCompleteCallback::~VCMEncodeCompleteCallback()
{
}

void
VCMEncodeCompleteCallback::RegisterTransportCallback(
                                            VCMPacketizationCallback* transport)
{
}

WebRtc_Word32
VCMEncodeCompleteCallback::SendData(
        const FrameType frameType,
        const WebRtc_UWord8  payloadType,
        const WebRtc_UWord32 timeStamp,
        const WebRtc_UWord8* payloadData,
        const WebRtc_UWord32 payloadSize,
        const RTPFragmentationHeader& fragmentationHeader,
        const webrtc::RTPVideoTypeHeader* videoTypeHdr)
{
    // will call the VCMReceiver input packet
    _frameType = frameType;
    // writing encodedData into file
    fwrite(payloadData, 1, payloadSize, _encodedFile);
    WebRtcRTPHeader rtpInfo;
    rtpInfo.header.markerBit = true; // end of frame
    rtpInfo.type.Video.isFirstPacket = true;
    rtpInfo.type.Video.codec = _codecType;
    switch (_codecType)
    {
    case webrtc::kRTPVideoH263:
        rtpInfo.type.Video.codecHeader.H263.bits = false;
        rtpInfo.type.Video.codecHeader.H263.independentlyDecodable = false;
        rtpInfo.type.Video.height = (WebRtc_UWord16)_height;
        rtpInfo.type.Video.width = (WebRtc_UWord16)_width;
        break;
    case webrtc::kRTPVideoVP8:
        break;
    default:
        assert(false);
        return -1;
    }

    rtpInfo.header.payloadType = payloadType;
    rtpInfo.header.sequenceNumber = _seqNo++;
    rtpInfo.header.ssrc = 0;
    rtpInfo.header.timestamp = timeStamp;
    rtpInfo.frameType = frameType;
    // Size should also be received from that table, since the payload type
    // defines the size.

    _encodedBytes += payloadSize;
    // directly to receiver
    // TODO(hlundin): Remove assert once we've piped PictureID into VCM
    // through the WebRtcRTPHeader.
    assert(rtpInfo.type.Video.codec != kRTPVideoVP8);
    _VCMReceiver->IncomingPacket(payloadData, payloadSize, rtpInfo);
    _encodeComplete = true;

    return 0;
}

 float
 VCMEncodeCompleteCallback::EncodedBytes()
 {
     return _encodedBytes;
 }

bool
VCMEncodeCompleteCallback::EncodeComplete()
{
    if (_encodeComplete)
    {
        _encodeComplete = false;
        return true;
    }
    return false;
}

void
VCMEncodeCompleteCallback::Initialize()
{
    _encodeComplete = false;
    _encodedBytes = 0;
    _seqNo = 0;
    return;
}

void
VCMEncodeCompleteCallback::ResetByteCount()
{
    _encodedBytes = 0;
}

/***********************************/
/*   VCMRTPEncodeCompleteCallback  */
/***********************************/
// Encode Complete callback implementation
// passes the encoded frame via the RTP module to the decoder
// Packetization callback implmentation

WebRtc_Word32
VCMRTPEncodeCompleteCallback::SendData(
        const FrameType frameType,
        const WebRtc_UWord8  payloadType,
        const WebRtc_UWord32 timeStamp,
        const WebRtc_UWord8* payloadData,
        const WebRtc_UWord32 payloadSize,
        const RTPFragmentationHeader& fragmentationHeader,
        const webrtc::RTPVideoTypeHeader* videoTypeHdr)
{
    _frameType = frameType;
    _encodedBytes+= payloadSize;
    _encodeComplete = true;
    //printf("encoded = %d Bytes\n", payloadSize);
    return _RTPModule->SendOutgoingData(frameType,
                                        payloadType,
                                        timeStamp,
                                        payloadData,
                                        payloadSize,
                                        &fragmentationHeader,
                                        videoTypeHdr);
}

 float
 VCMRTPEncodeCompleteCallback::EncodedBytes()
 {
     // only good for one call  - after which will reset value;
     float tmp = _encodedBytes;
     _encodedBytes = 0;
     return tmp;
 }

bool
VCMRTPEncodeCompleteCallback::EncodeComplete()
{
    if (_encodeComplete)
    {
        _encodeComplete = false;
        return true;
    }
    return false;
}

// Decoded Frame Callback Implmentation

 WebRtc_Word32
 VCMDecodeCompleteCallback::FrameToRender(VideoFrame& videoFrame)
 {
      fwrite(videoFrame.Buffer(), 1, videoFrame.Length(), _decodedFile);
     _decodedBytes+= videoFrame.Length();
     // keeping last decoded frame
     _lastDecodedFrame.VerifyAndAllocate(videoFrame.Size());
     _lastDecodedFrame.CopyFrame(videoFrame.Size(), videoFrame.Buffer());
     _lastDecodedFrame.SetHeight(videoFrame.Height());
     _lastDecodedFrame.SetWidth(videoFrame.Width());
     _lastDecodedFrame.SetTimeStamp(videoFrame.TimeStamp());

    return VCM_OK;
 }

int
VCMDecodeCompleteCallback::PSNRLastFrame(const VideoFrame& sourceFrame,  double *YPSNRptr)
{
    double mse = 0.0;
    double mseLogSum = 0.0;

    WebRtc_Word32 frameBytes = sourceFrame.Height() * sourceFrame.Width(); // only Y
    WebRtc_UWord8 *ref = sourceFrame.Buffer();
    if (_lastDecodedFrame.Height() == 0)
    {
        *YPSNRptr = 0;
        return 0; // no new decoded frames
    }
    WebRtc_UWord8 *test  = _lastDecodedFrame.Buffer();
    for( int k = 0; k < frameBytes; k++ )
    {
            mse += (test[k] - ref[k]) * (test[k] - ref[k]);
    }

    // divide by number of pixels
    mse /= (double) (frameBytes);

    // accumulate for total average
    mseLogSum += std::log10( mse );

    *YPSNRptr = 20.0 * std::log10(255.0) - 10.0 * mseLogSum; // for only 1 frame

    _lastDecodedFrame.Free();
    _lastDecodedFrame.SetHeight(0);
    return 0;
}

WebRtc_Word32
VCMDecodeCompleteCallback::DecodedBytes()
{
    return _decodedBytes;
}

RTPSendCompleteCallback::RTPSendCompleteCallback(RtpRtcp* rtp, const char* filename):
    _sendCount(0),
    _rtp(rtp),
    _lossPct(0),
    _burstLength(0),
    _prevLossState(0),
    _rtpDump(NULL)
{
    if (filename != NULL)
    {
        _rtpDump = RtpDump::CreateRtpDump();
        _rtpDump->Start(filename);
    }
}
RTPSendCompleteCallback::~RTPSendCompleteCallback()
{
    if (_rtpDump != NULL)
    {
        _rtpDump->Stop();
        RtpDump::DestroyRtpDump(_rtpDump);
    }
}
int
RTPSendCompleteCallback::SendPacket(int channel, const void *data, int len)
{
    _sendCount++;

    // Packet Loss

    if (_burstLength <= 1.0)
    {
        // Random loss: if _burstLength parameter is not set, or <=1
        if (PacketLoss(_lossPct))
        {
            // drop
            //printf("\tDrop packet, sendCount = %d\n", _sendCount);
            return len;
        }
    }
    else
    {
        // Simulate bursty channel (Gilbert model)
        // (1st order) Markov chain model with memory of the previous/last
        // packet state (loss or received)

        // 0 = received state
        // 1 = loss state

        // probTrans10: if previous packet is lost, prob. to -> received state
        // probTrans11: if previous packet is lost, prob. to -> loss state

        // probTrans01: if previous packet is received, prob. to -> loss state
        // probTrans00: if previous packet is received, prob. to -> received

        // Map the two channel parameters (average loss rate and burst length)
        // to the transition probabilities:
        double probTrans10 = 100 * (1.0 / _burstLength);
        double probTrans11 = (100.0 - probTrans10);
        double probTrans01 = (probTrans10 * ( _lossPct / (100.0 - _lossPct) ) );

        // Note: Random loss (Bernoulli) model is a special case where:
        // burstLength = 100.0 / (100.0 - _lossPct) (i.e., p10 + p01 = 100)

        if (_prevLossState == 0 )
        {
            // previous packet was received
            if (PacketLoss(probTrans01) )
            {
                // drop, update previous state to loss
                _prevLossState = 1;
                return len;
            }
        }
        else if (_prevLossState == 1)
        {
            // previous packet was lost
            if (PacketLoss(probTrans11) )
            {
                // drop, update previous state to loss
                _prevLossState = 1;
                return len;
            }
        }
        // no drop, update previous state to received
        _prevLossState = 0;
    }

    if (_rtpDump != NULL)
    {
        if (_rtpDump->DumpPacket((const WebRtc_UWord8*)data, len) != 0)
        {
            return -1;
        }
    }
    if(_rtp->IncomingPacket((const WebRtc_UWord8*)data, len) == 0)
    {
        return len;
    }
    return -1;
    }

int
RTPSendCompleteCallback::SendRTCPPacket(int channel, const void *data, int len)
{
    if(_rtp->IncomingPacket((const WebRtc_UWord8*)data, len) == 0)
    {
        return len;
    }
    return -1;
}

void
RTPSendCompleteCallback::SetLossPct(double lossPct)
{
    _lossPct = lossPct;
    return;
}

void
RTPSendCompleteCallback::SetBurstLength(double burstLength)
{
    _burstLength = burstLength;
    return;
}

bool
RTPSendCompleteCallback::PacketLoss(double lossPct)
{
    double randVal = (std::rand() + 1.0)/(RAND_MAX + 1.0);
    return randVal < lossPct/100;
}

WebRtc_Word32
PacketRequester::ResendPackets(const WebRtc_UWord16* sequenceNumbers, WebRtc_UWord16 length)
{
    return _rtp.SendNACK(sequenceNumbers, length);
}

WebRtc_Word32
PSNRfromFiles(const WebRtc_Word8 *refFileName, const WebRtc_Word8 *testFileName, WebRtc_Word32 width, WebRtc_Word32 height, double *YPSNRptr)
{
    FILE *refFp = fopen(refFileName, "rb");
    if( refFp == NULL ) {
        // cannot open reference file
        fprintf(stderr, "Cannot open file %s\n", refFileName);
        return -1;
    }

    FILE *testFp = fopen(testFileName, "rb");
    if( testFp == NULL ) {
        // cannot open test file
        fprintf(stderr, "Cannot open file %s\n", testFileName);
        return -2;
    }

    double mse = 0.0;
    double mseLogSum = 0.0;
    WebRtc_Word32 frames = 0;

    WebRtc_Word32 frameBytes = 3*width*height/2; // bytes in one frame I420
    WebRtc_UWord8 *ref = new WebRtc_UWord8[frameBytes]; // space for one frame I420
    WebRtc_UWord8 *test = new WebRtc_UWord8[frameBytes]; // space for one frame I420

    WebRtc_Word32 refBytes = (WebRtc_Word32) fread(ref, 1, frameBytes, refFp);
    WebRtc_Word32 testBytes = (WebRtc_Word32) fread(test, 1, frameBytes, testFp);

    while( refBytes == frameBytes && testBytes == frameBytes )
    {
        mse = 0.0;

        int sh = 8;//boundary offset
        for( int k2 = sh; k2 < height-sh;k2++)
        for( int k = sh; k < width-sh;k++)
        {
            int kk = k2*width + k;
            mse += (test[kk] - ref[kk]) * (test[kk] - ref[kk]);
        }

        // divide by number of pixels
          mse /= (double) (width * height);

        // accumulate for total average
        mseLogSum += std::log10( mse );
        frames++;

        refBytes = (int) fread(ref, 1, frameBytes, refFp);
        testBytes = (int) fread(test, 1, frameBytes, testFp);
    }
     // for identical reproduction:
    if (mse == 0)
    {
        *YPSNRptr = 48;
    }
    else
    {
        *YPSNRptr = 20.0 * std::log10(255.0) - 10.0 * mseLogSum / frames;
    }


    delete [] ref;
    delete [] test;

    fclose(refFp);
    fclose(testFp);

    return 0;
}

static double
similarity
(
    unsigned long sum_s,
    unsigned long sum_r,
    unsigned long sum_sq_s,
    unsigned long sum_sq_r,
    unsigned long sum_sxr,
    int count
)
{
    int64_t ssim_n, ssim_d;
    int64_t c1, c2;
    const int64_t cc1 =  26634; // (64^2*(.01*255)^2
    const int64_t cc2 = 239708; // (64^2*(.03*255)^2

    //scale the constants by number of pixels
    c1 = (cc1*count*count)>>12;
    c2 = (cc2*count*count)>>12;

    ssim_n = (2*sum_s*sum_r+ c1)*((int64_t) 2*count*sum_sxr-
          (int64_t) 2*sum_s*sum_r+c2);

    ssim_d = (sum_s*sum_s +sum_r*sum_r+c1)*
        ((int64_t)count*sum_sq_s-(int64_t)sum_s*sum_s +
        (int64_t)count*sum_sq_r-(int64_t) sum_r*sum_r +c2) ;

    return ssim_n * 1.0 / ssim_d;
}

static double
ssim_8x8_c
(
    unsigned char *s,
    int sp,
    unsigned char *r,
    int rp
)
{
    unsigned long sum_s    = 0;
    unsigned long sum_r    = 0;
    unsigned long sum_sq_s = 0;
    unsigned long sum_sq_r = 0;
    unsigned long sum_sxr  = 0;

    int i,j;
    for(i=0;i<8;i++,s+=sp,r+=rp)
    {
        for(j=0;j<8;j++)
        {
            sum_s += s[j];
            sum_r += r[j];
            sum_sq_s += s[j] * s[j];
            sum_sq_r += r[j] * r[j];
            sum_sxr += s[j] * r[j];
        }
    }

    return similarity(sum_s, sum_r, sum_sq_s, sum_sq_r, sum_sxr, 64);
}

#if defined(WEBRTC_USE_SSE2)
#include <xmmintrin.h>
static double
ssim_8x8_sse2
(
    unsigned char *s,
    int sp,
    unsigned char *r,
    int rp
)
{
    int i;
    const __m128i z     = _mm_setzero_si128();
    __m128i sum_s_16    = _mm_setzero_si128();
    __m128i sum_r_16    = _mm_setzero_si128();
    __m128i sum_sq_s_32 = _mm_setzero_si128();
    __m128i sum_sq_r_32 = _mm_setzero_si128();
    __m128i sum_sxr_32  = _mm_setzero_si128();

    for(i=0;i<8;i++,s+=sp,r+=rp)
    {
        const __m128i s_8 = _mm_loadl_epi64((__m128i*)(s));
        const __m128i r_8 = _mm_loadl_epi64((__m128i*)(r));

        const __m128i s_16 = _mm_unpacklo_epi8(s_8,z);
        const __m128i r_16 = _mm_unpacklo_epi8(r_8,z);

        sum_s_16 = _mm_adds_epu16(sum_s_16, s_16);
        sum_r_16 = _mm_adds_epu16(sum_r_16, r_16);
        const __m128i sq_s_32 = _mm_madd_epi16(s_16, s_16);
        sum_sq_s_32 = _mm_add_epi32(sum_sq_s_32, sq_s_32);
        const __m128i sq_r_32 = _mm_madd_epi16(r_16, r_16);
        sum_sq_r_32 = _mm_add_epi32(sum_sq_r_32, sq_r_32);
        const __m128i sxr_32 = _mm_madd_epi16(s_16, r_16);
        sum_sxr_32 = _mm_add_epi32(sum_sxr_32, sxr_32);
    }

    const __m128i sum_s_32  = _mm_add_epi32(_mm_unpackhi_epi16(sum_s_16, z),
                                            _mm_unpacklo_epi16(sum_s_16, z));
    const __m128i sum_r_32  = _mm_add_epi32(_mm_unpackhi_epi16(sum_r_16, z),
                                            _mm_unpacklo_epi16(sum_r_16, z));

    unsigned long sum_s_64[2];
    unsigned long sum_r_64[2];
    unsigned long sum_sq_s_64[2];
    unsigned long sum_sq_r_64[2];
    unsigned long sum_sxr_64[2];

    _mm_store_si128 ((__m128i*)sum_s_64,
                      _mm_add_epi64(_mm_unpackhi_epi32(sum_s_32, z),
                                    _mm_unpacklo_epi32(sum_s_32, z)));
    _mm_store_si128 ((__m128i*)sum_r_64,
                      _mm_add_epi64(_mm_unpackhi_epi32(sum_r_32, z),
                                    _mm_unpacklo_epi32(sum_r_32, z)));
    _mm_store_si128 ((__m128i*)sum_sq_s_64,
                      _mm_add_epi64(_mm_unpackhi_epi32(sum_sq_s_32, z),
                                    _mm_unpacklo_epi32(sum_sq_s_32, z)));
    _mm_store_si128 ((__m128i*)sum_sq_r_64,
                      _mm_add_epi64(_mm_unpackhi_epi32(sum_sq_r_32, z),
                                    _mm_unpacklo_epi32(sum_sq_r_32, z)));
    _mm_store_si128 ((__m128i*)sum_sxr_64,
                      _mm_add_epi64(_mm_unpackhi_epi32(sum_sxr_32, z),
                                    _mm_unpacklo_epi32(sum_sxr_32, z)));

    const unsigned long sum_s    = sum_s_64[0]    + sum_s_64[1];
    const unsigned long sum_r    = sum_r_64[0]    + sum_r_64[1];
    const unsigned long sum_sq_s = sum_sq_s_64[0] + sum_sq_s_64[1];
    const unsigned long sum_sq_r = sum_sq_r_64[0] + sum_sq_r_64[1];
    const unsigned long sum_sxr  = sum_sxr_64[0]  + sum_sxr_64[1];

    return similarity(sum_s, sum_r, sum_sq_s, sum_sq_r, sum_sxr, 64);
}
#endif

double
SSIM_frame
(
    unsigned char *img1,
    unsigned char *img2,
    int stride_img1,
    int stride_img2,
    int width,
    int height)
{
    int i,j;
    unsigned int samples = 0;
    double ssim_total = 0;
    double (*ssim_8x8)(unsigned char*, int, unsigned char*, int rp);

    ssim_8x8 = ssim_8x8_c;
    if(WebRtc_GetCPUInfo(kSSE2))
    {
#if defined(WEBRTC_USE_SSE2)
        ssim_8x8 = ssim_8x8_sse2;
#endif
    }

    // sample point start with each 4x4 location
    for(i=0; i < height-8; i+=4, img1 += stride_img1*4, img2 += stride_img2*4)
    {
        for(j=0; j < width-8; j+=4 )
        {
            double v = ssim_8x8(img1+j, stride_img1, img2+j, stride_img2);
            ssim_total += v;
            samples++;
        }
    }
    ssim_total /= samples;
    return ssim_total;
}

WebRtc_Word32
SSIMfromFiles(const WebRtc_Word8 *refFileName, const WebRtc_Word8 *testFileName, WebRtc_Word32 width, WebRtc_Word32 height, double *SSIMptr)
{
    FILE *refFp = fopen(refFileName, "rb");
    if( refFp == NULL ) {
        // cannot open reference file
        fprintf(stderr, "Cannot open file %s\n", refFileName);
        return -1;
    }

    FILE *testFp = fopen(testFileName, "rb");
    if( testFp == NULL ) {
        // cannot open test file
        fprintf(stderr, "Cannot open file %s\n", testFileName);
        return -2;
    }

    int frames = 0;

    const int frameBytes = 3*width*height/2; // bytes in one frame I420
    unsigned char *ref = new unsigned char[frameBytes];
    unsigned char *test = new unsigned char[frameBytes];

    int refBytes = (int) fread(ref, 1, frameBytes, refFp);
    int testBytes = (int) fread(test, 1, frameBytes, testFp);

    double ssimScene = 0.0; //avgerage SSIM for sequence

    while( refBytes == frameBytes && testBytes == frameBytes )
    {
        ssimScene += SSIM_frame(ref, test, width, width, width, height);

        frames++;

        refBytes = (int) fread(ref, 1, frameBytes, refFp);
        testBytes = (int) fread(test, 1, frameBytes, testFp);
    }

    //SSIM: normalize/average for sequence
    ssimScene  = ssimScene / frames;
    *SSIMptr = ssimScene;


    delete [] ref;
    delete [] test;


    fclose(refFp);
    fclose(testFp);

    return 0;

}


RTPVideoCodecTypes
ConvertCodecType(const char* plname)
{
    if (strncmp(plname,"VP8" , 3) == 0)
    {
        return kRTPVideoVP8;
    }else if (strncmp(plname,"H263" , 5) == 0)
    {
        return kRTPVideoH263;
    }else if (strncmp(plname, "H263-1998",10) == 0)
    {
        return kRTPVideoH263;
    }else if (strncmp(plname,"I420" , 5) == 0)
    {
        return kRTPVideoI420;
    }else
    {
        return kRTPVideoNoVideo; // defualt value
    }

}

WebRtc_Word32
SendStatsTest::SendStatistics(const WebRtc_UWord32 bitRate, const WebRtc_UWord32 frameRate)
{
    TEST(frameRate <= _frameRate);
    TEST(bitRate > 0 && bitRate < 100000);
    printf("VCM 1 sec: Bit rate: %u\tFrame rate: %u\n", bitRate, frameRate);
    return 0;
}

WebRtc_Word32
KeyFrameReqTest::FrameTypeRequest(const FrameType frameType)
{
    TEST(frameType == kVideoFrameKey);
    if (frameType == kVideoFrameKey)
    {
        printf("Key frame requested\n");
    }
    else
    {
        printf("Non-key frame requested: %d\n", frameType);
    }
    return 0;
}
