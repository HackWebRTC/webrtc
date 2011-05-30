package org.webrtc.videoengine;

import java.nio.ByteBuffer;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.util.Log;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.SurfaceHolder.Callback;

public class ViESurfaceRenderer implements Callback {

	private Bitmap _bitmap=null; // the bitmap used for drawing.
	private ByteBuffer _byteBuffer;
	private SurfaceHolder _surfaceHolder;
	private Rect _srcRect=new Rect(); // Rect of the source bitmap to draw
	private Rect _dstRect=new Rect(); // Rect of the destination canvas to draw to
	private int  _dstHeight=0;
	private int  _dstWidth=0;
	private float _dstTopScale=0;
	private float _dstBottomScale=1;	
	private float _dstLeftScale=0;
	private float _dstRightScale=1;
	
	
	public  ViESurfaceRenderer(SurfaceView view)
	{
		_surfaceHolder=view.getHolder();		
		if(_surfaceHolder==null)
			return;
		
		Canvas canvas=_surfaceHolder.lockCanvas();
		if(canvas!=null)
		{
			Rect dst=_surfaceHolder.getSurfaceFrame();
			if(dst!=null)
			{
				_dstRect=dst;
				_dstHeight=_dstRect.bottom-_dstRect.top;
				_dstWidth=_dstRect.right-_dstRect.left;
			}
			_surfaceHolder.unlockCanvasAndPost(canvas);
		}
		
		_surfaceHolder.addCallback(this);
		
	}
	public void surfaceChanged(SurfaceHolder holder, int format, int width,
			int height) {
		
		_dstHeight=height;
		_dstWidth=width;
		
		_dstRect.left=(int)(_dstLeftScale*_dstWidth);
		_dstRect.top=(int)(_dstTopScale*_dstHeight);
		_dstRect.bottom=(int)(_dstBottomScale*_dstHeight);
		_dstRect.right=(int) (_dstRightScale*_dstWidth);
	}

	public void surfaceCreated(SurfaceHolder holder) {
		// TODO Auto-generated method stub

	}

	public void surfaceDestroyed(SurfaceHolder holder) {
		// TODO Auto-generated method stub

	}
	public Bitmap CreateBitmap(int width, int height)
	{
		if (_bitmap == null) 
		{
			try {
				android.os.Process
						.setThreadPriority(android.os.Process.THREAD_PRIORITY_DISPLAY);
			} catch (Exception e) {
				
			}			
		}
		_bitmap=Bitmap.createBitmap(width, height, Bitmap.Config.RGB_565);
		_srcRect.left=0;
		_srcRect.top=0;
		_srcRect.bottom=height;
		_srcRect.right=width;
		
		
		return _bitmap;
	}
	
	public ByteBuffer CreateByteBuffer(int width, int height)
	{
		if (_bitmap == null) 
		{
			try {
				android.os.Process
						.setThreadPriority(android.os.Process.THREAD_PRIORITY_DISPLAY);
			} catch (Exception e) {
				
			}			
		}
		
		try {
			_bitmap=Bitmap.createBitmap(width, height, Bitmap.Config.RGB_565);
			_byteBuffer=ByteBuffer.allocateDirect(width*height*2);
			_srcRect.left=0;
			_srcRect.top=0;
			_srcRect.bottom=height;
			_srcRect.right=width;
		}
		catch (Exception ex) { 		
			Log.e("*WEBRTC*", "Failed to CreateByteBuffer");
			_bitmap=null;
			_byteBuffer=null;		
		}
			
		
		return _byteBuffer;
	}
	
	public void SetCoordinates(            
            float left,
            float top,
            float right,
            float bottom)
	{		
		_dstLeftScale=left;
		_dstTopScale=top;
		_dstRightScale=right;
		_dstBottomScale=bottom;
		
		_dstRect.left=(int)(_dstLeftScale*_dstWidth);
		_dstRect.top=(int)(_dstTopScale*_dstHeight);
		_dstRect.bottom=(int)(_dstBottomScale*_dstHeight);
		_dstRect.right=(int) (_dstRightScale*_dstWidth);
		
	}
	
	public void DrawByteBuffer()
	{		
		if(_byteBuffer==null)
			return;
		_byteBuffer.rewind();
		_bitmap.copyPixelsFromBuffer(_byteBuffer);
		DrawBitmap();
		
	}
	
	public void DrawBitmap()
	{
		if(_bitmap==null)
			return;
		
		Canvas canvas=_surfaceHolder.lockCanvas();
		if(canvas!=null)
		{
			canvas.drawBitmap(_bitmap, _srcRect, _dstRect, null);
			_surfaceHolder.unlockCanvasAndPost(canvas);
		}
		
	}
	

}
