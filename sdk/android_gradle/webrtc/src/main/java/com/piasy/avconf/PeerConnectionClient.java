package com.piasy.avconf;

import android.content.Context;
import android.text.TextUtils;
import com.piasy.avconf.utils.AndroidSafeScheduledThreadPoolExecutor;
import com.piasy.avconf.utils.AudioDeviceModuleError;
import com.piasy.avconf.utils.Consumer;
import com.piasy.avconf.utils.DefaultPeerConnectionObserver;
import com.piasy.avconf.utils.DefaultSdpObserver;

import java.util.*;
import java.util.concurrent.ScheduledExecutorService;

import org.webrtc.AudioSource;
import org.webrtc.AudioTrack;
import org.webrtc.DataChannel;
import org.webrtc.DefaultVideoDecoderFactory;
import org.webrtc.DefaultVideoEncoderFactory;
import org.webrtc.EglBase;
import org.webrtc.IceCandidate;
import org.webrtc.Loggable;
import org.webrtc.Logging;
import org.webrtc.MediaConstraints;
import org.webrtc.MediaStream;
import org.webrtc.MediaStreamTrack;
import org.webrtc.PeerConnection;
import org.webrtc.PeerConnectionFactory;
import org.webrtc.RtpParameters;
import org.webrtc.RtpReceiver;
import org.webrtc.RtpSender;
import org.webrtc.RtpTransceiver;
import org.webrtc.SdpObserver;
import org.webrtc.SessionDescription;
import org.webrtc.SurfaceTextureHelper;
import org.webrtc.VideoCapturer;
import org.webrtc.VideoDecoderFactory;
import org.webrtc.VideoEncoderFactory;
import org.webrtc.VideoSink;
import org.webrtc.VideoSource;
import org.webrtc.VideoTrack;
import org.webrtc.audio.AudioDeviceModule;
import org.webrtc.audio.JavaAudioDeviceModule;

import static com.piasy.avconf.PeerConnectionClientCallback.ERR_CREATE_PC_FAIL;
import static com.piasy.avconf.PeerConnectionClientCallback.ERR_CREATE_SDP_FAIL;
import static com.piasy.avconf.PeerConnectionClientCallback.ERR_ICE_FAIL;
import static com.piasy.avconf.PeerConnectionClientCallback.ERR_NO_FACTORY;
import static com.piasy.avconf.PeerConnectionClientCallback.ERR_NO_SENDING_TRACK;
import static com.piasy.avconf.PeerConnectionClientCallback.ERR_SET_SDP_FAIL;
import static org.webrtc.MediaStreamTrack.AUDIO_TRACK_KIND;
import static org.webrtc.MediaStreamTrack.VIDEO_TRACK_KIND;

import javax.annotation.Nullable;

/**
 * Created by Piasy{github.com/Piasy} on 2018/11/8.
 */
public class PeerConnectionClient implements PeerConnection.Observer, SdpObserver {

    public static final int DIR_SEND_RECV = 0;
    public static final int DIR_SEND_ONLY = 1;
    public static final int DIR_RECV_ONLY = 2;
    public static final int DIR_INACTIVE = 3;

    public static final String K_AUDIO_TRACK_ID = "CFAMSa0";
    public static final String K_VIDEO_TRACK_ID = "CFAMSv0";

    private static final String TAG = "PeerConnectionClient";
    private static final String VIDEO_TRACK_TYPE = "video";

    private static PeerConnectionFactory sPeerConnectionFactory;
    private static AudioDeviceModule sAdm;
    private static SurfaceTextureHelper sSurfaceTextureHelper;

    private static AudioSource sLocalAudioSource;
    private static AudioTrack sLocalAudioTrack;
    private static VideoSource sLocalVideoSource;
    private static HijackCapturerObserver sHijackCaptureObserver;
    private static VideoTrack sLocalVideoTrack;

    private final String mUid;
    private final int mDir;
    private final boolean mHasVideo;
    private int mVideoMaxBitrateKbps;
    private int mVideoMaxFrameRate;

    private final ScheduledExecutorService mExecutor;
    private final PeerConnectionClientCallback mCallback;

    private PeerConnection mPeerConnection;
    private boolean mErrorHappened;
    private boolean mIsInitiator = false;
    private boolean mIsSettingLocalSdp = false;
    private List<org.webrtc.IceCandidate> queuedRemoteCandidates;

    private final Map<String, AudioTrack> mRemoteAudioTracks = new HashMap<>(1);
    private final Map<String, VideoTrack> mRemoteVideoTracks = new HashMap<>(1);
    private final Map<String, List<VideoSink>> mRemoteTrackRenderers = new HashMap<>(1);
    private final String mNullTidKey = String.valueOf(mRemoteTrackRenderers.hashCode());

    public PeerConnectionClient(
            final String uid, final int dir, final boolean hasVideo,
            final PeerConnectionClientCallback callback,
            final int videoMaxBitrateKbps, final int videoMaxFrameRate
    ) {
        mUid = uid;
        mDir = dir;
        mHasVideo = hasVideo;
        mCallback = callback;
        mVideoMaxBitrateKbps = videoMaxBitrateKbps;
        mVideoMaxFrameRate = videoMaxFrameRate;

        mExecutor = new AndroidSafeScheduledThreadPoolExecutor(1);
    }

    public static synchronized int initialize(final Context appContext, final String fieldTrials,
                                              final Loggable loggable, final Logging.Severity severity) {
        PeerConnectionFactory.initialize(
                PeerConnectionFactory.InitializationOptions.builder(appContext)
                        .setFieldTrials(fieldTrials)
                        .setEnableInternalTracer(true)
                        .setInjectableLogger(loggable, severity)
                        .createInitializationOptions());

        Logging.d(TAG, "initialize success");

        return 0;
    }

    public static boolean send(final int dir) {
        return dir == DIR_SEND_ONLY || dir == DIR_SEND_RECV;
    }

    public static boolean receive(final int dir) {
        return dir == DIR_RECV_ONLY || dir == DIR_SEND_RECV;
    }

    public static synchronized int createPeerConnectionFactory(
            final Context appContext, final EglBase rootEglBase, final PeerConnectionFactory.Options options,
            @Nullable final JavaAudioDeviceModule.SamplesReadyCallback recordSamplesReadyCallback,
            @Nullable final JavaAudioDeviceModule.SamplesReadyCallback trackSamplesReadyCallback,
            final boolean enableH264HighProfile) {
        Logging.d(TAG, "createPeerConnectionFactory, ver " + org.webrtc.BuildConfig.VERSION_NAME
                + ", options " + options + ", enableH264HighProfile " + enableH264HighProfile);

        if (sPeerConnectionFactory != null) {
            Logging.e(TAG, "createPeerConnectionFactory error: already created");
            return -1;
        }

        sAdm = createJavaAudioDevice(appContext, recordSamplesReadyCallback,
            trackSamplesReadyCallback);

        final VideoEncoderFactory encoderFactory = new DefaultVideoEncoderFactory(
                null /* we won't use CVO, so the frame will always be I420, we can't use texture */,
                true, enableH264HighProfile);
        final VideoDecoderFactory decoderFactory = new DefaultVideoDecoderFactory(
                rootEglBase.getEglBaseContext());

        sPeerConnectionFactory = PeerConnectionFactory.builder()
                .setOptions(options)
                .setAudioDeviceModule(sAdm)
                .setVideoEncoderFactory(encoderFactory)
                .setVideoDecoderFactory(decoderFactory)
                .createPeerConnectionFactory();

        sAdm.release();

        Logging.d(TAG, "PeerConnectionFactory created");

        return 0;
    }

    public static synchronized void getOfferForRtpCapabilities(final Consumer<String> consumer) {
        if (sPeerConnectionFactory == null) {
            Logging.e(TAG, "getOfferForRtpCapabilities error: no factory");
            consumer.accept("");
            return;
        }
        PeerConnection pc = sPeerConnectionFactory.createPeerConnection(
                Collections.emptyList(), new DefaultPeerConnectionObserver());
        if (pc != null) {
            pc.addTransceiver(MediaStreamTrack.MediaType.MEDIA_TYPE_AUDIO);
            pc.addTransceiver(MediaStreamTrack.MediaType.MEDIA_TYPE_VIDEO);
            pc.createOffer(new DefaultSdpObserver() {
                @Override
                public void onCreateSuccess(SessionDescription sdp) {
                    consumer.accept(sdp.description);
                }

                @Override
                public void onCreateFailure(String error) {
                    consumer.accept("");
                }
            }, new MediaConstraints());
        } else {
            Logging.e(TAG, "getOfferForRtpCapabilities error: fail to create pc");
            consumer.accept("");
        }
    }

    public static synchronized int createLocalTracks(final Context appContext, final EglBase rootEglBase,
                                                     final VideoCapturer capturer) {
        Logging.d(TAG, "createLocalTracks");

        if (sPeerConnectionFactory == null) {
            Logging.e(TAG, "createLocalTracks error: no factory");
            return -1;
        }

        if (capturer != null) {
            sSurfaceTextureHelper = SurfaceTextureHelper.create("CaptureThread",
                    rootEglBase.getEglBaseContext());
            sLocalVideoSource = sPeerConnectionFactory.createVideoSource(capturer.isScreencast());
            sHijackCaptureObserver =
                    new HijackCapturerObserver(sLocalVideoSource.getCapturerObserver());

            capturer.initialize(sSurfaceTextureHelper, appContext, sHijackCaptureObserver);

            sLocalVideoTrack = sPeerConnectionFactory.createVideoTrack(K_VIDEO_TRACK_ID,
                    sLocalVideoSource);
        }

        sLocalAudioSource = sPeerConnectionFactory.createAudioSource(new MediaConstraints());
        sLocalAudioTrack = sPeerConnectionFactory.createAudioTrack(K_AUDIO_TRACK_ID,
                sLocalAudioSource);

        Logging.d(TAG, "createLocalTracks success");
        return 0;
    }

    public static synchronized void adaptVideoOutputFormat(final int width, final int height, final int fps) {
        if (sLocalVideoSource != null) {
            sLocalVideoSource.adaptOutputFormat(width, height, fps);
        }
    }

    public static HijackCapturerObserver getsHijackCaptureObserver() {
        return sHijackCaptureObserver;
    }

    public static synchronized void addLocalTrackRenderer(final VideoSink localTrackRenderer) {
        Logging.d(TAG, "addLocalTrackRenderer " + localTrackRenderer);
        if (sLocalVideoTrack != null) {
            sLocalVideoTrack.addSink(localTrackRenderer);
        }
    }

    public static synchronized void removeLocalTrackRenderer(final VideoSink localTrackRenderer) {
        Logging.d(TAG, "removeLocalTrackRenderer " + localTrackRenderer);
        if (sLocalVideoTrack != null) {
            sLocalVideoTrack.removeSink(localTrackRenderer);
        }
    }

    public static synchronized int destroyPeerConnectionFactory() {
        Logging.d(TAG, "destroyPeerConnectionFactory");
        if (sLocalAudioSource != null) {
            sLocalAudioSource.dispose();
            sLocalAudioSource = null;
        }
        if (sLocalAudioTrack != null) {
            sLocalAudioTrack.dispose();
            sLocalAudioTrack = null;
        }
        if (sLocalVideoSource != null) {
            sLocalVideoSource.dispose();
            sLocalVideoSource = null;
        }
        if (sHijackCaptureObserver != null) {
            sHijackCaptureObserver.dispose();
            sHijackCaptureObserver = null;
        }
        if (sLocalVideoTrack != null) {
            sLocalVideoTrack.dispose();
            sLocalVideoTrack = null;
        }
        if (sSurfaceTextureHelper != null) {
            sSurfaceTextureHelper.dispose();
            sSurfaceTextureHelper = null;
        }
        if (sPeerConnectionFactory != null) {
            sPeerConnectionFactory.dispose();
            sPeerConnectionFactory = null;
        }
        sAdm = null;

        Logging.d(TAG, "destroyPeerConnectionFactory success");
        return 0;
    }

    public static synchronized void toggleAudioRecordPause(final boolean pause) {
        Logging.d(TAG, "toggleAudioRecordPause(" + pause + ")");
        if (sAdm != null) {
            sAdm.toggleRecordPause(pause);
        }
    }

    private static AudioDeviceModule createJavaAudioDevice(final Context appContext,
        final JavaAudioDeviceModule.SamplesReadyCallback recordSamplesReadyCallback,
        final JavaAudioDeviceModule.SamplesReadyCallback trackSamplesReadyCallback) {
        JavaAudioDeviceModule.AudioRecordErrorCallback
                audioRecordErrorCallback = new JavaAudioDeviceModule.AudioRecordErrorCallback() {
            @Override
            public void onWebRtcAudioRecordInitError(String errorMessage) {
                Logging.e(TAG, "onWebRtcAudioRecordInitError: " + errorMessage);
                AudioDeviceModuleError.onAudioRecordError(AudioDeviceModuleError.ERR_INIT);
            }

            @Override
            public void onWebRtcAudioRecordStartError(
                    JavaAudioDeviceModule.AudioRecordStartErrorCode errorCode,
                    String errorMessage) {
                Logging.e(TAG, "onWebRtcAudioRecordStartError: " + errorCode + ". " + errorMessage);
                AudioDeviceModuleError.onAudioRecordError(AudioDeviceModuleError.ERR_START);
            }

            @Override
            public void onWebRtcAudioRecordError(String errorMessage) {
                Logging.e(TAG, "onWebRtcAudioRecordError: " + errorMessage);
                AudioDeviceModuleError.onAudioRecordError(AudioDeviceModuleError.ERR_RUN);
            }
        };

        JavaAudioDeviceModule.AudioTrackErrorCallback
                audioTrackErrorCallback = new JavaAudioDeviceModule.AudioTrackErrorCallback() {
            @Override
            public void onWebRtcAudioTrackInitError(String errorMessage) {
                Logging.e(TAG, "onWebRtcAudioTrackInitError: " + errorMessage);
                AudioDeviceModuleError.onAudioTrackError(AudioDeviceModuleError.ERR_INIT);
            }

            @Override
            public void onWebRtcAudioTrackStartError(
                    JavaAudioDeviceModule.AudioTrackStartErrorCode errorCode, String errorMessage) {
                Logging.e(TAG, "onWebRtcAudioTrackStartError: " + errorCode + ". " + errorMessage);
                AudioDeviceModuleError.onAudioTrackError(AudioDeviceModuleError.ERR_START);
            }

            @Override
            public void onWebRtcAudioTrackError(String errorMessage) {
                Logging.e(TAG, "onWebRtcAudioTrackError: " + errorMessage);
                AudioDeviceModuleError.onAudioTrackError(AudioDeviceModuleError.ERR_RUN);
            }
        };

        return JavaAudioDeviceModule.builder(appContext)
                .setSamplesReadyCallback(recordSamplesReadyCallback)
                .setTrackSamplesReadyCallback(trackSamplesReadyCallback)
                .setUseHardwareAcousticEchoCanceler(true)
                .setUseHardwareNoiseSuppressor(true)
                .setAudioRecordErrorCallback(audioRecordErrorCallback)
                .setAudioTrackErrorCallback(audioTrackErrorCallback)
                .createAudioDeviceModule();
    }

    public void createPeerConnection(final List<PeerConnection.IceServer> iceServers) {
        logInfo("createPeerConnection " + iceServers);
        PeerConnectionFactory factory = sPeerConnectionFactory;
        AudioTrack localAudioTrack = sLocalAudioTrack;
        VideoTrack localVideoTrack = sLocalVideoTrack;
        if (factory == null) {
            logError("createPeerConnection error: no factory");
            mCallback.onError(mUid, ERR_NO_FACTORY);
            return;
        }
        if (send() && (localAudioTrack == null || (mHasVideo && localVideoTrack == null))) {
            logError("createPeerConnection error: no sending track");
            mCallback.onError(mUid, ERR_NO_SENDING_TRACK);
            return;
        }

        mExecutor.execute(() -> {
            queuedRemoteCandidates = new ArrayList<>();

            PeerConnection.RTCConfiguration rtcConfig =
                    new PeerConnection.RTCConfiguration(iceServers);
            // TCP candidates are only useful when connecting to a server that supports ICE-TCP.
            rtcConfig.tcpCandidatePolicy = PeerConnection.TcpCandidatePolicy.DISABLED;
            rtcConfig.bundlePolicy = PeerConnection.BundlePolicy.MAXBUNDLE;
            rtcConfig.rtcpMuxPolicy = PeerConnection.RtcpMuxPolicy.REQUIRE;
            rtcConfig.continualGatheringPolicy
                    = PeerConnection.ContinualGatheringPolicy.GATHER_CONTINUALLY;
            // Use ECDSA encryption.
            rtcConfig.keyType = PeerConnection.KeyType.ECDSA;
            rtcConfig.sdpSemantics = PeerConnection.SdpSemantics.UNIFIED_PLAN;

            mPeerConnection = factory.createPeerConnection(rtcConfig, this);
            if (mPeerConnection == null) {
                logError("createPeerConnection error: create pc fail");
                mCallback.onError(mUid, ERR_CREATE_PC_FAIL);
                return;
            }

            if (send()) {
                RtpTransceiver.RtpTransceiverInit transceiverInit
                        = new RtpTransceiver.RtpTransceiverInit(
                        receive() ? RtpTransceiver.RtpTransceiverDirection.SEND_RECV
                                : RtpTransceiver.RtpTransceiverDirection.SEND_ONLY,
                        Collections.singletonList(mUid));
                mPeerConnection.addTransceiver(localAudioTrack, transceiverInit);
                if (localVideoTrack != null) {
                    mPeerConnection.addTransceiver(localVideoTrack, transceiverInit);
                }
            } else if (receive()) {
                RtpTransceiver.RtpTransceiverInit transceiverInit
                        = new RtpTransceiver.RtpTransceiverInit(
                        RtpTransceiver.RtpTransceiverDirection.RECV_ONLY,
                        Collections.singletonList(mUid));
                mPeerConnection.addTransceiver(MediaStreamTrack.MediaType.MEDIA_TYPE_AUDIO,
                        transceiverInit);
                if (mHasVideo) {
                    mPeerConnection.addTransceiver(MediaStreamTrack.MediaType.MEDIA_TYPE_VIDEO,
                            transceiverInit);
                }
            }

            logInfo("createPeerConnection success");
        });
    }

    public void getStats() {
        mPeerConnection.getStats(report -> mCallback.onPeerConnectionStatsReady(mUid, report));
    }

    public void setAudioSendingEnabled(final boolean enable) {
        logInfo("setAudioSendingEnabled " + enable);
        mExecutor.execute(() -> {
            AudioTrack localAudioTrack = sLocalAudioTrack;
            if (localAudioTrack != null) {
                localAudioTrack.setEnabled(enable);
                logInfo("setAudioSendingEnabled " + enable + " success");
            }
        });
    }

    public void setVideoSendingEnabled(final boolean enable) {
        logInfo("setVideoSendingEnabled " + enable);
        mExecutor.execute(() -> {
            VideoTrack localVideoTrack = sLocalVideoTrack;
            HijackCapturerObserver hijackCapturerObserver = sHijackCaptureObserver;
            if (localVideoTrack != null && hijackCapturerObserver != null) {
                localVideoTrack.setEnabled(enable);
                hijackCapturerObserver.toggleMute(!enable);
                logInfo("setVideoSendingEnabled " + enable + " success");
            }
        });
    }

    /**
     * Null trackId means for all tracks.
     */
    public void setAudioReceivingEnabled(@Nullable final String trackId, final boolean enable) {
        logInfo("setAudioReceivingEnabled " + trackId + " " + enable);
        mExecutor.execute(() -> {
            if (trackId == null) {
                for (AudioTrack track : mRemoteAudioTracks.values()) {
                    track.setEnabled(enable);
                }
                logInfo("setAudioReceivingEnabled " + enable + " success");
            } else {
                AudioTrack track = mRemoteAudioTracks.get(trackId);
                if (track != null) {
                    track.setEnabled(enable);
                    logInfo("setAudioReceivingEnabled " + trackId + " " + enable + " success");
                } else {
                    logError("setAudioReceivingEnabled " + trackId + " " + enable + " no track");
                }
            }
        });
    }

    /**
     * Null trackId means for all tracks.
     */
    public void setVideoReceivingEnabled(@Nullable final String trackId, final boolean enable) {
        logInfo("setVideoReceivingEnabled " + trackId + " " + enable);
        mExecutor.execute(() -> {
            if (trackId == null) {
                for (VideoTrack track : mRemoteVideoTracks.values()) {
                    track.setEnabled(enable);
                }
                logInfo("setVideoReceivingEnabled " + enable + " success");
            } else {
                VideoTrack track = mRemoteVideoTracks.get(trackId);
                if (track != null) {
                    track.setEnabled(enable);
                    logInfo("setVideoReceivingEnabled " + trackId + " " + enable + " success");
                } else {
                    logError("setVideoReceivingEnabled " + trackId + " " + enable + " no track");
                }
            }
        });
    }

    public void createOffer() {
        logInfo("createOffer");
        mExecutor.execute(() -> {
            if (mPeerConnection != null && !mErrorHappened) {
                mIsInitiator = true;
                mPeerConnection.createOffer(this, defaultSdpConstraints());
                logInfo("createOffer finish");
            }
        });
    }

    public void createAnswer() {
        logInfo("createAnswer");
        mExecutor.execute(() -> {
            if (mPeerConnection != null && !mErrorHappened) {
                mIsInitiator = false;
                mPeerConnection.createAnswer(this, defaultSdpConstraints());
                logInfo("createAnswer finish");
            }
        });
    }

    private MediaConstraints defaultSdpConstraints() {
        return new MediaConstraints();
    }

    public void addIceCandidate(final IceCandidate candidate) {
        logInfo("addIceCandidate " + candidate);
        mExecutor.execute(() -> {
            if (mPeerConnection == null || mErrorHappened) {
                logError("addIceCandidate error: no pc");
                return;
            }
            if (queuedRemoteCandidates != null) {
                queuedRemoteCandidates.add(candidate);
            } else {
                mPeerConnection.addIceCandidate(candidate);
            }
            logInfo("addIceCandidate success");
        });
    }

    public void removeIceCandidates(final List<IceCandidate> candidates) {
        logInfo("removeIceCandidates " + candidates);
        mExecutor.execute(() -> {
            if (mPeerConnection == null || mErrorHappened) {
                logError("removeIceCandidates error: no pc");
                return;
            }
            drainCandidates();
            IceCandidate[] array = new IceCandidate[candidates.size()];
            candidates.toArray(array);
            mPeerConnection.removeIceCandidates(array);
            logInfo("removeIceCandidates success");
        });
    }

    public void setRemoteDescription(final SessionDescription sdp) {
        logInfo("setRemoteDescription\n" + sdp.description);
        mExecutor.execute(() -> {
            if (mPeerConnection == null || mErrorHappened) {
                logError("setRemoteDescription error: no pc");
                return;
            }
            // already refined by AvConf
            mPeerConnection.setRemoteDescription(this, sdp);
            logInfo("setRemoteDescription finish");
        });
    }

    public boolean send() {
        return send(mDir);
    }

    public boolean receive() {
        return receive(mDir);
    }

    public int startRecorder(final int dir, final String path) {
        PeerConnection peerConnection = mPeerConnection;
        if (peerConnection != null) {
            return peerConnection.startRecorder(dir, path);
        }
        return -100;
    }

    public int stopRecorder(final int dir) {
        PeerConnection peerConnection = mPeerConnection;
        if (peerConnection != null) {
            return peerConnection.stopRecorder(dir);
        }
        return -100;
    }

    public void requestFir() {
        PeerConnection peerConnection = mPeerConnection;
        if (peerConnection != null) {
            peerConnection.sendVideoKeyFrame();
        }
    }

    public void close() {
        logInfo("close");
        mExecutor.execute(() -> {
            if (mPeerConnection != null) {
                mPeerConnection.dispose();
                mPeerConnection = null;
            }
            mExecutor.shutdownNow();
            logInfo("close success");
        });
    }

    /**
     * Null trackId means for all tracks.
     */
    public void addRemoteTrackRenderer(@Nullable final String trackId, final VideoSink remoteTrackRenderer) {
        logInfo("addRemoteTrackRenderer " + trackId + " " + remoteTrackRenderer);
        mExecutor.execute(() -> {
            if (trackId == null) {
                if (mRemoteVideoTracks.isEmpty()) {
                    addRendererForLaterUsage(mNullTidKey, remoteTrackRenderer);
                } else {
                    for (VideoTrack track : mRemoteVideoTracks.values()) {
                        track.addSink(remoteTrackRenderer);
                    }
                }
            } else {
                VideoTrack track = mRemoteVideoTracks.get(trackId);
                if (track != null) {
                    track.addSink(remoteTrackRenderer);
                } else {
                    addRendererForLaterUsage(trackId, remoteTrackRenderer);
                }
            }
        });
    }

    private void addRendererForLaterUsage(final String trackId, final VideoSink renderer) {
        if (mRemoteTrackRenderers.containsKey(trackId)) {
            mRemoteTrackRenderers.get(trackId).add(renderer);
        } else {
            List<VideoSink> renderers = new ArrayList<>();
            renderers.add(renderer);
            mRemoteTrackRenderers.put(trackId, renderers);
        }
    }

    private void removeLaterRenderer(final String trackId, final VideoSink renderer) {
        if (mRemoteTrackRenderers.containsKey(trackId)) {
            mRemoteTrackRenderers.get(trackId).remove(renderer);
        }
    }

    /**
     * Null trackId means for all tracks.
     */
    public void removeRemoteTrackRenderer(@Nullable final String trackId, final VideoSink remoteTrackRenderer) {
        logInfo("removeRemoteTrackRenderer " + trackId + " " + remoteTrackRenderer);
        mExecutor.execute(() -> {
            if (trackId == null) {
                if (mRemoteVideoTracks.isEmpty()) {
                    removeLaterRenderer(mNullTidKey, remoteTrackRenderer);
                } else {
                    for (VideoTrack track : mRemoteVideoTracks.values()) {
                        track.removeSink(remoteTrackRenderer);
                    }
                }
            } else {
                VideoTrack track = mRemoteVideoTracks.get(trackId);
                if (track != null) {
                    track.removeSink(remoteTrackRenderer);
                } else {
                    removeLaterRenderer(trackId, remoteTrackRenderer);
                }
            }
        });
    }

    private void drainCandidates() {
        if (queuedRemoteCandidates != null) {
            logInfo("Add " + queuedRemoteCandidates.size() + " remote candidates");
            for (org.webrtc.IceCandidate candidate : queuedRemoteCandidates) {
                mPeerConnection.addIceCandidate(candidate);
            }
            queuedRemoteCandidates = null;
        }
    }

    private void reportError(final int code) {
        mCallback.onError(mUid, code);
        mErrorHappened = true;
    }

    @Override
    public void onSignalingChange(final PeerConnection.SignalingState newState) {
        logInfo("SignalingState: " + newState);
    }

    @Override
    public void onIceConnectionChange(final PeerConnection.IceConnectionState newState) {
        logInfo("onIceConnectionChange: " + newState);
        switch (newState) {
            case CONNECTED:
                mCallback.onIceConnected(mUid);
                break;
            case DISCONNECTED:
                mCallback.onIceDisconnected(mUid);
                break;
            case FAILED:
                reportError(ERR_ICE_FAIL);
                break;
            default:
                break;
        }
    }

    @Override
    public void onIceConnectionReceivingChange(final boolean receiving) {
        logInfo("onIceConnectionReceivingChange: "
                + (receiving ? "" : "not") + " receiving");
    }

    @Override
    public void onIceGatheringChange(final PeerConnection.IceGatheringState newState) {
        logInfo("onIceGatheringChange: " + newState);
    }

    @Override
    public void onIceCandidate(final org.webrtc.IceCandidate candidate) {
        logInfo("onIceCandidate " + candidate);
        mCallback.onIceCandidate(mUid, candidate);
    }

    @Override
    public void onIceCandidatesRemoved(final org.webrtc.IceCandidate[] candidates) {
        logInfo("onIceCandidatesRemoved " + Arrays.toString(candidates));
        mCallback.onIceCandidatesRemoved(mUid, Arrays.asList(candidates));
    }

    @Override
    public void onAddStream(final MediaStream stream) {
        logInfo("onAddStream: " + stream);
    }

    @Override
    public void onRemoveStream(final MediaStream stream) {
        logInfo("onRemoveStream: " + stream);
    }

    @Override
    public void onDataChannel(final DataChannel dataChannel) {
        logInfo("onDataChannel: " + dataChannel);
    }

    @Override
    public void onRenegotiationNeeded() {
        logInfo("onRenegotiationNeeded");
    }

    @Override
    public void onAddTrack(final RtpReceiver receiver, final MediaStream[] mediaStreams) {
        logInfo("onAddTrack " + receiver + ", " + receiver.id() + ", " + receiver.track()
                + ", " + receiver.track().id() + ", " + Arrays.toString(mediaStreams));
        mExecutor.execute(() -> {
            String trackId = receiver.track().id();
            String kind = receiver.track().kind();
            if (TextUtils.equals(kind, AUDIO_TRACK_KIND)) {
                mRemoteAudioTracks.put(trackId, (AudioTrack) receiver.track());
            } else if (TextUtils.equals(kind, VIDEO_TRACK_KIND)) {
                VideoTrack track = (VideoTrack) receiver.track();
                mRemoteVideoTracks.put(trackId, track);

                if (mRemoteTrackRenderers.containsKey(mNullTidKey)) {
                    for (VideoSink renderer : mRemoteTrackRenderers.get(mNullTidKey)) {
                        track.addSink(renderer);
                    }
                    mRemoteTrackRenderers.clear();
                } else {
                    List<VideoSink> renderers = mRemoteTrackRenderers.remove(trackId);
                    if (renderers != null && !renderers.isEmpty()) {
                        for (VideoSink renderer : renderers) {
                            track.addSink(renderer);
                        }
                    }
                }
            }
        });
    }

    @Override
    public void onRemoveTrack(RtpReceiver receiver) {
        logInfo("onRemoveTrack " + receiver + ", " + receiver.id() + ", " + receiver.track()
                + ", " + receiver.track().id());
        mExecutor.execute(() -> {
            String trackId = receiver.track().id();
            String kind = receiver.track().kind();
            if (TextUtils.equals(kind, AUDIO_TRACK_KIND)) {
                mRemoteAudioTracks.remove(trackId);
            } else if (TextUtils.equals(kind, VIDEO_TRACK_KIND)) {
                mRemoteVideoTracks.remove(trackId);
            }
        });
    }

    @Override
    public void onCreateSuccess(final SessionDescription sdp) {
        logInfo("onCreateSuccess\n" + sdp.description);
        SessionDescription localSdp = new SessionDescription(sdp.type,
                mCallback.onPreferCodecs(mUid, sdp.description));
        mExecutor.execute(() -> {
            if (mPeerConnection != null && !mErrorHappened) {
                logInfo("refined sdp\n" + localSdp.description);
                mIsSettingLocalSdp = true;
                mPeerConnection.setLocalDescription(this, localSdp);
            }
        });
    }

    @Override
    public void onSetSuccess() {
        logInfo("onSetSuccess");
        mExecutor.execute(() -> {
            if (mPeerConnection == null || mErrorHappened) {
                return;
            }
            if (mIsSettingLocalSdp) {
                mIsSettingLocalSdp = false;
                logInfo("Local SDP set successfully");
                mCallback.onLocalDescription(mUid, mPeerConnection.getLocalDescription());
                doSetVideoMaxBitrate();
            } else {
                logInfo("Remote SDP set successfully");
                mCallback.onSetRemoteSdpResult(mUid, true);
            }

            // For offering peer connection we first create offer and set
            // local SDP, then after receiving answer set remote SDP.
            // After setting answer (remote description), we need to drain remote candidates.
            // For answering peer connection we set remote SDP and then
            // create answer and set local SDP.
            // After setting answer (local description), we need to drain remote candidates.
            if ((mIsInitiator && !mIsSettingLocalSdp) || (!mIsInitiator && mIsSettingLocalSdp)) {
                drainCandidates();
            }
        });
    }

    @Override
    public void onCreateFailure(final String error) {
        logError("onCreateFailure " + error);
        reportError(ERR_CREATE_SDP_FAIL);
    }

    @Override
    public void onSetFailure(final String error) {
        logError("onSetFailure " + error);
        if (!mIsSettingLocalSdp) {
            mCallback.onSetRemoteSdpResult(mUid, false);
        }
        mIsSettingLocalSdp = false;
        reportError(ERR_SET_SDP_FAIL);
    }

    public void setVideoMaxBitrateKbps(final int videoMaxBitrateKbps) {
        logInfo("setVideoMaxBitrateKbps " + videoMaxBitrateKbps);
        mExecutor.execute(() -> {
            mVideoMaxBitrateKbps = videoMaxBitrateKbps;
            doSetVideoMaxBitrate();
        });
    }

    private void doSetVideoMaxBitrate() {
        for (RtpSender sender : mPeerConnection.getSenders()) {
            if (sender.track() != null) {
                String trackType = sender.track().kind();
                if (trackType.equals(VIDEO_TRACK_TYPE)) {
                    logInfo("Found video sender");

                    RtpParameters parameters = sender.getParameters();
                    if (parameters.encodings.isEmpty()) {
                        Logging.w(TAG, "RtpParameters are not ready");
                        return;
                    }

                    for (RtpParameters.Encoding encoding : parameters.encodings) {
                        encoding.maxBitrateBps = mVideoMaxBitrateKbps * 1000;
                        encoding.maxFramerate = mVideoMaxFrameRate;
                    }
                    if (!sender.setParameters(parameters)) {
                        logError("RtpSender.setParameters failed");
                    }
                    logInfo("Configured max video bitrate to: " + mVideoMaxBitrateKbps);
                }
            }
        }
    }

    private void logInfo(final String content) {
        Logging.d(TAG, "@" + hashCode() + " " + content);
    }

    private void logError(final String content) {
        Logging.e(TAG, "@" + hashCode() + " " + content);
    }
}
