/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test.h"
#include "video_source.h"
#include "vplib.h"
#include "event_wrapper.h"
#include "thread_wrapper.h"
#include <iostream>
#include <fstream>
#include <cmath>
#include <ctime>
#include <string.h>
#include <cassert>
#include <vector>

using namespace webrtc;

long filesize(const char *filename); // local function defined at end of file

struct SSIMcontext
{
    SSIMcontext() :
    refFileName(NULL), testFileName(NULL), width(0), height(0),
        SSIMptr(NULL), startFrame(-1), endFrame(-1), evnt(NULL) {};
    SSIMcontext(const char *ref, const char *test, int w, int h, double *Sptr,
        int start, int end, EventWrapper* ev) :
    refFileName(ref), testFileName(test), width(w), height(h),
        SSIMptr(Sptr), startFrame(start), endFrame(end), evnt(ev) {};
    const char *refFileName;
    const char *testFileName;
    int width;
    int height;
    double *SSIMptr;
    int startFrame;
    int endFrame;
    EventWrapper* evnt;
};

Test::Test(std::string name, std::string description)
:
_bitRate(0),
_inname(""),
_outname(""),
_encodedName(""),
_name(name),
_description(description)
{
    memset(&_inst, 0, sizeof(_inst));
    unsigned int seed = static_cast<unsigned int>(0);
    std::srand(seed);
}

Test::Test(std::string name, std::string description, WebRtc_UWord32 bitRate)
:
_bitRate(bitRate),
_inname(""),
_outname(""),
_encodedName(""),
_name(name),
_description(description)
{
    memset(&_inst, 0, sizeof(_inst));
    unsigned int seed = static_cast<unsigned int>(0);
    std::srand(seed);
}

void
Test::Print()
{
    std::cout << _name << " completed!" << std::endl;
    (*_log) << _name << std::endl;
    (*_log) << _description << std::endl;
    (*_log) << "Input file: " << _inname << std::endl;
    (*_log) << "Output file: " << _outname << std::endl;
    double psnr = -1.0, ssim = -1.0;
    PSNRfromFiles(_inname.c_str(), _outname.c_str(), _inst.width, _inst.height, &psnr);
    ssim = SSIMfromFilesMT(4 /* number of threads*/);

    (*_log) << "PSNR: " << psnr << std::endl;
    std::cout << "PSNR: " << psnr << std::endl << std::endl;
    (*_log) << "SSIM: " << ssim << std::endl;
    std::cout << "SSIM: " << ssim << std::endl << std::endl;
    (*_log) << std::endl;
}

void
Test::Setup()
{
    int widhei          = _inst.width*_inst.height;
    _lengthSourceFrame  = 3*widhei/2;
    _sourceBuffer       = new unsigned char[_lengthSourceFrame];
}

void
Test::CodecSettings(int width, int height, WebRtc_UWord32 frameRate /*=30*/, WebRtc_UWord32 bitRate /*=0*/)
{
    if (bitRate > 0)
    {
        _bitRate = bitRate;
    }
    else if (_bitRate == 0)
    {
        _bitRate = 600;
    }
    _inst.maxFramerate = (unsigned char)frameRate;
    _inst.startBitrate = (int)_bitRate;
    _inst.maxBitrate = 8000;
    _inst.width = width;
    _inst.height = height;
}

void
Test::Teardown()
{
    delete [] _sourceBuffer;
}

void
Test::SetEncoder(webrtc::VideoEncoder*encoder)
{
    _encoder = encoder;
}

void
Test::SetDecoder(VideoDecoder*decoder)
{
    _decoder = decoder;
}

void
Test::SetLog(std::fstream* log)
{
    _log = log;
}

int
Test::PSNRfromFiles(const char *refFileName, const char *testFileName, int width, int height, double *YPSNRptr)
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
    int frames = 0;

    int frameBytes = 3*width*height/2; // bytes in one frame I420
    unsigned char *ref = new unsigned char[frameBytes]; // space for one frame I420
    unsigned char *test = new unsigned char[frameBytes]; // space for one frame I420

    int refBytes = (int) fread(ref, 1, frameBytes, refFp);
    int testBytes = (int) fread(test, 1, frameBytes, testFp);

    while( refBytes == frameBytes && testBytes == frameBytes )
    {
        mse = 0.0;

        // calculate Y sum-square-difference
        for( int k = 0; k < width * height; k++ )
        {
            mse += (test[k] - ref[k]) * (test[k] - ref[k]);
        }

        // divide by number of pixels
        mse /= (double) (width * height);

        // accumulate for total average
        mseLogSum += std::log10( mse );
        frames++;

        refBytes = (int) fread(ref, 1, frameBytes, refFp);
        testBytes = (int) fread(test, 1, frameBytes, testFp);
    }

    // ypsnrAvg = sum( 10 log (255^2 / MSE) ) / frames
    //          = 20 * log(255) - 10 * mseLogSum / frames
    *YPSNRptr = 20.0 * std::log10(255.0) - 10.0 * mseLogSum / frames;

    delete [] ref;
    delete [] test;

    fclose(refFp);
    fclose(testFp);

    return 0;
}
int
Test::SSIMfromFiles(const char *refFileName, const char *testFileName, int width, int height, double *SSIMptr,
                    int startFrame /*= -1*/, int endFrame /*= -1*/)
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

    if (startFrame >= 0)
    {
        if (fseek(refFp, frameBytes * startFrame, SEEK_SET) != 0){
            fprintf(stderr, "Cannot go to frame %i in %s\n", startFrame, refFileName);
            return -1;
        }
        if (fseek(testFp, frameBytes * startFrame, SEEK_SET) != 0){
            fprintf(stderr, "Cannot go to frame %i in %s\n", startFrame, testFileName);
            return -1;
        }
    }

    int refBytes = (int) fread(ref, 1, frameBytes, refFp);
    int testBytes = (int) fread(test, 1, frameBytes, testFp);

    //
    // SSIM: variable definition, window function, initialization
    int window = 10;
    int flag_window = 0;  //0 for uniform window filter, 1 for gaussian symmetric window
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
    float offset1 = 0.1f;
    float offset2 = 0.1f;
    float offset3 = offset2/2;
    //
    //define window for SSIM: take uniform filter for now
    float sumfil = 0.0;
    int nn=-1;
    for(int j=-window/2;j<=window/2;j++)
    for(int i=-window/2;i<=window/2;i++)
    {
        nn+=1;
        if (flag_window == 0)
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

    while( refBytes == frameBytes && testBytes == frameBytes &&
        !(endFrame >= 0 && frames > endFrame - startFrame))
    {
        float ssimFrame = 0.0;
        int sh = window/2+1;
        int numPixels = 0;
        for(int i=sh;i<height-sh;i++)
        for(int j=sh;j<width-sh;j++)
        {
            avgTest[0] = 0.0;
            avgRef[0] = 0.0;
            contrastTest[0] = 0.0;
            contrastRef[0] = 0.0;
            crossCorr[0] = 0.0;

            numPixels +=1;

            //for uniform window, only need to loop over whole window for first column pixel in image, and then shift
            if (j == sh || flag_window == 1)
            {
                //initialize statistics
                for(int k=1;k<window+2;k++)
                {
                    avgTest[k] = 0.0;
                    avgRef[k] = 0.0;
                    contrastTest[k] = 0.0;
                    contrastRef[k] = 0.0;
                    crossCorr[k] = 0.0;
                }
                int nn=-1;
                //compute contrast and correlation
                for(int jj=-window/2;jj<=window/2;jj++)
                for(int ii=-window/2;ii<=window/2;ii++)
                {
                    nn+=1;
                    int i2 = i+ii;
                    int j2 = j+jj;
                    float tmp1 = (float)test[i2*width+j2];
                    float tmp2 = (float)ref[i2*width+j2];
                    //local average of each signal
                    avgTest[jj+window/2+1] += ssimFilter[nn]*tmp1;
                    avgRef[jj+window/2+1] += ssimFilter[nn]*tmp2;
                    //local correlation/contrast of each signal
                    contrastTest[jj+window/2+1] += ssimFilter[nn]*tmp1*tmp1;
                    contrastRef[jj+window/2+1] += ssimFilter[nn]*tmp2*tmp2;
                    //local cross correlation
                    crossCorr[jj+window/2+1] += ssimFilter[nn]*tmp1*tmp2;
                }
            }
            //for uniform window case, can shift window horiz, then compute statistics for last column in window
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
                avgTest[window+1] = 0.0;
                avgRef[window+1] = 0.0;
                contrastTest[window+1] = 0.0;
                contrastRef[window+1] = 0.0;
                crossCorr[window+1] = 0.0;
                int nn = (window+1)*window - 1;
                int jj = window/2;
                int j2 = j + jj;
                for(int ii=-window/2;ii<=window/2;ii++)
                {
                    nn+=1;
                    int i2 = i+ii;
                    float tmp1 = (float)test[i2*width+j2];
                    float tmp2 = (float)ref[i2*width+j2];
                    //local average of each signal
                    avgTest[jj+window/2+1] += ssimFilter[nn]*tmp1;
                    avgRef[jj+window/2+1] += ssimFilter[nn]*tmp2;
                    //local correlation/contrast of each signal
                    contrastTest[jj+window/2+1] += ssimFilter[nn]*tmp1*tmp1;
                    contrastRef[jj+window/2+1] += ssimFilter[nn]*tmp2*tmp2;
                    //local cross correlation
                    crossCorr[jj+window/2+1] += ssimFilter[nn]*tmp1*tmp2;
                }
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
        }
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

    fclose(refFp);
    fclose(testFp);

    return 0;
}

bool
Test::SSIMthread(void *vctx)
{
    SSIMcontext *ctx = (SSIMcontext *) vctx;
    SSIMfromFiles(ctx->refFileName, ctx->testFileName, ctx->width, ctx->height, ctx->SSIMptr, ctx->startFrame, ctx->endFrame);
    ctx->evnt->Set();
    return false;
}

double Test::SSIMfromFilesMT(const int numThreads)
{
    int numFrames = filesize(_inname.c_str()) / _lengthSourceFrame;
    std::vector<int> nFramesVec(numThreads);
    std::vector<double> ssimVec(numThreads);
    int framesPerCore = (numFrames + numThreads - 1) / numThreads; // rounding up
    int i = 0;
    int nFrames;
    for (nFrames = numFrames; nFrames >= framesPerCore; nFrames -= framesPerCore)
    {
        nFramesVec[i++] = framesPerCore;
    }
    if (nFrames > 0)
    {
        assert(i == numThreads - 1);
        nFramesVec[i] = nFrames; // remainder
    }

    int frameIx = 0;
    std::vector<EventWrapper*> eventVec(numThreads);
    std::vector<ThreadWrapper*> threadVec(numThreads);
    std::vector<SSIMcontext> ctxVec(numThreads);
    for (i = 0; i < numThreads; i++)
    {
        eventVec[i] = EventWrapper::Create();
        ctxVec[i] = SSIMcontext(_inname.c_str(), _outname.c_str(), _inst.width, _inst.height, &ssimVec[i], frameIx, frameIx + nFramesVec[i] - 1, eventVec[i]);
        threadVec[i] = ThreadWrapper::CreateThread(SSIMthread, &(ctxVec[i]), kLowPriority);
        unsigned int id;
        threadVec[i]->Start(id);
        frameIx += nFramesVec[i];
    }

    // wait for all events
    for (i = 0; i < numThreads; i++) {
        eventVec[i]->Wait(100000 /* ms*/);
        threadVec[i]->Stop();
        delete threadVec[i];
        delete eventVec[i];
    }

    double avgSsim = 0;
    for (i = 0; i < numThreads; i++)
    {
        avgSsim += (ssimVec[i] * nFramesVec[i]);
    }

    avgSsim /= numFrames;
    return avgSsim;
}


double Test::ActualBitRate(int nFrames)
{
    return 8.0 * _sumEncBytes / (nFrames / _inst.maxFramerate);
}

bool Test::PacketLoss(double lossRate)
{
    return RandUniform() < lossRate;
}

void
Test::VideoBufferToRawImage(TestVideoBuffer& videoBuffer, RawImage &image)
{
    image._buffer = videoBuffer.GetBuffer();
    image._size = videoBuffer.GetSize();
    image._length = videoBuffer.GetLength();
    image._width = videoBuffer.GetWidth();
    image._height = videoBuffer.GetHeight();
    image._timeStamp = videoBuffer.GetTimeStamp();
}
void
Test::VideoEncodedBufferToEncodedImage(TestVideoEncodedBuffer& videoBuffer, EncodedImage &image)
{
    image._buffer = videoBuffer.GetBuffer();
    image._length = videoBuffer.GetLength();
    image._size = videoBuffer.GetSize();
    image._frameType = static_cast<VideoFrameType>(videoBuffer.GetFrameType());
    image._timeStamp = videoBuffer.GetTimeStamp();
    image._encodedWidth = videoBuffer.GetCaptureWidth();
    image._encodedHeight = videoBuffer.GetCaptureHeight();
    image._completeFrame = true;
}

long filesize(const char *filename)
{
FILE *f = fopen(filename,"rb");  /* open the file in read only */

long size = 0;
  if (fseek(f,0,SEEK_END)==0) /* seek was successful */
      size = ftell(f);
  fclose(f);
  return size;
}
