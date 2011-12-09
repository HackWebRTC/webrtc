//
//  CocoaFullScreenWindow.m
//

#import "cocoa_full_screen_window.h"
#include "trace.h"

using namespace webrtc;

@implementation CocoaFullScreenWindow



-(id)init{	
	WEBRTC_TRACE(kTraceModuleCall, kTraceVideoRenderer, 0, "%s:%d", __FUNCTION__, __LINE__); 
	
	self = [super init];
	if(!self){
		WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, 0, "%s:%d COULD NOT CREATE INSTANCE", __FUNCTION__, __LINE__); 
		return nil;
	}
	
	
	WEBRTC_TRACE(kTraceInfo, kTraceVideoRenderer, 0, "%s:%d Created instance", __FUNCTION__, __LINE__); 
	return self;
}

-(void)grabFullScreen{
	WEBRTC_TRACE(kTraceModuleCall, kTraceVideoRenderer, 0, "%s:%d", __FUNCTION__, __LINE__); 
	
#ifdef GRAB_ALL_SCREENS
	if(CGCaptureAllDisplays() != kCGErrorSuccess)
#else
	if(CGDisplayCapture(kCGDirectMainDisplay) != kCGErrorSuccess)
#endif
	{
		WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, 0, "%s:%d Could not capture main level", __FUNCTION__, __LINE__); 
	}
	
	// get the shielding window level
	int windowLevel = CGShieldingWindowLevel();
	
	// get the screen rect of main display
	NSRect screenRect = [[NSScreen mainScreen]frame];
	
	_window = [[NSWindow alloc]initWithContentRect:screenRect 
										   styleMask:NSBorderlessWindowMask
											 backing:NSBackingStoreBuffered
											   defer:NO
											  screen:[NSScreen mainScreen]];
	
	[_window setLevel:windowLevel];
	[_window setBackgroundColor:[NSColor blackColor]];
	[_window makeKeyAndOrderFront:nil];

}
 
-(void)releaseFullScreen
{
	WEBRTC_TRACE(kTraceModuleCall, kTraceVideoRenderer, 0, "%s:%d", __FUNCTION__, __LINE__); 
	[_window orderOut:self];
	
#ifdef GRAB_ALL_SCREENS
	if(CGReleaseAllDisplays() != kCGErrorSuccess)
#else
	if(CGDisplayRelease(kCGDirectMainDisplay) != kCGErrorSuccess)
#endif
	{
		WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, 0, "%s:%d Could not release the displays", __FUNCTION__, __LINE__); 
	}		
}

- (NSWindow*)window
{
  return _window;
}

- (void) dealloc
{
	WEBRTC_TRACE(kTraceModuleCall, kTraceVideoRenderer, 0, "%s:%d", __FUNCTION__, __LINE__); 
	[self releaseFullScreen];
	[super dealloc];
}	


	
@end
