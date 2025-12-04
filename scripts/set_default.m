#import <Cocoa/Cocoa.h>

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        // Identifier of our app
        NSString *bundleID = @"com.nkriuchkov.linkswitch";
        
        // Get the current default browser
        NSURL *currentBrowser = [[NSWorkspace sharedWorkspace] URLForApplicationToOpenURL:[NSURL URLWithString:@"http://google.com"]];
        if (currentBrowser) {
             NSBundle *bundle = [NSBundle bundleWithURL:currentBrowser];
             NSLog(@"Current default browser: %@", [bundle bundleIdentifier]);
        }

        // Attempt to set LinkSwitch as default for http
        OSStatus statusHttp = LSSetDefaultHandlerForURLScheme(CFSTR("http"), (__bridge CFStringRef)bundleID);
        OSStatus statusHttps = LSSetDefaultHandlerForURLScheme(CFSTR("https"), (__bridge CFStringRef)bundleID);
        OSStatus statusHtml = LSSetDefaultRoleHandlerForContentType(kUTTypeHTML, kLSRolesAll, (__bridge CFStringRef)bundleID);

        if (statusHttp == noErr && statusHttps == noErr) {
            NSLog(@"Successfully requested to set LinkSwitch as default browser.");
            
            // Show a dialog to the user because macOS will prompt for confirmation anyway
            NSAlert *alert = [[NSAlert alloc] init];
            [alert setMessageText:@"Default Browser Request"];
            [alert setInformativeText:@"LinkSwitch has requested to be your default browser. macOS should show a confirmation prompt shortly. Please click 'Use LinkSwitch' when asked."];
            [alert runModal];
        } else {
            NSLog(@"Failed to set default browser. Error codes: %d, %d", (int)statusHttp, (int)statusHttps);
        }
    }
    return 0;
}
