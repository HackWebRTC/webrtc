/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_INTERFACE_VIDEO_CAPTURE_H_
#define WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_INTERFACE_VIDEO_CAPTURE_H_

/*
 * video_capture.h
 */

#include "modules/interface/module.h"
#include "video_capture_defines.h"

namespace webrtc {
// Class definitions
class VideoCaptureModule: public Module
{
public:
    /*
     *   Create a video capture module object
     *
     *   id              - unique identifier of this video capture module object
     *   deviceUniqueIdUTF8 -  name of the device. Available names can be found by using GetDeviceName
     */
    static VideoCaptureModule* Create(const WebRtc_Word32 id,
                                       const WebRtc_UWord8* deviceUniqueIdUTF8);

    /*
     *   Create a video capture module object used for external capture.
     *
     *   id              - unique identifier of this video capture module object
     *   externalCapture - [out] interface to call when a new frame is captured.
     */
    static VideoCaptureModule* Create(const WebRtc_Word32 id,
                                       VideoCaptureExternal*& externalCapture);

    /*
     *   destroy a video capture module object
     *
     *   module  - object to destroy
     */
    static void Destroy(VideoCaptureModule* module);

    /* Android specific function
     * Set global Android VM*/
    static WebRtc_Word32 SetAndroidObjects(void* javaVM, void* javaContext);

    /*
     *   Returns version of the module and its components
     *
     *   version                 - buffer to which the version will be written
     *   remainingBufferInBytes  - remaining number of WebRtc_Word8 in the version buffer
     *   position                - position of the next empty WebRtc_Word8 in the version buffer
     */
    static WebRtc_Word32 GetVersion(WebRtc_Word8* version,
                                    WebRtc_UWord32& remainingBufferInBytes,
                                    WebRtc_UWord32& position);

    /*
     *   Change the unique identifier of this object
     *
     *   id      - new unique identifier of this video capture module object
     */
    virtual WebRtc_Word32 ChangeUniqueId(const WebRtc_Word32 id) = 0;

    /**************************************************************************
     *
     *   Device Information
     *
     **************************************************************************/

    class DeviceInfo
    {
    public:

        virtual WebRtc_UWord32 NumberOfDevices()=0;

        /*
         * Returns the available capture devices.
         * deviceNumber   -[in] index of capture device
         * deviceNameUTF8 - friendly name of the capture device
         * deviceUniqueIdUTF8 - unique name of the capture device if it exist. Otherwise same as deviceNameUTF8
         * productUniqueIdUTF8 - unique product id if it exist. Null terminated otherwise.
         */
        virtual WebRtc_Word32 GetDeviceName(WebRtc_UWord32 deviceNumber,
                                            WebRtc_UWord8* deviceNameUTF8,
                                            WebRtc_UWord32 deviceNameLength,
                                            WebRtc_UWord8* deviceUniqueIdUTF8,
                                            WebRtc_UWord32 deviceUniqueIdUTF8Length,
                                            WebRtc_UWord8* productUniqueIdUTF8 = 0,
                                            WebRtc_UWord32 productUniqueIdUTF8Length = 0) = 0;

        /*
         *   Returns the number of capabilities for this device
         */
        virtual WebRtc_Word32 NumberOfCapabilities(const WebRtc_UWord8* deviceUniqueIdUTF8)=0;

        /*
         *   Gets the capabilities of the named device
         */
        virtual WebRtc_Word32 GetCapability(const WebRtc_UWord8* deviceUniqueIdUTF8,
                                            const WebRtc_UWord32 deviceCapabilityNumber,
                                            VideoCaptureCapability& capability) = 0;

        /*
         *   Gets clockwise angle the captured frames should be rotated in order to be displayed
         *   correctly on a normally rotated display.
         */
        virtual WebRtc_Word32 GetOrientation(const WebRtc_UWord8* deviceUniqueIdUTF8,
                                             VideoCaptureRotation& orientation)=0;

        /*
         *  Gets the capability that best matches the requested width, height and frame rate.
         *  Returns the deviceCapabilityNumber on success.
         */
        virtual WebRtc_Word32 GetBestMatchedCapability(
                                         const WebRtc_UWord8*deviceUniqueIdUTF8,
                                         const VideoCaptureCapability requested,
                                         VideoCaptureCapability& resulting) = 0;

        /*
         * Display OS /capture device specific settings dialog
         */
        virtual WebRtc_Word32 DisplayCaptureSettingsDialogBox(
                                        const WebRtc_UWord8* deviceUniqueIdUTF8,
                                        const WebRtc_UWord8* dialogTitleUTF8,
                                        void* parentWindow, WebRtc_UWord32 positionX,
                                        WebRtc_UWord32 positionY) = 0;

    protected:
        virtual ~DeviceInfo(){}
    };

    static DeviceInfo* CreateDeviceInfo(const WebRtc_Word32 id);
    static void DestroyDeviceInfo(DeviceInfo* deviceInfo);

    class VideoCaptureEncodeInterface
    {
    public:
        virtual WebRtc_Word32 ConfigureEncoder(const VideoCodec& codec,
                                               WebRtc_UWord32 maxPayloadSize)=0;
        // Inform the encoder about the new target bit rate.
        //
        //          - newBitRate       : New target bit rate in Kbit/s
        //          - frameRate        : The target frame rate
        //
        virtual WebRtc_Word32 SetRates(WebRtc_Word32 newBitRate, WebRtc_Word32 frameRate) = 0;
        // Inform the encoder about the packet loss
        //
        //          - packetLoss       : Fraction lost
        //                               (loss rate in percent = 100 * packetLoss / 255)
        //
        virtual WebRtc_Word32 SetPacketLoss(WebRtc_UWord32 packetLoss) = 0;
        // Encode the next frame as key frame.
        //
        virtual WebRtc_Word32 EncodeFrameType(const FrameType type) =0;
    protected:
        virtual ~VideoCaptureEncodeInterface(){}
    };

    /**************************************************************************
     *
     *   Observers
     *
     ***************************************************************************/

    /*
     *   Register capture data callback
     */
    virtual WebRtc_Word32 RegisterCaptureDataCallback(
                                    VideoCaptureDataCallback& dataCallback) = 0;

    /*
     *   Remove capture data callback
     */
    virtual WebRtc_Word32 DeRegisterCaptureDataCallback() = 0;

    /*
     *   Register capture callback
     */
    virtual WebRtc_Word32 RegisterCaptureCallback(VideoCaptureFeedBack& callBack) = 0;

    /*
     *   Remove capture callback
     */
    virtual WebRtc_Word32 DeRegisterCaptureCallback() = 0;

    /**************************************************************************
     *
     *   Start/Stop
     *
     ***************************************************************************/

    /*
     *   Start capture device
     */
    virtual WebRtc_Word32 StartCapture(const VideoCaptureCapability& capability) = 0;

    /*
     *   Stop capture device
     */
    virtual WebRtc_Word32 StopCapture() = 0;

    /*
     *   Send an image when the capture device is not running.
     */
    virtual WebRtc_Word32 StartSendImage(const VideoFrame& videoFrame,
                                         WebRtc_Word32 frameRate = 1)=0;

    /*
     *   Stop send image.
     */
    virtual WebRtc_Word32 StopSendImage()=0;

    /**************************************************************************
     *
     *   Properties of the set device
     *
     ***************************************************************************/
    /*
     *   Returns the name of the device used by this module.
     */
    virtual const WebRtc_UWord8* CurrentDeviceName() const =0;

    /*
     * Returns true if the capture device is running
     */
    virtual bool CaptureStarted() = 0;

    /*
     *  Gets the current configuration.
     */
    virtual WebRtc_Word32 CaptureSettings(VideoCaptureCapability& settings) = 0;

    virtual WebRtc_Word32 SetCaptureDelay(WebRtc_Word32 delayMS)=0;

    /* Returns the current CaptureDelay.
     Only valid when the camera is running*/
    virtual WebRtc_Word32 CaptureDelay()=0;

    /* Set the rotation of the captured frames.
     If the rotation is set to the same as returned by
     DeviceInfo::GetOrientation the captured frames are displayed correctly if rendered.*/
    virtual WebRtc_Word32 SetCaptureRotation(VideoCaptureRotation rotation)=0;

    /* Gets a pointer to an encode interface if the capture device supports the requested type and size.
     NULL otherwise.
     */
    virtual VideoCaptureEncodeInterface* GetEncodeInterface(const VideoCodec& codec)= 0;

    /**************************************************************************
     * Information Callbacks
     *
     ***************************************************************************/
    virtual WebRtc_Word32 EnableFrameRateCallback(const bool enable) = 0;
    virtual WebRtc_Word32 EnableNoPictureAlarm(const bool enable) = 0;

};
} //namespace webrtc
#endif  // WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_INTERFACE_VIDEO_CAPTURE_H_
