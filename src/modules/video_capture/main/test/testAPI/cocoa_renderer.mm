// 
//  CocoaRenderer.mm
//  testCocoaCommandLine
#import <Cocoa/Cocoa.h>
#import <AppKit/AppKit.h>
#import "cocoa_renderer.h"

@implementation CocoaRenderer
@synthesize _nsOpenGLContext;
@synthesize screen = _screen;
- (void)initCocoaRenderer:(NSOpenGLPixelFormat*)fmt{
	self = [super initWithFrame:[self frame] pixelFormat:[fmt autorelease]];
	if (self != nil)
	{
		_nsOpenGLContext = [self openGLContext];
	}
}
@end

