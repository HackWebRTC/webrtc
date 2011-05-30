/**
 * 
 */
package org.webrtc.videoengine;
import java.io.IOException;
import java.util.Locale;
import java.util.concurrent.locks.ReentrantLock;

import org.webrtc.videoengine.CaptureCapabilityAndroid;
import org.webrtc.videoengine.VideoCaptureDeviceInfoAndroid.AndroidVideoCaptureDevice;

import android.graphics.ImageFormat;
import android.graphics.PixelFormat;
import android.hardware.Camera;
import android.hardware.Camera.PreviewCallback;
import android.util.Log;
import android.view.SurfaceHolder;
import android.view.SurfaceHolder.Callback;
 

public class VideoCaptureAndroid implements 
		PreviewCallback, Callback{

	private Camera _camera;
	private AndroidVideoCaptureDevice _currentDevice=null;
	public ReentrantLock _previewBufferLock = new ReentrantLock();	
	private int PIXEL_FORMAT = ImageFormat.NV21;
	PixelFormat _pixelFormat = new PixelFormat();	
	private boolean _isRunning=false; // True when the C++ layer has ordered the camera to be started.
	
	private final int _numCaptureBuffers = 3;
	private int _expectedFrameSize = 0;
	private int _orientation = 0;
	private int _id=0;
	private long _context=0; // C++ callback context variable.
	private SurfaceHolder _localPreview=null;
	private boolean _ownsBuffers=false; // True if this class owns the preview video buffers.
	
	
	//Logging
	private static int LOGLEVEL = 0; // Set this to 2 for VERBOSE logging. 1 for DEBUG
	private static boolean VERBOSE = LOGLEVEL > 2;
	private static boolean DEBUG = LOGLEVEL > 1;	
	 
	
	CaptureCapabilityAndroid _currentCapability=null;
	
	public static void DeleteVideoCaptureAndroid(VideoCaptureAndroid captureAndroid)
	{
		if(DEBUG) Log.d("*WEBRTC*", "DeleteVideoCaptureAndroid");
		
		captureAndroid.StopCapture();
		captureAndroid._camera.release();		
		captureAndroid._camera=null;
		captureAndroid._context=0;
		
		if(DEBUG) Log.v("*WEBRTC*", "DeleteVideoCaptureAndroid ended");
		
	}
	
	public VideoCaptureAndroid(int id, long context,Camera camera,AndroidVideoCaptureDevice device)
	{
		_id=id;		
		_context=context;
		_camera=camera;		
		_currentDevice=device;
	}	
		
	public int StartCapture(int width, int height, int frameRate)
	{
		if(DEBUG) Log.d("*WEBRTC*", "StartCapture width" + width + " height " + height +" frame rate " + frameRate);
		try
		{
			if (_camera == null) 
			{					
				Log.e("*WEBRTC*",String.format(Locale.US,"Camera not initialized %d",_id));
				return -1;
			}			
			_currentCapability=new CaptureCapabilityAndroid();
			_currentCapability.width=width;
			_currentCapability.height=height;
			_currentCapability.maxFPS=frameRate;
			PixelFormat.getPixelFormatInfo(PIXEL_FORMAT, _pixelFormat);		
			
			Camera.Parameters parameters = _camera.getParameters();
			parameters.setPreviewSize(_currentCapability.width, _currentCapability.height);
			parameters.setPreviewFormat(PIXEL_FORMAT );		
			parameters.setPreviewFrameRate(_currentCapability.maxFPS);			
		   _camera.setParameters(parameters);
		   
		   _localPreview=ViERenderer.GetLocalRenderer(); // Get the local preview SurfaceHolder from the static render class
		   if(_localPreview!=null)
		   {
			   _localPreview.addCallback(this);			   
		   }
		   
		      
 			   	
		   int bufSize = width	* height * _pixelFormat.bitsPerPixel / 8;
		   if(android.os.Build.VERSION.SDK_INT>=7)
		   {
				//According to Doc addCallbackBuffer belongs to API level 8. But it seems like it works on Android 2.1 as well.
			    //At least SE X10 and Milestone
			   byte[] buffer = null;
			   for (int i = 0; i < _numCaptureBuffers; i++)
			   {
				   buffer = new byte[bufSize];
				   _camera.addCallbackBuffer(buffer);
			   }
		   
			   _camera.setPreviewCallbackWithBuffer(this);
			   _ownsBuffers=true;
		   }
		   else
		   {		  
			   _camera.setPreviewCallback(this);
		   }
		   
		   _camera.startPreview();		   
		   _previewBufferLock.lock();
		   _expectedFrameSize = bufSize;
		   _isRunning=true;
		   _previewBufferLock.unlock();
		   
		}		
		catch (Exception ex) { 		
			Log.e("*WEBRTC*", "Failed to start camera");
			return -1;
		}	
		return 0;
	}
	
	public int StopCapture()
	{
		if(DEBUG) Log.d("*WEBRTC*", "StopCapture");
		try
		{
		   _previewBufferLock.lock();
		   _isRunning=false;
		   _previewBufferLock.unlock();
		   		   
		   _camera.stopPreview();
		   
		   if(android.os.Build.VERSION.SDK_INT>7)
		   {
			   _camera.setPreviewCallbackWithBuffer(null);
		   }
		   else
		   {
			   _camera.setPreviewCallback(null);
		   }		   			
		} catch (Exception ex) { 		
			Log.e("*WEBRTC*", "Failed to stop camera");
			return -1;
		}

		if(DEBUG) Log.d("*WEBRTC*", "StopCapture ended");
		return 0;
	}
	
	native void ProvideCameraFrame(byte[] data,int length, long captureObject);

	public void onPreviewFrame(byte[] data, Camera camera) {
		_previewBufferLock.lock();

		if(VERBOSE) Log.v("*WEBRTC*",String.format(Locale.US,"preview frame length %d context %x",data.length,_context));
		if(_isRunning)
		{
			// If StartCapture has been called but not StopCapture
			// Call the C++ layer with the captured frame
			if (data.length == _expectedFrameSize)
			{
				ProvideCameraFrame(data, _expectedFrameSize, _context);
				if (VERBOSE) Log.v("*WEBRTC*", String.format(Locale.US, "frame delivered"));
				if(_ownsBuffers)
				{
					// Give the video buffer to the camera service again.
					_camera.addCallbackBuffer(data);
				}
			}
		}
		_previewBufferLock.unlock();		
		
	}


	public void surfaceChanged(SurfaceHolder holder, int format, int width,
			int height) {
		
		try {			
			if(_camera!=null)
			{
				_camera.setPreviewDisplay(_localPreview);				
			}			
		} catch (IOException e) {			
			Log.e("*WEBRTC*", String.format(Locale.US, "Failed to set Local preview. "+ e.getMessage()));			
		}
	}
	/*
	 * Sets the rotation of the preview render window.
	 * Does not affect the captured video image.
	 */
	public void SetPreviewRotation(int rotation)
	{		
		if(_camera!=null)
		{
			_previewBufferLock.lock();
			final boolean running=_isRunning;
			int width=0;
			int height=0;
			int framerate=0;
						
			if(running)
			{
				width=_currentCapability.width;
				height=_currentCapability.height;
				framerate=_currentCapability.maxFPS;
				
				StopCapture();
				
			}
		
			
			int resultRotation=0;
			if(_currentDevice._frontCameraType==VideoCaptureDeviceInfoAndroid.FrontFacingCameraType.Android23)
			{
				// this is a 2.3 or later front facing camera. SetDisplayOrientation will flip the image horizontally before doing the rotation. 				
				resultRotation=(360-rotation) % 360; // compensate the mirror
			}
			else
			{ // Back facing or 2.2 or previous front camera
				resultRotation=rotation;						
			}
			if(android.os.Build.VERSION.SDK_INT>7)
			{
				_camera.setDisplayOrientation(resultRotation);
			}
			else // Android 2.1 and previous
			{
				// This rotation unfortunately does not seems to work.
				//http://code.google.com/p/android/issues/detail?id=1193
				Camera.Parameters parameters = _camera.getParameters();
				parameters.setRotation(resultRotation);
				_camera.setParameters(parameters);
			}					
							
			if(running)				
			{
				StartCapture(width, height, framerate);
			}
			_previewBufferLock.unlock();		
		}
	}
	


	public void surfaceCreated(SurfaceHolder holder) {

	}


	public void surfaceDestroyed(SurfaceHolder holder) {		// 
		
	}

}
