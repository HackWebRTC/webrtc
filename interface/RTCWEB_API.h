#ifndef RTCWEB_H
#define RTCWEB_H


class StateNotifier
{

public:

	// Called when the state of the session changes.
	// INIT->SENT_OFFER->RECEIVED_ANSWER->INPROGRESS->TERMINATED
	virtual void onStateChange(int newState, char * stateInfo)=0;
};



class Session 
{
public:

	static Session * create(char* id, StateNotifier & obj);


	// generates a session description
	virtual int generateLocalDescription(char * desc, int maxLen) = 0;
	
	// configures the local media options
	virtual int setLocalDescription(char * desc, int maxLenDesc, char * type, int maxLenType) = 0;
	
	// configures the remote media options
	virtual int setRemoteDescription(char * desc, int maxLenDesc, char * type, int maxLenType) = 0;
 
	// Starts or stops sending/receiving media.
	virtual int enable(bool enable) = 0;
	
	// Mutes or unmutes the sending of media.
	virtual int mute(char * media, int maxLen, bool mute) = 0;
	
	// Sends a DTMF tone (for use telephony situations)
	virtual int sendDTMF(int event) = 0;

	// Adds an additional stream to the session (for multi-user)
	virtual int addStream(char * media, int maxLen, int source) = 0;
	
	// Removes a stream from the session.
	virtual int removeStream(char * media, int maxLen, int source) = 0;

	// Gets a URL for a given stream that can be used by
	// <video> or another playout destination. The default
	// stream can be obtained by passing “0”.
	virtual int getStreamURL(char * media, int maxLen, int source) = 0;

};



#endif // RTCWEB_H
