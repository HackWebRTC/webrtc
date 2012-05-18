/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
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
import android.content.pm.ActivityInfo;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.DashPathEffect;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.hardware.SensorManager;
import android.media.AudioManager;
import android.os.Bundle;
import android.os.PowerManager;
import android.os.PowerManager.WakeLock;

import android.util.Log;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.Surface;
import android.view.SurfaceView;
import android.view.View;
import android.view.Display;
import android.view.Window;
import android.view.WindowManager;
import android.view.WindowManager.LayoutParams;

import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.CheckBox;

import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.RadioGroup;
import android.widget.Spinner;
import android.widget.TabHost;
import android.widget.TextView;
import android.widget.AdapterView.OnItemSelectedListener;
import android.widget.TabHost.TabSpec;
import android.view.OrientationEventListener;

public class WebRTCDemo extends TabActivity implements IViEAndroidCallback,
                                                View.OnClickListener,
                                                OnItemSelectedListener {
    private ViEAndroidJavaAPI ViEAndroidAPI = null;

    // remote renderer
    private SurfaceView remoteSurfaceView = null;

    // local renderer and camera
    private SurfaceView svLocal = null;

    // channel number
    private int channel;
    private int cameraId;
    private int voiceChannel = -1;

    // flags
    private boolean viERunning = false;
    private boolean voERunning = false;

    // debug
    private boolean enableTrace = false;

    // Constant
    private static final String TAG = "WEBRTC";
    private static final int RECEIVE_CODEC_FRAMERATE = 30;
    private static final int SEND_CODEC_FRAMERATE = 30;
    private static final int INIT_BITRATE = 400;

    private static final int EXPIRARY_YEAR = 2010;
    private static final int EXPIRARY_MONTH = 10;
    private static final int EXPIRARY_DAY = 22;

    private int volumeLevel = 204;

    private TabHost mTabHost = null;

    private TabSpec mTabSpecConfig;
    private TabSpec mTabSpecVideo;

    private LinearLayout mLlRemoteSurface = null;
    private LinearLayout mLlLocalSurface = null;

    private Button btStartStopCall;
    private Button btSwitchCamera;

    // Global Settings
    private CheckBox cbVideoSend;
    private boolean enableVideoSend = true;
    private CheckBox cbVideoReceive;
    private boolean enableVideoReceive = true;
    private boolean enableVideo = true;
    private CheckBox cbVoice;
    private boolean enableVoice = true;
    private EditText etRemoteIp;
    private String remoteIp = "10.1.100.68";
    private CheckBox cbLoopback;
    private boolean loopbackMode = true;
    private CheckBox cbStats;
    private boolean isStatsOn = true;
    private boolean useOpenGLRender = true;

    // Video settings
    private Spinner spCodecType;
    private int codecType = 0;
    private Spinner spCodecSize;
    private int codecSizeWidth = 352;
    private int codecSizeHeight = 288;
    private TextView etVRxPort;
    private int receivePortVideo = 11111;
    private TextView etVTxPort;
    private int destinationPortVideo = 11111;
    private CheckBox cbEnableNack;
    private boolean enableNack = false;

    // Audio settings
    private Spinner spVoiceCodecType;
    private int voiceCodecType = 5; // PCMU = 5
    private TextView etARxPort;
    private int receivePortVoice = 11113;
    private TextView etATxPort;
    private int destinationPortVoice = 11113;
    private CheckBox cbEnableSpeaker;
    private boolean enableSpeaker = false;
    private CheckBox cbEnableAGC;
    private boolean enableAGC = false;
    private CheckBox cbEnableAECM;
    private boolean enableAECM = false;
    private CheckBox cbEnableNS;
    private boolean enableNS = false;

    // Stats variables
    private int frameRateI;
    private int bitRateI;
    private int packetLoss;
    private int frameRateO;
    private int bitRateO;

    private WakeLock wakeLock;

    private boolean usingFrontCamera = false;

    private OrientationEventListener orientationListener;
    int currentOrientation = OrientationEventListener.ORIENTATION_UNKNOWN;
    int currentCameraOrientation = 0;

    private StatsView statsView = null;

    public int GetCameraOrientation(int cameraOrientation) {
        Display display = this.getWindowManager().getDefaultDisplay();
        int displatyRotation = display.getRotation();
        int degrees = 0;
        switch (displatyRotation) {
            case Surface.ROTATION_0: degrees = 0; break;
            case Surface.ROTATION_90: degrees = 90; break;
            case Surface.ROTATION_180: degrees = 180; break;
            case Surface.ROTATION_270: degrees = 270; break;
        }
        int result=0;
        if(cameraOrientation>180) {
            result=(cameraOrientation + degrees) % 360;
        }
        else {
            result=(cameraOrientation - degrees+360) % 360;
        }
        return result;
    }

    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        int newRotation = GetCameraOrientation(currentCameraOrientation);
        if (viERunning){
            ViEAndroidAPI.SetRotation(cameraId,newRotation);
        }
    }

    // Called when the activity is first created.
    @Override
    public void onCreate(Bundle savedInstanceState) {
        Log.d(TAG, "onCreate");

        super.onCreate(savedInstanceState);
        requestWindowFeature(Window.FEATURE_NO_TITLE);
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN,
                WindowManager.LayoutParams.FLAG_FULLSCREEN);
        // Set screen orientation
        setRequestedOrientation (ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);

        PowerManager pm = (PowerManager)this.getSystemService(
            Context.POWER_SERVICE);
        wakeLock = pm.newWakeLock(
            PowerManager.SCREEN_DIM_WAKE_LOCK, TAG);

        setContentView(R.layout.tabhost);
        mTabHost = getTabHost();

        // Main tab
        mTabSpecVideo = mTabHost.newTabSpec("tab_video");
        mTabSpecVideo.setIndicator("Main");
        mTabSpecVideo.setContent(R.id.tab_video);
        mTabHost.addTab(mTabSpecVideo);

        // Shared config tab
        mTabHost = getTabHost();
        mTabSpecConfig = mTabHost.newTabSpec("tab_config");
        mTabSpecConfig.setIndicator("Settings");
        mTabSpecConfig.setContent(R.id.tab_config);
        mTabHost.addTab(mTabSpecConfig);

        TabSpec mTabv;
        mTabv = mTabHost.newTabSpec("tab_vconfig");
        mTabv.setIndicator("Video");
        mTabv.setContent(R.id.tab_vconfig);
        mTabHost.addTab(mTabv);
        TabSpec mTaba;
        mTaba = mTabHost.newTabSpec("tab_aconfig");
        mTaba.setIndicator("Audio");
        mTaba.setContent(R.id.tab_aconfig);
        mTabHost.addTab(mTaba);

        int childCount = mTabHost.getTabWidget().getChildCount();
        for (int i=0; i<childCount; i++)
            mTabHost.getTabWidget().getChildAt(i).getLayoutParams().height = 50;

        orientationListener =
                new OrientationEventListener(this,SensorManager.SENSOR_DELAY_UI) {
                    public void onOrientationChanged (int orientation) {
                        if (orientation != ORIENTATION_UNKNOWN) {
                            currentOrientation = orientation;
                        }
                    }
                };
        orientationListener.enable ();

        StartMain();
        return;
    }

    private class StatsView extends View{
        public StatsView(Context context){
            super(context);
        }

        @Override protected void onDraw(Canvas canvas) {
            super.onDraw(canvas);

            Paint mLoadPaint = new Paint();
            mLoadPaint.setAntiAlias(true);
            mLoadPaint.setTextSize(16);
            mLoadPaint.setARGB(255, 255, 255, 255);

            String mLoadText;
            mLoadText = "> " + frameRateI + " fps/" + bitRateI + "k bps/ " + packetLoss;
            canvas.drawText(mLoadText, 4, 172, mLoadPaint);
            mLoadText = "< " + frameRateO + " fps/ " + bitRateO + "k bps";
            canvas.drawText(mLoadText, 4, 192, mLoadPaint);

            updateDisplay();
        }

        void updateDisplay() {
            invalidate();
        }
    }

    private String GetLocalIpAddress() {
        String localIPs = "";
        try {
            for (Enumeration<NetworkInterface> en = NetworkInterface
                         .getNetworkInterfaces(); en.hasMoreElements();) {
                NetworkInterface intf = en.nextElement();
                for (Enumeration<InetAddress> enumIpAddr =
                             intf.getInetAddresses();
                     enumIpAddr.hasMoreElements(); ) {
                    InetAddress inetAddress = enumIpAddr.nextElement();
                    if (!inetAddress.isLoopbackAddress()) {
                        localIPs +=
                                inetAddress.getHostAddress().toString() + " ";
                        // Set the remote ip address the same as
                        // the local ip address of the last netif
                        remoteIp = inetAddress.getHostAddress().toString();
                    }
                }
            }
        } catch (SocketException ex) {
            Log.e(TAG, ex.toString());
        }
        return localIPs;
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        if (keyCode == KeyEvent.KEYCODE_BACK) {
            if (viERunning) {
                StopAll();
                StartMain();
            }
            finish();
            return true;
        }
        return super.onKeyDown(keyCode, event);
    }

    private void StopAll() {
        Log.d(TAG, "StopAll");

        if (ViEAndroidAPI != null) {
            if (voERunning) {
                voERunning = false;
                StopVoiceEngine();
            }

            if (viERunning) {
                viERunning = false;
                ViEAndroidAPI.StopRender(channel);
                ViEAndroidAPI.StopReceive(channel);
                ViEAndroidAPI.StopSend(channel);
                ViEAndroidAPI.RemoveRemoteRenderer(channel);
                ViEAndroidAPI.StopCamera(cameraId);
                ViEAndroidAPI.Terminate();
                mLlRemoteSurface.removeView(remoteSurfaceView);
                mLlLocalSurface.removeView(svLocal);
                remoteSurfaceView = null;
                svLocal = null;
            }
        }
    }

    private void StartMain() {
        mTabHost.setCurrentTab(0);

        mLlRemoteSurface = (LinearLayout) findViewById(R.id.llRemoteView);
        mLlLocalSurface = (LinearLayout) findViewById(R.id.llLocalView);

        if (null == ViEAndroidAPI)
            ViEAndroidAPI = new ViEAndroidJavaAPI(this);

        btSwitchCamera = (Button)findViewById(R.id.btSwitchCamera);
        btSwitchCamera.setOnClickListener(this);
        btStartStopCall = (Button)findViewById(R.id.btStartStopCall);
        btStartStopCall.setOnClickListener(this);
        findViewById(R.id.btExit).setOnClickListener(this);

        // cleaning
        remoteSurfaceView = null;
        svLocal = null;

        // init UI
        ArrayAdapter<?> adapter;

        int resource = android.R.layout.simple_spinner_item;
        int dropdownRes = android.R.layout.simple_spinner_dropdown_item;

        // video codec
        spCodecType = (Spinner) findViewById(R.id.spCodecType);
        adapter = ArrayAdapter.createFromResource(this,
                R.array.codectype,
                resource);
        adapter.setDropDownViewResource(dropdownRes);
        spCodecType.setAdapter(adapter);
        spCodecType.setSelection(codecType);
        spCodecType.setOnItemSelectedListener(this);

        RadioGroup radioGroup = (RadioGroup)findViewById(R.id.radio_group1);
        radioGroup.clearCheck();
        if (useOpenGLRender == true) {
            radioGroup.check(R.id.radio_opengl);
        }
        else {
            radioGroup.check(R.id.radio_surface);
        }

        // voice codec
        spVoiceCodecType = (Spinner) findViewById(R.id.spVoiceCodecType);
        adapter = ArrayAdapter.createFromResource(this, R.array.voiceCodecType,
                resource);
        adapter.setDropDownViewResource(dropdownRes);
        spVoiceCodecType.setAdapter(adapter);
        spVoiceCodecType.setSelection(voiceCodecType);
        spVoiceCodecType.setOnItemSelectedListener(this);

        spCodecSize = (Spinner) findViewById(R.id.spCodecSize);
        adapter = ArrayAdapter.createFromResource(this, R.array.codecSize,
                resource);
        adapter.setDropDownViewResource(dropdownRes);
        spCodecSize.setAdapter(adapter);
        spCodecSize.setOnItemSelectedListener(this);

        etRemoteIp = (EditText) findViewById(R.id.etRemoteIp);
        etRemoteIp.setText(remoteIp);

        cbLoopback = (CheckBox) findViewById(R.id.cbLoopback);
        cbLoopback.setChecked(loopbackMode);

        cbStats = (CheckBox) findViewById(R.id.cbStats);
        cbStats.setChecked(isStatsOn);

        cbVoice = (CheckBox) findViewById(R.id.cbVoice);
        cbVoice.setChecked(enableVoice);

        cbVideoSend = (CheckBox) findViewById(R.id.cbVideoSend);
        cbVideoSend.setChecked(enableVideoSend);
        cbVideoReceive = (CheckBox) findViewById(R.id.cbVideoReceive);
        cbVideoReceive.setChecked(enableVideoReceive);

        etVTxPort = (EditText) findViewById(R.id.etVTxPort);
        etVTxPort.setText(Integer.toString(destinationPortVideo));

        etVRxPort = (EditText) findViewById(R.id.etVRxPort);
        etVRxPort.setText(Integer.toString(receivePortVideo));

        etATxPort = (EditText) findViewById(R.id.etATxPort);
        etATxPort.setText(Integer.toString(destinationPortVoice));

        etARxPort = (EditText) findViewById(R.id.etARxPort);
        etARxPort.setText(Integer.toString(receivePortVoice));

        cbEnableNack = (CheckBox) findViewById(R.id.cbNack);
        cbEnableNack.setChecked(enableNack);

        cbEnableSpeaker = (CheckBox) findViewById(R.id.cbSpeaker);
        cbEnableSpeaker.setChecked(enableSpeaker);
        cbEnableAGC = (CheckBox) findViewById(R.id.cbAutoGainControl);
        cbEnableAGC.setChecked(enableAGC);
        cbEnableAECM = (CheckBox) findViewById(R.id.cbAECM);
        cbEnableAECM.setChecked(enableAECM);
        cbEnableNS = (CheckBox) findViewById(R.id.cbNoiseSuppression);
        cbEnableNS.setChecked(enableNS);

        cbStats.setOnClickListener(this);
        cbEnableNack.setOnClickListener(this);
        cbEnableSpeaker.setOnClickListener(this);
        cbEnableAECM.setOnClickListener(this);

        cbEnableAGC.setOnClickListener(this);
        cbEnableNS.setOnClickListener(this);
    }

    private void StartCall() {
        int ret = 0;

        if (enableVoice) {
            SetupVoE();
            StartVoiceEngine();
        }

        if (enableVideo) {
            if (enableVideoSend) {
                // camera and preview surface
                svLocal = ViERenderer.CreateLocalRenderer(this);
            }

            ret = ViEAndroidAPI.GetVideoEngine();
            ret = ViEAndroidAPI.Init(enableTrace);
            channel = ViEAndroidAPI.CreateChannel(voiceChannel);
            ret = ViEAndroidAPI.SetLocalReceiver(channel,
                    receivePortVideo);
            ret = ViEAndroidAPI.SetSendDestination(channel,
                    destinationPortVideo,
                    remoteIp.getBytes());

            if (enableVideoReceive) {
                if(useOpenGLRender) {
                    Log.v(TAG, "Create OpenGL Render");
                    remoteSurfaceView = ViERenderer.CreateRenderer(this, true);
                    ret = ViEAndroidAPI.AddRemoteRenderer(channel, remoteSurfaceView);
                }
                else {
                    Log.v(TAG, "Create SurfaceView Render");
                    remoteSurfaceView = ViERenderer.CreateRenderer(this, false);
                    ret = ViEAndroidAPI.AddRemoteRenderer(channel, remoteSurfaceView);
                }

                ret = ViEAndroidAPI.SetReceiveCodec(channel,
                        codecType,
                        INIT_BITRATE,
                        codecSizeWidth,
                        codecSizeHeight,
                        RECEIVE_CODEC_FRAMERATE);
                ret = ViEAndroidAPI.StartRender(channel);
                ret = ViEAndroidAPI.StartReceive(channel);
            }

            if (enableVideoSend) {
                currentCameraOrientation =
                        ViEAndroidAPI.GetCameraOrientation(usingFrontCamera?1:0);
                ret = ViEAndroidAPI.SetSendCodec(channel, codecType, INIT_BITRATE,
                        codecSizeWidth, codecSizeHeight, SEND_CODEC_FRAMERATE);
                int camId = ViEAndroidAPI.StartCamera(channel, usingFrontCamera?1:0);

                if(camId > 0) {
                    cameraId = camId;
                    int neededRotation = GetCameraOrientation(currentCameraOrientation);
                    ViEAndroidAPI.SetRotation(cameraId, neededRotation);
                }
                else {
                    ret = camId;
                }
                ret = ViEAndroidAPI.StartSend(channel);
            }

            ret = ViEAndroidAPI.SetCallback(channel, this);

            if (enableVideoSend) {
                if (mLlLocalSurface != null)
                    mLlLocalSurface.addView(svLocal);
            }

            if (enableVideoReceive) {
                if (mLlRemoteSurface != null) {
                    mLlRemoteSurface.addView(remoteSurfaceView);
                }
            }

            isStatsOn = cbStats.isChecked();
            if (isStatsOn) {
                AddStatsView();
            }
            else {
                RemoveSatsView();
            }

            viERunning = true;
        }
    }

    private void StopVoiceEngine() {
        // Stop send
        if (0 != ViEAndroidAPI.VoE_StopSend(voiceChannel)) {
            Log.d(TAG, "VoE stop send failed");
        }

        // Stop listen
        if (0 != ViEAndroidAPI.VoE_StopListen(voiceChannel)) {
            Log.d(TAG, "VoE stop listen failed");
        }

        // Stop playout
        if (0 != ViEAndroidAPI.VoE_StopPlayout(voiceChannel)) {
            Log.d(TAG, "VoE stop playout failed");
        }

        if (0 != ViEAndroidAPI.VoE_DeleteChannel(voiceChannel)) {
            Log.d(TAG, "VoE delete channel failed");
        }
        voiceChannel=-1;

        // Terminate
        if (0 != ViEAndroidAPI.VoE_Terminate()) {
            Log.d(TAG, "VoE terminate failed");
        }
    }

    private void SetupVoE() {
        // Create VoiceEngine
        // Error logging is done in native API wrapper
        ViEAndroidAPI.VoE_Create();

        // Initialize
        if (0 != ViEAndroidAPI.VoE_Init(enableTrace)) {
            Log.d(TAG, "VoE init failed");
        }

        // Create channel
        voiceChannel = ViEAndroidAPI.VoE_CreateChannel();
        if (0 != voiceChannel) {
            Log.d(TAG, "VoE create channel failed");
        }

        // Suggest to use the voice call audio stream for hardware volume controls
        setVolumeControlStream(AudioManager.STREAM_VOICE_CALL);
    }

    private int StartVoiceEngine() {
        // Set local receiver
        if (0 != ViEAndroidAPI.VoE_SetLocalReceiver(voiceChannel,
                        receivePortVoice)) {
            Log.d(TAG, "VoE set local receiver failed");
        }

        if (0 != ViEAndroidAPI.VoE_StartListen(voiceChannel)) {
            Log.d(TAG, "VoE start listen failed");
        }

        // Route audio
        RouteAudio(enableSpeaker);

        // set volume to default value
        if (0 != ViEAndroidAPI.VoE_SetSpeakerVolume(volumeLevel)) {
            Log.d(TAG, "VoE set speaker volume failed");
        }

        // Start playout
        if (0 != ViEAndroidAPI.VoE_StartPlayout(voiceChannel)) {
            Log.d(TAG, "VoE start playout failed");
        }

        if (0 != ViEAndroidAPI.VoE_SetSendDestination(voiceChannel,
                        destinationPortVoice,
                        remoteIp)) {
            Log.d(TAG, "VoE set send  destination failed");
        }

        // 0 = iPCM-wb, 5 = PCMU
        if (0 != ViEAndroidAPI.VoE_SetSendCodec(voiceChannel, voiceCodecType)) {
            Log.d(TAG, "VoE set send codec failed");
        }

        if (0 != ViEAndroidAPI.VoE_SetECStatus(enableAECM, 5, 0, 28)){
            Log.d(TAG, "VoE set EC Status failed");
        }

        if (0 != ViEAndroidAPI.VoE_StartSend(voiceChannel)) {
            Log.d(TAG, "VoE start send failed");
        }

        voERunning = true;
        return 0;
    }

    private void RouteAudio(boolean enableSpeaker) {
        int sdkVersion = Integer.parseInt(android.os.Build.VERSION.SDK);
        if (sdkVersion >= 5) {
            AudioManager am =
                    (AudioManager) this.getSystemService(Context.AUDIO_SERVICE);
            am.setSpeakerphoneOn(enableSpeaker);
        }
        else {
            if (0 != ViEAndroidAPI.VoE_SetLoudspeakerStatus(enableSpeaker)) {
                Log.d(TAG, "VoE set louspeaker status failed");
            }
        }
    }

    public void onClick(View arg0) {
        switch (arg0.getId()) {
            case R.id.btSwitchCamera:
                if (usingFrontCamera ){
                    btSwitchCamera.setText(R.string.frontCamera);
                }
                else {
                    btSwitchCamera.setText(R.string.backCamera);
                }
                usingFrontCamera = !usingFrontCamera;

                if (viERunning) {
                    currentCameraOrientation =
                            ViEAndroidAPI.GetCameraOrientation(usingFrontCamera?1:0);
                    ViEAndroidAPI.StopCamera(cameraId);
                    mLlLocalSurface.removeView(svLocal);

                    ViEAndroidAPI.StartCamera(channel,usingFrontCamera?1:0);
                    mLlLocalSurface.addView(svLocal);
                    int neededRotation = GetCameraOrientation(currentCameraOrientation);
                    ViEAndroidAPI.SetRotation(cameraId, neededRotation);
                }
                break;
            case R.id.btStartStopCall:
                ReadSettings();
                if (viERunning || voERunning) {
                    StopAll();
                    wakeLock.release(); // release the wake lock
                    btStartStopCall.setText(R.string.startCall);
                }
                else if (enableVoice || enableVideo){
                    StartCall();
                    wakeLock.acquire(); // screen stay on during the call
                    btStartStopCall.setText(R.string.stopCall);
                }
                break;
            case R.id.btExit:
                StopAll();
                finish();
                break;
            case R.id.cbStats:
                isStatsOn = cbStats.isChecked();
                if (isStatsOn) {
                    AddStatsView();
                }
                else {
                    RemoveSatsView();
                }
                break;
            case R.id.radio_surface:
                useOpenGLRender = false;
                break;
            case R.id.radio_opengl:
                useOpenGLRender = true;
                break;
            case R.id.cbNack:
                enableNack  = cbEnableNack.isChecked();
                if (viERunning) {
                    ViEAndroidAPI.EnableNACK(channel, enableNack);
                }
                break;
            case R.id.cbSpeaker:
                enableSpeaker = cbEnableSpeaker.isChecked();
                if (voERunning){
                    RouteAudio(enableSpeaker);
                }
                break;
            case R.id.cbAutoGainControl:
                enableAGC=cbEnableAGC.isChecked();
                if(voERunning) {
                    // Enable AGC default mode.
                    ViEAndroidAPI.VoE_SetAGCStatus(enableAGC,1);
                }
                break;
            case R.id.cbNoiseSuppression:
                enableNS=cbEnableNS.isChecked();
                if(voERunning) {
                    // Enable NS default mode.
                    ViEAndroidAPI.VoE_SetNSStatus(enableNS, 1);
                }
                break;
            case R.id.cbAECM:
                enableAECM = cbEnableAECM.isChecked();
                if (voERunning) {
                    // EC_AECM=5
                    // AECM_DEFAULT=0
                    ViEAndroidAPI.VoE_SetECStatus(enableAECM, 5, 0, 28);
                }
                break;
        }
    }

    private void ReadSettings() {
        codecType = spCodecType.getSelectedItemPosition();
        voiceCodecType = spVoiceCodecType.getSelectedItemPosition();

        String sCodecSize = spCodecSize.getSelectedItem().toString();
        String[] aCodecSize = sCodecSize.split("x");
        codecSizeWidth = Integer.parseInt(aCodecSize[0]);
        codecSizeHeight = Integer.parseInt(aCodecSize[1]);

        loopbackMode  = cbLoopback.isChecked();
        enableVoice  = cbVoice.isChecked();
        enableVideoSend = cbVideoSend.isChecked();
        enableVideoReceive = cbVideoReceive.isChecked();
        enableVideo = enableVideoSend || enableVideoReceive;

        destinationPortVideo =
                Integer.parseInt(etVTxPort.getText().toString());
        receivePortVideo =
                Integer.parseInt(etVRxPort.getText().toString());
        destinationPortVoice =
                Integer.parseInt(etATxPort.getText().toString());
        receivePortVoice =
                Integer.parseInt(etARxPort.getText().toString());

        enableNack  = cbEnableNack.isChecked();
        enableSpeaker  = cbEnableSpeaker.isChecked();
        enableAGC  = cbEnableAGC.isChecked();
        enableAECM  = cbEnableAECM.isChecked();
        enableNS  = cbEnableNS.isChecked();

        if (loopbackMode)
            remoteIp = "127.0.0.1";
        else
            remoteIp = etRemoteIp.getText().toString();
    }

    public void onItemSelected(AdapterView<?> adapterView, View view,
            int position, long id) {
        if ((adapterView == spCodecType || adapterView == spCodecSize) &&
                viERunning) {
            ReadSettings();
            // change the codectype
            if (enableVideoReceive) {
                if (0 != ViEAndroidAPI.SetReceiveCodec(channel, codecType,
                                INIT_BITRATE, codecSizeWidth,
                                codecSizeHeight,
                                RECEIVE_CODEC_FRAMERATE))
                    Log.d(TAG, "ViE set receive codec failed");
            }
            if (enableVideoSend) {
                if (0 != ViEAndroidAPI.SetSendCodec(channel, codecType,
                                INIT_BITRATE, codecSizeWidth, codecSizeHeight,
                                SEND_CODEC_FRAMERATE))
                    Log.d(TAG, "ViE set send codec failed");
            }
        }
        else if ((adapterView == spVoiceCodecType) && voERunning) {
            // change voice engine codec
            ReadSettings();
            if (0 != ViEAndroidAPI.VoE_SetSendCodec(voiceChannel, voiceCodecType)) {
                Log.d(TAG, "VoE set send codec failed");
            }
        }
    }

    public void onNothingSelected(AdapterView<?> arg0) {
        Log.d(TAG, "No setting selected");
    }

    public int UpdateStats(int in_frameRateI, int in_bitRateI, int in_packetLoss,
            int in_frameRateO, int in_bitRateO) {
        frameRateI = in_frameRateI;
        bitRateI = in_bitRateI;
        packetLoss = in_packetLoss;
        frameRateO = in_frameRateO;
        bitRateO = in_bitRateO;
        return 0;
    }

    private void AddStatsView() {
        if (statsView != null) {
            return;
        }
        statsView = new StatsView(this);
        WindowManager.LayoutParams params = new WindowManager.LayoutParams(
            WindowManager.LayoutParams.MATCH_PARENT,
            WindowManager.LayoutParams.WRAP_CONTENT,
            WindowManager.LayoutParams.TYPE_SYSTEM_OVERLAY,
            WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE |
            WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE,
            PixelFormat.TRANSLUCENT);
        params.gravity = Gravity.RIGHT | Gravity.TOP;
        params.setTitle("Load Average");
        mTabHost.addView(statsView, params);
        statsView.setBackgroundColor(0);
    }

    private void RemoveSatsView() {
        mTabHost.removeView(statsView);
        statsView = null;
    }
}
