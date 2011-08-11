/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

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

    int frameBytes = 3*width*height/2; // bytes in one frame I420
    unsigned char *ref = new unsigned char[frameBytes]; // space for one frame I420
    unsigned char *test = new unsigned char[frameBytes]; // space for one frame I420

    int refBytes = (int) fread(ref, 1, frameBytes, refFp);
    int testBytes = (int) fread(test, 1, frameBytes, testFp);

    float *righMostColumnAvgTest =  new float[width];
    float *righMostColumnAvgRef =  new float[width];
    float *righMostColumnContrastTest =  new float[width];
    float *righMostColumnContrastRef = new float[width];
    float *righMostColumnCrossCorr = new float[width];

    float term1,term2,term3,term4,term5;

    //
    // SSIM: variable definition, window function, initialization
    int window = 10;
    //
    int flag_window = 0;  //0 and 1 for uniform window filter, 2 for gaussian window
    //
    float variance_window = 2.0; //variance for window function
    float ssimFilter[121]; //2d window filter: typically 11x11 = (window+1)*(window+1)
    //statistics per column of window (#columns = window+1), 0 element for avg over all columns
    float avgTest[12];
    float avgRef[12];
    float contrastTest[12];
    float contrastRef[12];
    float crossCorr[12];
    //
    //offsets for stability
    float offset1 = 1.0f; //0.1
    float offset2 = 1.0f; //0.1
    //for Guassian window: settings from paper:
    //float offset1 = 6.0f;   // ~ (K1*L)^2 , K1 = 0.01
    //float offset2 = 58.0f;  // ~ (K1*L)^2 , K2 = 0.03


    float offset3 = offset2/2;
    //
    //define window for SSIM: take uniform filter for now
    float sumfil = 0.0;
    int nn=-1;
    for(int j=-window/2;j<=window/2;j++)
    for(int i=-window/2;i<=window/2;i++)
    {
        nn+=1;
        if (flag_window != 2)
            ssimFilter[nn] =  1.0;
        else
        {
            float dist  = (float)(i*i) + (float)(j*j);
            float tmp = 0.5f*dist/variance_window;
            ssimFilter[nn] = exp(-tmp);
        }
        sumfil +=ssimFilter[nn];
    }
    //normalize window
    nn=-1;
    for(int j=-window/2;j<=window/2;j++)
    for(int i=-window/2;i<=window/2;i++)
    {
        nn+=1;
        ssimFilter[nn] = ssimFilter[nn]/((float)sumfil);
    }
    //
    float ssimScene = 0.0; //avgerage SSIM for sequence
    //
    //SSIM: done with variables  and defintion
    //

       int sh = 8; //boundary offset

    while( refBytes == frameBytes && testBytes == frameBytes )
    {
        float ssimFrame = 0.0;

        int numPixels = 0;

        //skip over pixels vertically and horizontally
        //for window cases 1 and 2
        int skipH = 2;
        int skipV = 2;

        //uniform window case, with window computation updated for each pixel horiz and vert: can't skip pixels for this case
        if (flag_window == 0)
        {
            skipH = 1;
            skipV = 1;
        }
        for(int i=sh;i<height-sh;i+=skipV)
        for(int j=sh;j<width-sh;j+=skipH)
        {
            avgTest[0] = 0.0;
            avgRef[0] = 0.0;
            contrastTest[0] = 0.0;
            contrastRef[0] = 0.0;
            crossCorr[0] = 0.0;

            numPixels +=1;

            if (flag_window > 0 )
            {
                //initialize statistics
                avgTest[0] = 0.0;
                avgRef[0] = 0.0;
                contrastTest[0] = 0.0;
                contrastRef[0] = 0.0;
                crossCorr[0] = 0.0;

                int nn=-1;
                //compute contrast and correlation
                //windows are symmetrics
                for(int jj=-window/2;jj<=window/2;jj++)
                for(int ii=-window/2;ii<=window/2;ii++)
                {
                    nn+=1;
                    int i2 = i+ii;
                    int j2 = j+jj;
                    float tmp1 = (float)test[i2*width+j2];
                    float tmp2 = (float)ref[i2*width+j2];

                    term1 = tmp1;
                    term2 = tmp2;
                    term3 = tmp1*tmp1;
                    term4 = tmp2*tmp2;
                    term5 = tmp1*tmp2;

                    //local average of each signal
                    avgTest[0] += ssimFilter[nn]*term1;
                    avgRef[0] += ssimFilter[nn]*term2;
                    //local correlation/contrast of each signal
                    contrastTest[0] += ssimFilter[nn]*term3;
                    contrastRef[0] += ssimFilter[nn]*term4;
                    //local cross correlation
                    crossCorr[0] += ssimFilter[nn]*term5;

                }

            }

            else
            {
                //for uniform window case == 0: only need to loop over whole window for first row and column, and then shift/update
                if (j == sh || i == sh)
                {
                    //initialize statistics
                    for(int k=0;k<window+2;k++)
                    {
                        avgTest[k] = 0.0;
                        avgRef[k] = 0.0;
                        contrastTest[k] = 0.0;
                        contrastRef[k] = 0.0;
                        crossCorr[k] = 0.0;
                    }

                    int nn=-1;
                    //compute contrast and correlation
                    //windows are symmetrics
                    for(int jj=-window/2;jj<=window/2;jj++)
                    for(int ii=-window/2;ii<=window/2;ii++)
                    {
                        nn+=1;
                        int i2 = i+ii;
                        int j2 = j+jj;
                        float tmp1 = (float)test[i2*width+j2];
                        float tmp2 = (float)ref[i2*width+j2];

                        term1 = tmp1;
                        term2 = tmp2;
                        term3 = tmp1*tmp1;
                        term4 = tmp2*tmp2;
                        term5 = tmp1*tmp2;

                        //local average of each signal
                        avgTest[jj+window/2+1] += term1;
                        avgRef[jj+window/2+1] += term2;
                        //local correlation/contrast of each signal
                        contrastTest[jj+window/2+1] += term3;
                        contrastRef[jj+window/2+1] += term4;
                        //local cross correlation
                        crossCorr[jj+window/2+1] += term5;

                    }

                    //normalize
                    for(int k=1;k<window+2;k++)
                    {
                        avgTest[k] = ssimFilter[0]*avgTest[k];
                        avgRef[k] = ssimFilter[0]*avgRef[k];
                        contrastTest[k] = ssimFilter[0]*contrastTest[k];
                        contrastRef[k] = ssimFilter[0]*contrastRef[k];
                        crossCorr[k] = ssimFilter[0]*crossCorr[k];
                    }

                }
                //for all other pixels, update window filter computation
                else
                {
                        //shift statistics horiz.
                    for(int k=1;k<window+1;k++)
                    {
                        avgTest[k]=avgTest[k+1];
                        avgRef[k]=avgRef[k+1];
                        contrastTest[k] = contrastTest[k+1];
                        contrastRef[k] = contrastRef[k+1];
                        crossCorr[k] = crossCorr[k+1];
                    }

                    //compute statistics for last column
                    //update right-most column, by updating with bottom pixel contribution
                    int j2 = j + window/2; //last column of window
                    int i2 = i + window/2; //last window pixel of column
                    int ix = i - window/2 - 1; //last window pixel of top neighboring pixel
                    float tmp1 = (float)test[i2*width+j2];
                    float tmp2 = (float)ref[i2*width+j2];
                    float tmp1x = (float)test[ix*width+j2];
                    float tmp2x = (float)ref[ix*width+j2];

                    avgTest[window+1] =  righMostColumnAvgTest[j]  + ssimFilter[0]*(tmp1 - tmp1x);
                    avgRef[window+1] =  righMostColumnAvgRef[j]  + ssimFilter[0]*(tmp2 - tmp2x);
                    contrastTest[window+1] =  righMostColumnContrastTest[j]  + ssimFilter[0]*(tmp1*tmp1 - tmp1x*tmp1x);
                    contrastRef[window+1] =  righMostColumnContrastRef[j]  + ssimFilter[0]*(tmp2*tmp2 - tmp2x*tmp2x);
                    crossCorr[window+1] =  righMostColumnCrossCorr[j]  + ssimFilter[0]*(tmp1*tmp2 - tmp1x*tmp2x);
                }

                //sum over all columns
                for(int k=1;k<window+2;k++)
                {
                    avgTest[0] += avgTest[k];
                    avgRef[0] += avgRef[k];
                    contrastTest[0] += contrastTest[k];
                    contrastRef[0] += contrastRef[k];
                    crossCorr[0] += crossCorr[k];
                }

                //
                righMostColumnAvgTest[j] = avgTest[window+1];
                righMostColumnAvgRef[j] = avgRef[window+1];
                righMostColumnContrastTest[j] = contrastTest[window+1];
                righMostColumnContrastRef[j] = contrastRef[window+1];
                righMostColumnCrossCorr[j] = crossCorr[window+1];
                //

            } //end of window = 0 case

            float tmp1 = (contrastTest[0] - avgTest[0]*avgTest[0]);
            if (tmp1 < 0.0) tmp1 = 0.0;
            contrastTest[0] = sqrt(tmp1);
            float tmp2 = (contrastRef[0] - avgRef[0]*avgRef[0]);
            if (tmp2 < 0.0) tmp2 = 0.0;
            contrastRef[0] = sqrt(tmp2);
            crossCorr[0] = crossCorr[0] - avgTest[0]*avgRef[0];

            float ssimCorrCoeff = (crossCorr[0]+offset3)/(contrastTest[0]*contrastRef[0] + offset3);
            float ssimLuminance = (2*avgTest[0]*avgRef[0]+offset1)/(avgTest[0]*avgTest[0] + avgRef[0]*avgRef[0] + offset1);
            float ssimContrast   = (2*contrastTest[0]*contrastRef[0]+offset2)/(contrastTest[0]*contrastTest[0] + contrastRef[0]*contrastRef[0] + offset2);

            float ssimPixel  = ssimCorrCoeff * ssimLuminance * ssimContrast;
            ssimFrame += ssimPixel;

        } //done with ssim computation

        ssimFrame = ssimFrame / (numPixels);
        //printf("***SSIM for frame ***%f \n",ssimFrame);
        ssimScene += ssimFrame;
        //
        //SSIM: done with SSIM computation
        //

        frames++;

        refBytes = (int) fread(ref, 1, frameBytes, refFp);
        testBytes = (int) fread(test, 1, frameBytes, testFp);

    }

    //SSIM: normalize/average for sequence
    ssimScene  = ssimScene / frames;
    *SSIMptr = ssimScene;


    delete [] ref;
    delete [] test;

    delete [] righMostColumnAvgTest;
    delete [] righMostColumnAvgRef;
    delete [] righMostColumnContrastTest;
    delete [] righMostColumnContrastRef;
    delete [] righMostColumnCrossCorr;


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
