// 
//  CocoaRenderView.mm
//

#import <Cocoa/Cocoa.h>
#import <AppKit/AppKit.h>
#import "cocoa_render_view.h"
#include "trace.h"

using namespace webrtc;

@implementation CocoaRenderView


-(void)initCocoaRenderView:(NSOpenGLPixelFormat*)fmt{
	WEBRTC_TRACE(kTraceModuleCall, kTraceVideoRenderer, 0, "%s:%d", __FUNCTION__, __LINE__); 
	
	self = [super initWithFrame:[self frame] pixelFormat:[fmt autorelease]];
	if (self == nil){
		
		WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, 0, "%s:%d Could not create instance", __FUNCTION__, __LINE__); 
	}
	
	
	WEBRTC_TRACE(kTraceModuleCall, kTraceVideoRenderer, 0, "%s:%d Created instance", __FUNCTION__, __LINE__); 
	_nsOpenGLContext = [self openGLContext];

}


-(NSOpenGLContext*)nsOpenGLContext{
    return _nsOpenGLContext;
}



-(void)initCocoaRenderViewFullScreen:(NSOpenGLPixelFormat*)fmt{
	WEBRTC_TRACE(kTraceModuleCall, kTraceVideoRenderer, 0, "%s:%d", __FUNCTION__, __LINE__); 
	
	NSRect screenRect = [[NSScreen mainScreen]frame];
//	[_windowRef setFrame:screenRect];
//	[_windowRef setBounds:screenRect];
	self = [super initWithFrame:screenRect	pixelFormat:[fmt autorelease]];
	if (self == nil){
		
		WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, 0, "%s:%d Could not create instance", __FUNCTION__, __LINE__); 
	}
	
	WEBRTC_TRACE(kTraceModuleCall, kTraceVideoRenderer, 0, "%s:%d Created instance", __FUNCTION__, __LINE__); 
	_nsOpenGLContext = [self openGLContext];

}

@end


