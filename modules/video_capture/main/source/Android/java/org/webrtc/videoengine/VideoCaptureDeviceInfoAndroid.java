package org.webrtc.videoengine;

import java.io.File;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

import dalvik.system.DexClassLoader;

import android.content.Context;
import android.hardware.Camera;
import android.hardware.Camera.Size;
import android.util.Log;

public class VideoCaptureDeviceInfoAndroid {
	
	//Context
	Context _context;
	
	//Logging
	private static int LOGLEVEL = 0; // Set this to 2 for VERBOSE logging. 1 for DEBUG
	private static boolean VERBOSE = LOGLEVEL > 2;
	private static boolean DEBUG = LOGLEVEL > 1;
	
	/* Private class with info about all available cameras and the capabilities*/
	public class AndroidVideoCaptureDevice
	{
		AndroidVideoCaptureDevice()
		{
			_frontCameraType=FrontFacingCameraType.None;
			_index=0;			
		}
		
		public String _deviceUniqueName;
		public CaptureCapabilityAndroid _captureCapabilies[];
		public FrontFacingCameraType _frontCameraType;
		
		public int _orientation; //Orientation of camera as described in android.hardware.Camera.CameraInfo.Orientation
		public int _index; // Camera index used in Camera.Open on Android 2.3 and onwards
	}
	public enum FrontFacingCameraType
	{
		None, // This is not a front facing camera
		GalaxyS, // Galaxy S front facing camera.
		HTCEvo, // HTC Evo front facing camera
		Android23 // Android 2.3 front facing camera.
	}
	

	String _currentDeviceUniqueId;
	int _id;
	List<AndroidVideoCaptureDevice> _deviceList;
	
	
	public static VideoCaptureDeviceInfoAndroid CreateVideoCaptureDeviceInfoAndroid(int id, Context context)
	{
		if(DEBUG) Log.d("*WEBRTC*",String.format(Locale.US,"VideoCaptureDeviceInfoAndroid"));
		
		VideoCaptureDeviceInfoAndroid self = new VideoCaptureDeviceInfoAndroid(id,context);
		if(self!=null && self.Init()==0)
		{
			return self;			
		}
		else
		{
			if(DEBUG) Log.d("*WEBRTC*", "Failed to create VideoCaptureDeviceInfoAndroid.");
		}
		return null;
	}	
	
	private VideoCaptureDeviceInfoAndroid(int id, Context context)
	{
		_id=id;
		_context=context;
		_deviceList= new ArrayList<AndroidVideoCaptureDevice>();	
	}
	
	private int Init()
	{
		// Populate the _deviceList with available cameras and their capabilities.
		Camera camera=null;
		try{						
			if(android.os.Build.VERSION.SDK_INT>8) // From Android 2.3 and onwards
			{
				for(int i=0; i<Camera.getNumberOfCameras();++i)
				{
					AndroidVideoCaptureDevice newDevice=new AndroidVideoCaptureDevice();
										
					Camera.CameraInfo info=new Camera.CameraInfo();
					Camera.getCameraInfo(i, info);
					newDevice._index=i;
					newDevice._orientation=info.orientation;
					if(info.facing == Camera.CameraInfo.CAMERA_FACING_BACK)
					{						
						newDevice._deviceUniqueName="Camera " + i +", Facing back, Orientation "+ info.orientation;
					}
					else
					{
						newDevice._deviceUniqueName="Camera " + i +", Facing front, Orientation "+ info.orientation;
						newDevice._frontCameraType=FrontFacingCameraType.Android23;
					}
					
					camera=Camera.open(i);
					Camera.Parameters parameters = camera.getParameters();						
					AddDeviceInfo(newDevice, parameters);
					camera.release();
					camera=null;
					_deviceList.add(newDevice);
				}
			}
			else // Prior to Android 2.3
			{			
				AndroidVideoCaptureDevice newDevice;
				Camera.Parameters parameters;
				
				newDevice=new AndroidVideoCaptureDevice();				
				camera=Camera.open();					
				parameters = camera.getParameters();
				newDevice._deviceUniqueName="Camera 1, Facing back";
				newDevice._orientation=90;
				AddDeviceInfo(newDevice, parameters);
				
				_deviceList.add(newDevice);
				camera.release();
				camera=null;
			
				newDevice=new AndroidVideoCaptureDevice();
				newDevice._deviceUniqueName="Camera 2, Facing front";
				parameters=SearchOldFrontFacingCameras(newDevice);
				if(parameters!=null)
				{
					AddDeviceInfo(newDevice, parameters);					
					_deviceList.add(newDevice);
				}				
			}
		}catch (Exception ex) { 		
			Log.e("*WEBRTC*", "VideoCaptureDeviceInfoAndroid:Init Failed to init VideoCaptureDeviceInfo ex " +ex.getLocalizedMessage());
			return -1;
		}			
		VerifyCapabilities();	
		return 0;	
	}
	/*
	 * Adds the capture capabilities of the currently opened device
	 */
	private void AddDeviceInfo(AndroidVideoCaptureDevice newDevice,Camera.Parameters parameters)
	{
		
		List<Size> sizes=parameters.getSupportedPreviewSizes();
		List<Integer> frameRates=parameters.getSupportedPreviewFrameRates();
		int maxFPS=0;
		for(Integer frameRate:frameRates)
		{
			if(VERBOSE) Log.v("*WEBRTC*", "VideoCaptureDeviceInfoAndroid:Init supports frameRate "+ frameRate);
			if(frameRate>maxFPS)
			{
				maxFPS=frameRate;
			}
			
		}
		
		newDevice._captureCapabilies= new CaptureCapabilityAndroid[sizes.size()];
		for(int i=0;i<sizes.size();++i)
		{
			Size s=sizes.get(i);
			newDevice._captureCapabilies[i]=new CaptureCapabilityAndroid();
			newDevice._captureCapabilies[i].height=s.height;				
			newDevice._captureCapabilies[i].width=s.width;
			newDevice._captureCapabilies[i].maxFPS=maxFPS;
		}				

	}

	/*
	 * Function that make sure device specific capabilities are in the capability list. 
	 * Ie Galaxy S supports CIF but does not list CIF as a supported capability.
	 * Motorola Droid Camera does not work with frame rate above 15fps.
	 * http://code.google.com/p/android/issues/detail?id=5514#c0
	 */
	private void VerifyCapabilities()
	{
		// Nexus S or Galaxy S
		if(android.os.Build.DEVICE.equals("GT-I9000") || android.os.Build.DEVICE.equals("crespo"))
		{
			CaptureCapabilityAndroid specificCapability=new CaptureCapabilityAndroid();
			specificCapability.width=352;
			specificCapability.height=288;
			specificCapability.maxFPS=15;
			AddDeviceSpecificCapability(specificCapability);
			
			specificCapability=new CaptureCapabilityAndroid();
			specificCapability.width=176;
			specificCapability.height=144;
			specificCapability.maxFPS=15;
			AddDeviceSpecificCapability(specificCapability);
					
			specificCapability=new CaptureCapabilityAndroid();
			specificCapability.width=320;
			specificCapability.height=240;
			specificCapability.maxFPS=15;
			AddDeviceSpecificCapability(specificCapability);

		}
		// Motorola Milestone Camera server does not work at 30fps even though it reports that it can
		if(android.os.Build.MANUFACTURER.equals("motorola") && android.os.Build.DEVICE.equals("umts_sholes"))
		{
			for(AndroidVideoCaptureDevice device:_deviceList)	
			{							
				for(CaptureCapabilityAndroid capability:device._captureCapabilies)
				{
					capability.maxFPS=15;					
				}
			}
		}
	}
	private void AddDeviceSpecificCapability(CaptureCapabilityAndroid specificCapability)
	{
		for(AndroidVideoCaptureDevice device:_deviceList)	
		{
			boolean foundCapability=false;				
			for(CaptureCapabilityAndroid capability:device._captureCapabilies)
			{
				if(capability.width==specificCapability.width && capability.height==specificCapability.height)
				{
					foundCapability=true;
					break;
				}					
			}
			if(foundCapability==false)
			{					
				CaptureCapabilityAndroid newCaptureCapabilies[]= new CaptureCapabilityAndroid[device._captureCapabilies.length+1];
				for(int i=0;i<device._captureCapabilies.length;++i)
				{							
					newCaptureCapabilies[i+1]=device._captureCapabilies[i];						
				}
				newCaptureCapabilies[0]=specificCapability;		
				device._captureCapabilies=newCaptureCapabilies;
			}
				
		}
	}

	/*
	 * Returns the number of Capture devices that is supported
	 */
	public int NumberOfDevices()
	{
		return _deviceList.size();
	}
	
	public String GetDeviceUniqueName(int deviceNumber)	                                           	                                           					  
	{
		if(deviceNumber<0 || deviceNumber>=_deviceList.size())
		{
			return null;
		}				
		return _deviceList.get(deviceNumber)._deviceUniqueName;
	}
	
	public CaptureCapabilityAndroid[] 	                                    
	GetCapabilityArray (String deviceUniqueId)
	{
		for (AndroidVideoCaptureDevice device: _deviceList)
		{
			if(device._deviceUniqueName.equals(deviceUniqueId))
			{
				return (CaptureCapabilityAndroid[]) device._captureCapabilies;
			}
		}	
		return null;	
	}
	
	/* Returns the camera orientation as described by 
	 * android.hardware.Camera.CameraInfo.orientation
	 */
	public int GetOrientation(String deviceUniqueId)
	{
		for (AndroidVideoCaptureDevice device: _deviceList)
		{
			if(device._deviceUniqueName.equals(deviceUniqueId))
			{
				return device._orientation;
			}
		}	
		return -1;
	}
	
	/*
	 * Returns an instance of VideoCaptureAndroid.
	 */
	public VideoCaptureAndroid AllocateCamera(int id, long context,String deviceUniqueId)
	{
		try
		{
			if(DEBUG) Log.d("*WEBRTC*", "AllocateCamera " + deviceUniqueId);
						
			Camera camera=null;
			AndroidVideoCaptureDevice deviceToUse=null;
			for (AndroidVideoCaptureDevice device: _deviceList)
			{
				if(device._deviceUniqueName.equals(deviceUniqueId)) // Found the wanted camera
				{
					deviceToUse=device;
					switch(device._frontCameraType)
					{
					case GalaxyS:
						camera= AllocateGalaxySFrontCamera();							
						break;
					case HTCEvo:
						camera= AllocateEVOFrontFacingCamera();
						break;
					default:
						if(android.os.Build.VERSION.SDK_INT>8) // From Android 2.3 and onwards)	
							camera=Camera.open(device._index);
						else
							camera=Camera.open(); // Default camera					
					}					
				}
			}
			
			if(camera==null)
			{
				return null;
			}
			if(VERBOSE) Log.v("*WEBRTC*", "AllocateCamera - creating VideoCaptureAndroid");
			
			return new VideoCaptureAndroid(id,context,camera,deviceToUse);
			
		}catch (Exception ex) { 		
			Log.e("*WEBRTC*", "AllocateCamera Failed to open camera- ex " +ex.getLocalizedMessage());		
		}	
		return null;	
	}
	
	/* 
	 * Searches for a front facing camera device. This is device specific code. 
	 */
	private Camera.Parameters SearchOldFrontFacingCameras(AndroidVideoCaptureDevice newDevice) throws SecurityException, IllegalArgumentException, NoSuchMethodException, ClassNotFoundException, IllegalAccessException, InvocationTargetException 
	{
		//Check the id of the opened camera device (Returns null on X10 and 1 on Samsung Galaxy S.
		Camera camera=Camera.open();
		Camera.Parameters parameters=camera.getParameters();		
		String cameraId=parameters.get("camera-id");
		if(cameraId!=null && cameraId.equals("1")) // This might be a Samsung Galaxy S with a front facing camera. 
		{
			try
			{
				parameters.set("camera-id", 2);
				camera.setParameters(parameters);
				parameters = camera.getParameters();				
				newDevice._frontCameraType=FrontFacingCameraType.GalaxyS;
				newDevice._orientation=0;
				camera.release();
				return parameters;
			}
			catch (Exception ex) {
				//Nope - it did not work.
				Log.e("*WEBRTC*", "VideoCaptureDeviceInfoAndroid:Init Failed to open front camera camera - ex " +ex.getLocalizedMessage());
			}
		}
		camera.release();
		
		//Check for Evo front facing camera.			
		File file = new File("/system/framework/com.htc.hardware.twinCamDevice.jar");			
		boolean exists = file.exists();
		if (!exists){
			file = new File("/system/framework/com.sprint.hardware.twinCamDevice.jar");		    	
			exists = file.exists();
		}
		if(exists)
		{
			newDevice._frontCameraType=FrontFacingCameraType.HTCEvo;
			newDevice._orientation=0;
			Camera evCamera=AllocateEVOFrontFacingCamera();
			parameters=evCamera.getParameters();
			evCamera.release();
			return parameters;
		}				
		return null;		
	}
	
	
	/*
	 * Returns a handle to HTC front facing camera.
	 * The caller is responsible to release it on completion.
	 */
	private Camera AllocateEVOFrontFacingCamera() throws SecurityException, NoSuchMethodException, ClassNotFoundException, IllegalArgumentException, IllegalAccessException, InvocationTargetException
	{
		
		String classPath=null;
		File file = new File("/system/framework/com.htc.hardware.twinCamDevice.jar");
		classPath = "com.htc.hardware.twinCamDevice.FrontFacingCamera";
		boolean exists = file.exists();
		if (!exists){
			file = new File("/system/framework/com.sprint.hardware.twinCamDevice.jar");
	    	classPath = "com.sprint.hardware.twinCamDevice.FrontFacingCamera";
			exists = file.exists();
		}
		if(!exists)
		{
			return null;
		}				

		String dexOutputDir="";
    	if(_context!=null){
        		dexOutputDir = _context.getFilesDir().getAbsolutePath();            	
            	File mFilesDir = new File(dexOutputDir, "dexfiles");
            	if(!mFilesDir.exists()){
            		//Log.e("*WEBRTCN*", "Directory doesn't exists");
            		if(!mFilesDir.mkdirs()) {
            			//Log.e("*WEBRTCN*", "Unable to create files directory");
            		}
            	}
    	}

    	dexOutputDir += "/dexfiles";
        	
    	DexClassLoader loader = new DexClassLoader(
                    file.getAbsolutePath(),
                    dexOutputDir,
                    null,
                    ClassLoader.getSystemClassLoader()
            );
    	
		Method method = loader.loadClass(classPath).getDeclaredMethod("getFrontFacingCamera", (Class[]) null);
		Camera camera = (Camera) method.invoke((Object[])null,(Object[]) null);
		return camera;
	}
	
	/*
	 * Returns a handle to Galaxy S front camera.
	 * The caller is responsible to release it on completion.
	 */
	private Camera AllocateGalaxySFrontCamera()
	{		
		Camera camera=Camera.open();
		Camera.Parameters parameters = camera.getParameters();
		parameters.set("camera-id",2);
		camera.setParameters(parameters);
		return camera;
	}
	

}
