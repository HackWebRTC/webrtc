package org.webrtc.videoengine;

import android.content.Context;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

public class ViERenderer {

	private static SurfaceHolder g_localRenderer; // View used for local rendering that Cameras can use for Video Overlay. 
	
	public static SurfaceView CreateRenderer(Context context)
	{
		return  CreateRenderer(context,false);
	}
	public static SurfaceView CreateRenderer(Context context, boolean useOpenGLES2)
	{
		if(useOpenGLES2==true && ViEAndroidGLES20.IsSupported(context))		
			return new ViEAndroidGLES20(context);
		else			
			return new SurfaceView(context); 
	}
	
	/*
	 * Creates a SurfaceView to be used by Android Camera service to display a local preview.
	 * This needs to be used on Android prior to version 2.1 in order to run the camera.
	 * Call this function before ViECapture::StartCapture.
	 * The created view needs to be added to a visible layout after a camera has been allocated (with the call ViECapture::AllocateCaptureDevice).
	 * 
	 *  IE.
	 *  	CreateLocalRenderer
	 *  	ViECapture::AllocateCaptureDevice
	 *  	LinearLayout.addview
	 *  	ViECapture::StartCapture
	 *  
	 */
	public static SurfaceView CreateLocalRenderer(Context context)
	{
		SurfaceView localRender= new SurfaceView(context);
		g_localRenderer=localRender.getHolder();
		
		g_localRenderer.setType(SurfaceHolder.SURFACE_TYPE_PUSH_BUFFERS);		
		return  localRender;
	}
	
	public static SurfaceHolder GetLocalRenderer()
	{
		return g_localRenderer;
	}
	
}
