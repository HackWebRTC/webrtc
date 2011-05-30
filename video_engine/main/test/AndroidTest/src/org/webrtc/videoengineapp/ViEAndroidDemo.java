/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * TODO, refactoring
 */

package org.webrtc.videoengineapp;

import java.net.InetAddress;
import java.net.NetworkInterface;
import java.net.SocketException;
import java.util.Enumeration;

import org.webrtc.videoengine.ViERenderer;


import android.app.TabActivity;
import android.content.Context;
import android.content.res.Configuration;
import android.hardware.SensorManager;
import android.media.AudioManager;
import android.os.Bundle;
import android.os.PowerManager;
import android.os.PowerManager.WakeLock;

import android.util.Log;
import android.view.KeyEvent;
import android.view.Surface;
import android.view.SurfaceView;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;

import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.CheckBox;

import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.Spinner;
import android.widget.TabHost;
import android.widget.TextView;
import android.widget.AdapterView.OnItemSelectedListener;
import android.widget.TabHost.TabSpec;
import android.view.OrientationEventListener;

public class ViEAndroidDemo extends TabActivity implements IViEAndroidCallback,
		View.OnClickListener, OnItemSelectedListener {
	private ViEAndroidJavaAPI _ViEAndroidAPI = null;

	// remote renderer
	private SurfaceView _remoteSurfaceView = null;

	// local renderer and camera
	private SurfaceView _svLocal=null;

	// channel number
	private int _channel;
	private int _cameraId;
	private int _voiceChannel=-1;

	// flags
	private boolean _viERunning = false;
	private boolean _voERunning = false;

	// debug
	private boolean _enableTrace = false;

	// Constant
	private static final String LOG_TAG = "*WEBRTCJ*";
	private static final int RECEIVE_CODEC_FRAMERATE = 30;
	private static final int SEND_CODEC_FRAMERATE = 15;
	private static final int INIT_BITRATE = 400;

	private static final int EXPIRARY_YEAR = 2010;
	private static final int EXPIRARY_MONTH = 10;
	private static final int EXPIRARY_DAY = 22;

	private int _volumeLevel = 204;

	private TabHost mTabHost = null;

	private TabSpec mTabSpecConfig;
	private TabSpec mTabSpecVideo;

	private LinearLayout mLlRemoteSurface = null;
	private LinearLayout mLlLocalSurface = null;

	private Button _btStartStopCall;
	private Button _btSwitchCamera;

	//Global Settings
	private CheckBox _cbVideoSend;
	private boolean _enableVideoSend = true;
	private CheckBox _cbVideoReceive;
	private boolean _enableVideoReceive = true;
	private boolean _enableVideo = true;
	private CheckBox _cbVoice;
	private boolean _enableVoice = false;
	private EditText _etRemoteIp;
	private String _remoteIp = "10.1.100.68";
	private CheckBox _cbLoopback;
	private boolean _loopbackMode = true;

	//Video settings
	private Spinner _spCodecType;
	private int _codecType = 0;
	private Spinner _spCodecSize;
	private int _codecSizeWidth = 352;
	private int _codecSizeHeight = 288;
	private TextView _etVRxPort;
	private int _receivePortVideo = 11111;
	private TextView _etVTxPort;
	private int _destinationPortVideo = 11111;
	private CheckBox _cbEnableNack;
	private boolean _enableNack = false;

	//Audio settings
	private Spinner _spVoiceCodecType;
	private int _voiceCodecType = 5; //PCMU = 5
	private TextView _etARxPort;
	private int _receivePortVoice = 11113;
	private TextView _etATxPort;
	private int _destinationPortVoice = 11113;
	private CheckBox _cbEnableSpeaker;
	private boolean _enableSpeaker = false;
	private CheckBox _cbEnableAGC;
	private boolean _enableAGC = false;
	private CheckBox _cbEnableAECM;
	private boolean _enableAECM = false;
	private CheckBox _cbEnableNS;
	private boolean _enableNS = false;

	//Stats
	private TextView _tvFrameRateI;
	private TextView _tvBitRateI;
	private TextView _tvPacketLoss;
	private TextView _tvFrameRateO;
	private TextView _tvBitRateO;
	private int _frameRateI;
	private int _bitRateI;
	private int _packetLoss;
	private int _frameRateO;
	private int _bitRateO;

	private WakeLock _wakeLock;

	private boolean _usingFrontCamera = false;

	private OrientationEventListener _orientationListener;
	int _currentOrientation = OrientationEventListener.ORIENTATION_UNKNOWN;
	int _currentCameraOrientation =0;


	//Convert current display orientation to how much the camera should be rotated.
    public int GetCameraOrientation(int cameraOrientation)
    {
        int displatyRotation = this.getWindowManager().getDefaultDisplay().getRotation();
        int degrees = 0;
        switch (displatyRotation) {
        case Surface.ROTATION_0: degrees = 0; break;
        case Surface.ROTATION_90: degrees = 90; break;
        case Surface.ROTATION_180: degrees = 180; break;
        case Surface.ROTATION_270: degrees = 270; break;
        }
        int result=0;
        if(cameraOrientation>180)
        {
        	result=(cameraOrientation + degrees) % 360;
        }
        else
        {
        	result=(cameraOrientation - degrees+360) % 360;
        }


        return result;
    }

	public void onConfigurationChanged(Configuration newConfig) {
	  super.onConfigurationChanged(newConfig);
	  int newRotation = GetCameraOrientation(_currentCameraOrientation);
	  if (_viERunning){
		  _ViEAndroidAPI.SetRotation(_cameraId,newRotation);
	  }
	}

	/** Called when the activity is first created. */
	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		requestWindowFeature(Window.FEATURE_NO_TITLE);
		getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN,
				WindowManager.LayoutParams.FLAG_FULLSCREEN);

		PowerManager pm = (PowerManager)this.getSystemService(
                Context.POWER_SERVICE);
		_wakeLock = pm.newWakeLock(
            PowerManager.SCREEN_DIM_WAKE_LOCK, LOG_TAG);

		setContentView(R.layout.tabhost);
		mTabHost = getTabHost();

		//Video tab
        mTabSpecVideo = mTabHost.newTabSpec("tab_video");
        mTabSpecVideo.setIndicator("Video");
        mTabSpecVideo.setContent(R.id.tab_video);
        mTabHost.addTab(mTabSpecVideo);

        //Shared config tab
        mTabHost = getTabHost();
        mTabSpecConfig = mTabHost.newTabSpec("tab_config");
        mTabSpecConfig.setIndicator("Config");
        mTabSpecConfig.setContent(R.id.tab_config);
        mTabHost.addTab(mTabSpecConfig);

        mTabHost.addTab(mTabHost.newTabSpec("tab_vconfig").setIndicator("V. Config").setContent(R.id.tab_vconfig));
        mTabHost.addTab(mTabHost.newTabSpec("tab_aconfig").setIndicator("A. Config").setContent(R.id.tab_aconfig));
        mTabHost.addTab(mTabHost.newTabSpec("tab_stats").setIndicator("Stats").setContent(R.id.tab_stats));

        int childCount = mTabHost.getTabWidget().getChildCount();
        for (int i=0; i<childCount; i++)
        	mTabHost.getTabWidget().getChildAt(i).getLayoutParams().height = 50;

        _orientationListener = new OrientationEventListener(this,SensorManager.SENSOR_DELAY_UI) {
            public void onOrientationChanged (int orientation) {
            	if (orientation != ORIENTATION_UNKNOWN) {
            		_currentOrientation = orientation;
            	}
            }
        };
        _orientationListener.enable ();

		StartMain();
		return;
	}

	private String GetLocalIpAddress() {
		String localIPs = "";
		try {
			for (Enumeration<NetworkInterface> en = NetworkInterface
					.getNetworkInterfaces(); en.hasMoreElements();) {
				NetworkInterface intf = en.nextElement();
				for (Enumeration<InetAddress> enumIpAddr = intf
						.getInetAddresses(); enumIpAddr.hasMoreElements();) {
					InetAddress inetAddress = enumIpAddr.nextElement();
					if (!inetAddress.isLoopbackAddress()) {
						localIPs += inetAddress.getHostAddress().toString() + " ";
						//set the remote ip address the same as the local ip address of the last netif
						_remoteIp = inetAddress.getHostAddress().toString();
					}
				}
			}
		} catch (SocketException ex) {
			Log.e(LOG_TAG, ex.toString());
		}
		return localIPs;
	}

	@Override
	public boolean onKeyDown(int keyCode, KeyEvent event) {
		if (keyCode == KeyEvent.KEYCODE_BACK) {
			if (_viERunning){
				StopAll();
				StartMain();
			}
			finish();
			return true;
		}
		return super.onKeyDown(keyCode, event);
	}

	private void StopAll() {
		if (_ViEAndroidAPI != null) {
			if (_voERunning){
				_voERunning = false;
				StopVoiceEngine();
			}

			if (_viERunning){
				_viERunning = false;
				_ViEAndroidAPI.StopRender(_channel);
				_ViEAndroidAPI.StopReceive(_channel);
				_ViEAndroidAPI.StopSend(_channel);
				_ViEAndroidAPI.RemoveRemoteRenderer(_channel);
				// stop the camera
				_ViEAndroidAPI.StopCamera(_cameraId);
				_ViEAndroidAPI.Terminate();
				mLlRemoteSurface.removeView(_remoteSurfaceView);
				mLlLocalSurface.removeView(_svLocal);
				_remoteSurfaceView = null;

				_svLocal = null;
			}
		}
	}

	private void StartMain() {
		mTabHost.setCurrentTab(0);

		mLlRemoteSurface = (LinearLayout) findViewById(R.id.llRemoteView);
		mLlLocalSurface = (LinearLayout) findViewById(R.id.llLocalView);

		if (null==_ViEAndroidAPI)
			_ViEAndroidAPI = new ViEAndroidJavaAPI(this);

		//setContentView(R.layout.main);

		_btSwitchCamera = (Button)findViewById(R.id.btSwitchCamera);
		_btSwitchCamera.setOnClickListener(this);
		_btStartStopCall = (Button)findViewById(R.id.btStartStopCall);
		_btStartStopCall.setOnClickListener(this);
		findViewById(R.id.btExit).setOnClickListener(this);

		// cleaning
		_remoteSurfaceView = null;
		_svLocal = null;

		// init UI
		ArrayAdapter<?> adapter;
		// video codec
		_spCodecType = (Spinner) findViewById(R.id.spCodecType);
		adapter = ArrayAdapter.createFromResource(this, R.array.codectype,
				android.R.layout.simple_spinner_item);
		adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
		_spCodecType.setAdapter(adapter);
		_spCodecType.setSelection(_codecType);
		_spCodecType.setOnItemSelectedListener(this);

		// voice codec
		_spVoiceCodecType = (Spinner) findViewById(R.id.spVoiceCodecType);
		adapter = ArrayAdapter.createFromResource(this, R.array.voiceCodecType,
				android.R.layout.simple_spinner_item);
		adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
		_spVoiceCodecType.setAdapter(adapter);
		_spVoiceCodecType.setSelection(_voiceCodecType);
		_spVoiceCodecType.setOnItemSelectedListener(this);

		_spCodecSize = (Spinner) findViewById(R.id.spCodecSize);
		adapter = ArrayAdapter.createFromResource(this, R.array.codecSize,
				android.R.layout.simple_spinner_item);
		adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
		_spCodecSize.setAdapter(adapter);
		_spCodecSize.setOnItemSelectedListener(this);

		String ip = GetLocalIpAddress();
		TextView tvLocalIp = (TextView) findViewById(R.id.tvLocalIp);
		tvLocalIp.setText("Local IP address - " + ip);

		_etRemoteIp = (EditText) findViewById(R.id.etRemoteIp);
		_etRemoteIp.setText(_remoteIp);

		_cbLoopback = (CheckBox) findViewById(R.id.cbLoopback);
		_cbLoopback.setChecked(_loopbackMode);

		_cbVoice = (CheckBox) findViewById(R.id.cbVoice);
		_cbVoice.setChecked(_enableVoice);

		_cbVideoSend = (CheckBox) findViewById(R.id.cbVideoSend);
		_cbVideoSend.setChecked(_enableVideoSend);
		_cbVideoReceive = (CheckBox) findViewById(R.id.cbVideoReceive);
		_cbVideoReceive.setChecked(_enableVideoReceive);

		_etVTxPort = (EditText) findViewById(R.id.etVTxPort);
		_etVTxPort.setText(Integer.toString(_destinationPortVideo));

		_etVRxPort = (EditText) findViewById(R.id.etVRxPort);
		_etVRxPort.setText(Integer.toString(_receivePortVideo));

		_etATxPort = (EditText) findViewById(R.id.etATxPort);
		_etATxPort.setText(Integer.toString(_destinationPortVoice));

		_etARxPort = (EditText) findViewById(R.id.etARxPort);
		_etARxPort.setText(Integer.toString(_receivePortVoice));

		_cbEnableNack = (CheckBox) findViewById(R.id.cbNack);
		_cbEnableNack.setChecked(_enableNack);

		_cbEnableSpeaker = (CheckBox) findViewById(R.id.cbSpeaker);
		_cbEnableSpeaker.setChecked(_enableSpeaker);
		_cbEnableAGC = (CheckBox) findViewById(R.id.cbAutoGainControl);
		_cbEnableAGC.setChecked(_enableAGC);
		_cbEnableAECM = (CheckBox) findViewById(R.id.cbAECM);
		_cbEnableAECM.setChecked(_enableAECM);
		_cbEnableNS = (CheckBox) findViewById(R.id.cbNoiseSuppression);
		_cbEnableNS.setChecked(_enableNS);

		_cbEnableNack.setOnClickListener(this);
		_cbEnableSpeaker.setOnClickListener(this);
		_cbEnableAECM.setOnClickListener(this);
				//
		_cbEnableAGC.setOnClickListener(this);
		_cbEnableNS.setOnClickListener(this);

		_tvFrameRateI = (TextView) findViewById(R.id.tvFrameRateI);
		_tvBitRateI = (TextView) findViewById(R.id.tvBitRateI);
		_tvPacketLoss = (TextView) findViewById(R.id.tvPacketLoss);
		_tvFrameRateO = (TextView) findViewById(R.id.tvFrameRateO);
		_tvBitRateO = (TextView) findViewById(R.id.tvBitRateO);

	}

	@Override
	protected void onPause() {
		super.onPause();
		/*if (_remoteSurfaceView != null)
			_glSurfaceView.onPause();*/
	}

	@Override
	protected void onResume() {
		super.onResume();
		/*if (_glSurfaceView != null)
			_glSurfaceView.onResume();*/
	}

	private void StartCall() {
		int ret = 0;

		if (_enableVoice){
			SetupVoE();
			StartVoiceEngine();
		}

		if (_enableVideo){
			if (_enableVideoSend){
				// camera and preview surface
				_svLocal=ViERenderer.CreateLocalRenderer(this);
			}

			ret = _ViEAndroidAPI.GetVideoEngine();
			ret = _ViEAndroidAPI.Init(_enableTrace);
			_channel = _ViEAndroidAPI.CreateChannel(_voiceChannel);
			ret = _ViEAndroidAPI.SetLocalReceiver(_channel,
					_receivePortVideo);
			ret = _ViEAndroidAPI.SetSendDestination(_channel,
					_destinationPortVideo, _remoteIp.getBytes());

			if(_codecType==3){//H.264 accordign to arrrys.xml
				ret = _ViEAndroidAPI.EnablePLI(_channel, true);
			}
			if (_enableVideoReceive){

				if(android.os.Build.MANUFACTURER.equals("samsung"))
				{
					_remoteSurfaceView = ViERenderer.CreateRenderer(this,true); // Create an Open GL renderer
					ret = _ViEAndroidAPI.AddRemoteRenderer(_channel,_remoteSurfaceView);
				}
				else
				{
					_remoteSurfaceView= ViERenderer.CreateRenderer(this,false);
					ret = _ViEAndroidAPI.AddRemoteRenderer(_channel,_remoteSurfaceView);
				}

				ret = _ViEAndroidAPI.SetReceiveCodec(_channel,
						_codecType, INIT_BITRATE, _codecSizeWidth, _codecSizeHeight, RECEIVE_CODEC_FRAMERATE);
				ret = _ViEAndroidAPI.StartRender(_channel);
				ret = _ViEAndroidAPI.StartReceive(_channel);
			}

			if (_enableVideoSend){
				_currentCameraOrientation=_ViEAndroidAPI.GetCameraOrientation(_usingFrontCamera?1:0);
				ret = _ViEAndroidAPI.SetSendCodec(_channel,
						_codecType, INIT_BITRATE, _codecSizeWidth, _codecSizeHeight, SEND_CODEC_FRAMERATE);
				int cameraId = _ViEAndroidAPI.StartCamera(_channel,_usingFrontCamera?1:0);

				if(cameraId>0)
				{
					_cameraId=cameraId;
					int neededRotation=GetCameraOrientation(_currentCameraOrientation);
					_ViEAndroidAPI.SetRotation(_cameraId,neededRotation);
				}
				else
				{
					ret=cameraId;
				}
				ret = _ViEAndroidAPI.StartSend(_channel);
			}

			ret = _ViEAndroidAPI.SetCallback(_channel,this);

			if (_enableVideoSend){
				if (mLlLocalSurface!=null)
					mLlLocalSurface.addView(_svLocal);
			}

			if (_enableVideoReceive){
				if (mLlRemoteSurface!=null)
					mLlRemoteSurface.addView(_remoteSurfaceView);
			}

			_viERunning = true;
		}

	}

	private void DemoLog(String msg) {
		Log.d("*WEBRTC*", msg);
	}


	private void StopVoiceEngine() {
		// Stop send
		if (0 != _ViEAndroidAPI.VoE_StopSend(_voiceChannel)) {
			DemoLog("VoE stop send failed");
		}

		// Stop listen
		if (0 != _ViEAndroidAPI.VoE_StopListen(_voiceChannel)) {
			DemoLog("VoE stop listen failed");
		}

		// Stop playout
		if (0 != _ViEAndroidAPI.VoE_StopPlayout(_voiceChannel)) {
			DemoLog("VoE stop playout failed");
		}

		if (0 != _ViEAndroidAPI.VoE_DeleteChannel(_voiceChannel)) {
			DemoLog("VoE delete channel failed");
		}
		_voiceChannel=-1;

		// Terminate
		if (0 != _ViEAndroidAPI.VoE_Terminate()) {
			DemoLog("VoE terminate failed");
		}
	}

	private void SetupVoE() {
		// Create VoiceEngine
		_ViEAndroidAPI.VoE_Create(this); // Error logging is done in native API wrapper

		// Initialize
		if (0 != _ViEAndroidAPI.VoE_Init(_enableTrace))
		{
			DemoLog("VoE init failed");
		}

		// Create channel
		_voiceChannel = _ViEAndroidAPI.VoE_CreateChannel();
		if (0 != _voiceChannel) {
			DemoLog("VoE create channel failed");
		}

		// Suggest to use the voice call audio stream for hardware volume controls
		setVolumeControlStream(AudioManager.STREAM_VOICE_CALL);
	}

	private int StartVoiceEngine() {
		// Set local receiver
		if (0 != _ViEAndroidAPI.VoE_SetLocalReceiver(_voiceChannel, _receivePortVoice)) {
			DemoLog("VoE set local receiver failed");
		}

		if (0 != _ViEAndroidAPI.VoE_StartListen(_voiceChannel)) {
			DemoLog("VoE start listen failed");
		}

		// Route audio
		RouteAudio(_enableSpeaker);

		// set volume to default value
		if (0 != _ViEAndroidAPI.VoE_SetSpeakerVolume(_volumeLevel))
		{
			DemoLog("VoE set speaker volume failed");
		}

		// Start playout
		if (0 != _ViEAndroidAPI.VoE_StartPlayout(_voiceChannel)) {
			DemoLog("VoE start playout failed");
		}

		if (0 != _ViEAndroidAPI.VoE_SetSendDestination(_voiceChannel, _destinationPortVoice,
				_remoteIp)) {
			DemoLog("VoE set send  destination failed");
		}

		// 0 = iPCM-wb, 5 = PCMU
		if (0 != _ViEAndroidAPI.VoE_SetSendCodec(_voiceChannel, _voiceCodecType)) {
			DemoLog("VoE set send codec failed");
		}

		if (0 != _ViEAndroidAPI.VoE_SetECStatus(_enableAECM, 5, 0, 28)){
			DemoLog("VoE set EC Status failed");
		}

		if (0 != _ViEAndroidAPI.VoE_StartSend(_voiceChannel)) {
			DemoLog("VoE start send failed");
		}

		_voERunning = true;
		return 0;
	}

	private void RouteAudio(boolean enableSpeaker) {
		int sdkVersion = Integer.parseInt(android.os.Build.VERSION.SDK);
		if (sdkVersion >= 5) {
			AudioManager am = (AudioManager) this.getSystemService(Context.AUDIO_SERVICE);
			am.setSpeakerphoneOn(enableSpeaker);
		}else{
			if (0 != _ViEAndroidAPI.VoE_SetLoudspeakerStatus(enableSpeaker))
			{
				DemoLog("VoE set louspeaker status failed");
			}
		}
	}


	public void onClick(View arg0) {
		switch (arg0.getId()) {
		case R.id.btSwitchCamera:
			if (_usingFrontCamera ){
				_btSwitchCamera.setText(R.string.frontCamera);
			}else{
				_btSwitchCamera.setText(R.string.backCamera);
			}
			_usingFrontCamera = !_usingFrontCamera;

			if (_viERunning){
				_currentCameraOrientation=_ViEAndroidAPI.GetCameraOrientation(_usingFrontCamera?1:0);
				_ViEAndroidAPI.StopCamera(_cameraId);
				mLlLocalSurface.removeView(_svLocal);

				_ViEAndroidAPI.StartCamera(_channel,_usingFrontCamera?1:0);
				mLlLocalSurface.addView(_svLocal);
				int neededRotation=GetCameraOrientation(_currentCameraOrientation);
				_ViEAndroidAPI.SetRotation(_cameraId,neededRotation);
			}
			break;
		case R.id.btStartStopCall:
			ReadSettings();
			if (_viERunning || _voERunning){
				StopAll();
				_wakeLock.release();//release the wake lock
				_btStartStopCall.setText(R.string.startCall);
			}
			else if (_enableVoice || _enableVideo){
				StartCall();
				_wakeLock.acquire();//screen stay on during the call
				_btStartStopCall.setText(R.string.stopCall);
			}
			break;
		case R.id.btExit:
			StopAll();
			finish();
			break;
		case R.id.cbNack:
			_enableNack  = _cbEnableNack.isChecked();
			if (_viERunning){
				_ViEAndroidAPI.EnableNACK(_channel, _enableNack);
			}
			break;
		case R.id.cbSpeaker:
			_enableSpeaker = _cbEnableSpeaker.isChecked();
			if (_voERunning){
				RouteAudio(_enableSpeaker);
			}
			break;
		case R.id.cbAutoGainControl:
			_enableAGC=_cbEnableAGC.isChecked();
			if(_voERunning)
			{
				//Enable AGC default mode.
				_ViEAndroidAPI.VoE_SetAGCStatus(_enableAGC,1);
			}
			break;
		case R.id.cbNoiseSuppression:
			_enableNS=_cbEnableNS.isChecked();
			if(_voERunning)
			{
				//Enable NS default mode.
				_ViEAndroidAPI.VoE_SetNSStatus(_enableNS, 1);
			}
			break;
		case R.id.cbAECM:
			_enableAECM = _cbEnableAECM.isChecked();
			if (_voERunning){
				//EC_AECM=5
				//AECM_DEFAULT=0
				_ViEAndroidAPI.VoE_SetECStatus(_enableAECM, 5, 0, 28);
			}
			break;
		}
	}

	private void ReadSettings() {
		_codecType = _spCodecType.getSelectedItemPosition();
		_voiceCodecType = _spVoiceCodecType.getSelectedItemPosition();

		String sCodecSize = _spCodecSize.getSelectedItem().toString();
		String[] aCodecSize = sCodecSize.split("x");
		_codecSizeWidth = Integer.parseInt(aCodecSize[0]);
		_codecSizeHeight = Integer.parseInt(aCodecSize[1]);

		_loopbackMode  = _cbLoopback.isChecked();
		_enableVoice  = _cbVoice.isChecked();
		_enableVideoSend = _cbVideoSend.isChecked();
		_enableVideoReceive = _cbVideoReceive.isChecked();
		_enableVideo = _enableVideoSend || _enableVideoReceive;

		_destinationPortVideo = Integer.parseInt(_etVTxPort.getText().toString());
		_receivePortVideo = Integer.parseInt(_etVRxPort.getText().toString());
		_destinationPortVoice = Integer.parseInt(_etATxPort.getText().toString());
		_receivePortVoice = Integer.parseInt(_etARxPort.getText().toString());

		_enableNack  = _cbEnableNack.isChecked();
		_enableSpeaker  = _cbEnableSpeaker.isChecked();
		_enableAGC  = _cbEnableAGC.isChecked();
		_enableAECM  = _cbEnableAECM.isChecked();
		_enableNS  = _cbEnableNS.isChecked();

		if (_loopbackMode)
			_remoteIp = "127.0.0.1";
		else
			_remoteIp = _etRemoteIp.getText().toString();
	}


	public void onItemSelected(AdapterView<?> adapterView, View view, int position, long id) {
		if ((adapterView==_spCodecType || adapterView==_spCodecSize) && _viERunning){
			ReadSettings();
			//change the codectype
			if (_enableVideoReceive){
				if (0 !=_ViEAndroidAPI.SetReceiveCodec(_channel,
						_codecType, INIT_BITRATE, _codecSizeWidth, _codecSizeHeight, RECEIVE_CODEC_FRAMERATE))
					DemoLog("ViE set receive codec failed");
			}
			if (_enableVideoSend){
				if (0!=_ViEAndroidAPI.SetSendCodec(_channel,
						_codecType, INIT_BITRATE, _codecSizeWidth, _codecSizeHeight, SEND_CODEC_FRAMERATE))
					DemoLog("ViE set send codec failed");
			}
		}else if ((adapterView==_spVoiceCodecType) && _voERunning){
			//change voice engine codec
			ReadSettings();
			if (0 != _ViEAndroidAPI.VoE_SetSendCodec(_voiceChannel, _voiceCodecType)) {
				DemoLog("VoE set send codec failed");
			}
		}
	}


	public void onNothingSelected(AdapterView<?> arg0) {
		DemoLog("No setting selected");
	}


	public int UpdateStats(int frameRateI, int bitRateI, int packetLoss,
			int frameRateO, int bitRateO) {
		_frameRateI = frameRateI;
		_bitRateI = bitRateI;
		_packetLoss = packetLoss;
		_frameRateO = frameRateO;
		_bitRateO = bitRateO;
		runOnUiThread(new Runnable() {
			public void run() {
				_tvFrameRateI.setText("Incoming FrameRate - " + Integer.toString(_frameRateI));
				_tvBitRateI.setText("Incoming BitRate - " + Integer.toString(_bitRateI));
				_tvPacketLoss.setText("Incoming Packet Loss - " + Integer.toString(_packetLoss));
				_tvFrameRateO.setText("Send FrameRate - " + Integer.toString(_frameRateO));
				_tvBitRateO.setText("Send BitRate - " + Integer.toString(_bitRateO));
			}
		});
		return 0;
	}
}
