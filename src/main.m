#import <Cocoa/Cocoa.h>
#include "config.h"
#include <sys/stat.h>

@interface AppDelegate : NSObject <NSApplicationDelegate>
@end

static NSString *ConfigPath(void) {
    return [NSHomeDirectory() stringByAppendingPathComponent:@".config/linkswitch/config.yaml"];
}

// NSURL rejects strings with characters that should have been percent-encoded,
// such as spaces. Some apps send links like that, so encode them before giving up.
static NSURL *URLFromString(NSString *string) {
    NSURL *url = [NSURL URLWithString:string];
    if (url) return url;
    NSMutableCharacterSet *allowed = [[NSCharacterSet URLQueryAllowedCharacterSet] mutableCopy];
    [allowed addCharactersInString:@"#%[]"];
    NSString *encoded = [string stringByAddingPercentEncodingWithAllowedCharacters:allowed];
    return encoded ? [NSURL URLWithString:encoded] : nil;
}

@implementation AppDelegate {
    BOOL _isHandlingURL;
}

// Runs the profile's command with the URL as $1 (see build_profile_command).
- (BOOL)launchProfile:(BrowserProfile *)profile withURL:(NSString *)urlString {
    char *script = build_profile_command(profile);
    NSString *command = script ? [NSString stringWithUTF8String:script] : nil;
    free(script);
    if (!command) {
        NSLog(@"Profile \"%s\" has no usable cmd or app", profile->name);
        return NO;
    }

    NSTask *task = [[NSTask alloc] init];
    task.executableURL = [NSURL fileURLWithPath:@"/bin/sh"];
    task.arguments = @[@"-c", command, @"linkswitch", urlString];
    NSError *error = nil;
    if (![task launchAndReturnError:&error]) {
        NSLog(@"Could not run profile \"%s\": %@", profile->name, error);
        return NO;
    }
    return YES;
}

// Opens the URL in the browser or profile called browserName. Returns NO if
// that browser can't be found or the profile can't be run.
- (BOOL)openURL:(NSString *)urlString withBrowser:(NSString *)browserName config:(Config *)config {
    BrowserProfile *profile = config ? find_profile(config, [browserName UTF8String]) : NULL;
    if (profile) {
        return [self launchProfile:profile withURL:urlString];
    }

    NSURL *url = URLFromString(urlString);
    if (!url) {
        NSLog(@"Not a valid URL: %@", urlString);
        return NO;
    }

    // Never fall back to the system's default browser: that is LinkSwitch itself.
    NSWorkspace *workspace = [NSWorkspace sharedWorkspace];
    NSString *appPath = browserName ? [workspace fullPathForApplication:browserName] : nil;
    NSString *bundleID = appPath ? [[NSBundle bundleWithPath:appPath] bundleIdentifier] : nil;
    if (!bundleID) {
        NSLog(@"Could not find browser: %@", browserName);
        return NO;
    }
    return [workspace openURLs:@[url]
                       withAppBundleIdentifier:bundleID
                       options:NSWorkspaceLaunchDefault
                       additionalEventParamDescriptor:nil
                       launchIdentifiers:nil];
}

- (void)browserButtonClicked:(NSButton *)sender {
    [NSApp stopModalWithCode:sender.tag];
}

- (void)showBrowserPickerForURL:(NSString *)urlString withConfig:(Config *)config {
    NSArray<NSString *> *browsers;
    
    if (config && config->browser_count > 0) {
        NSMutableArray *arr = [NSMutableArray array];
        for (int i = 0; i < config->browser_count; i++) {
            [arr addObject:[NSString stringWithUTF8String:config->browsers[i]]];
        }
        browsers = arr;
    } else {
        // Default to only Safari if no browsers configured
        browsers = @[@"Safari"];
    }
    
    CGFloat windowWidth = 260;
    CGFloat padding = 8;
    CGFloat buttonHeight = 26;
    CGFloat textHeight = 18;
    CGFloat buttonGap = 4;
    
    // Calculate height
    // Title + URL + (Browsers + Cancel) * (Height + Gap) + Padding
    CGFloat contentHeight = padding + textHeight + 5 + textHeight + 10 + ((browsers.count + 1) * (buttonHeight + buttonGap));
    
    NSRect frame = NSMakeRect(0, 0, windowWidth, contentHeight);
    NSWindow *window = [[NSWindow alloc] initWithContentRect:frame
                                                   styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskFullSizeContentView
                                                     backing:NSBackingStoreBuffered
                                                       defer:NO];
    [window center];
    [window setLevel:NSFloatingWindowLevel];
    [window setTitleVisibility:NSWindowTitleHidden];
    [window setTitlebarAppearsTransparent:YES];
    [window setMovableByWindowBackground:YES];
    
    NSView *contentView = [[NSView alloc] initWithFrame:frame];
    [window setContentView:contentView];
    
    // Title
    NSTextField *titleLabel = [NSTextField labelWithString:@"Select Browser"];
    [titleLabel setFrame:NSMakeRect(padding, contentHeight - padding - textHeight, windowWidth - 2*padding, textHeight)];
    [titleLabel setAlignment:NSTextAlignmentCenter];
    [titleLabel setFont:[NSFont boldSystemFontOfSize:14]];
    [contentView addSubview:titleLabel];
    
    // URL
    NSTextField *urlLabel = [NSTextField labelWithString:[NSString stringWithFormat:@"Open %@", urlString]];
    [urlLabel setFrame:NSMakeRect(padding, contentHeight - padding - textHeight - 5 - textHeight, windowWidth - 2*padding, textHeight)];
    [urlLabel setAlignment:NSTextAlignmentCenter];
    [urlLabel setTextColor:[NSColor secondaryLabelColor]];
    [[urlLabel cell] setLineBreakMode:NSLineBreakByTruncatingTail];
    [contentView addSubview:urlLabel];
    
    CGFloat currentY = contentHeight - padding - textHeight - 5 - textHeight - 10 - buttonHeight;
    
    // Browser Buttons
    for (NSInteger i = 0; i < browsers.count; i++) {
        NSString *title = browsers[i];
        NSString *keyEq = @"";
        NSEventModifierFlags mask = 0;
        
        if (i < 9) {
            NSInteger keyNum = i + 1;
            title = [NSString stringWithFormat:@"⌘%ld | %@", (long)keyNum, browsers[i]];
            keyEq = [NSString stringWithFormat:@"%ld", (long)keyNum];
            mask = NSEventModifierFlagCommand;
        }

        NSButton *btn = [NSButton buttonWithTitle:title target:self action:@selector(browserButtonClicked:)];
        [btn setFrame:NSMakeRect(padding, currentY, windowWidth - 2*padding, buttonHeight)];
        [btn setTag:i];
        [btn setBezelStyle:NSBezelStyleRounded];
        
        if (keyEq.length > 0) {
            [btn setKeyEquivalent:keyEq];
            [btn setKeyEquivalentModifierMask:mask];
        }

        [contentView addSubview:btn];
        currentY -= (buttonHeight + buttonGap);
    }
    
    // Cancel Button
    NSButton *cancelBtn = [NSButton buttonWithTitle:@"Cancel" target:self action:@selector(browserButtonClicked:)];
    [cancelBtn setFrame:NSMakeRect(padding, currentY, windowWidth - 2*padding, buttonHeight)];
    [cancelBtn setTag:-1];
    [cancelBtn setBezelStyle:NSBezelStyleRounded];
    [cancelBtn setKeyEquivalent:@"\033"];
    [contentView addSubview:cancelBtn];
    
    [NSApp activateIgnoringOtherApps:YES];
    NSInteger response = [NSApp runModalForWindow:window];
    [window close];
    
    if (response >= 0 && response < browsers.count) {
        if (![self openURL:urlString withBrowser:browsers[response] config:config]) {
            // Don't lose the link: Safari is always installed.
            [self openURL:urlString withBrowser:@"Safari" config:NULL];
        }
    }
}

- (void)routeURL:(NSString *)urlString config:(Config *)config {
    char *targetBrowser = find_browser_for_url(config, [urlString UTF8String]);
    NSString *browserName = targetBrowser ? [NSString stringWithUTF8String:targetBrowser] : nil;
    if (browserName && [self openURL:urlString withBrowser:browserName config:config]) {
        return;
    }
    // No rule matched, or its browser can't be opened: let the user choose.
    [self showBrowserPickerForURL:urlString withConfig:config];
}

- (void)routeURLs:(NSArray<NSString *> *)urlStrings {
    _isHandlingURL = YES;
    Config *config = load_config([ConfigPath() UTF8String]);
    for (NSString *urlString in urlStrings) {
        [self routeURL:urlString config:config];
    }
    free_config(config);

    // Terminate after handling
    [NSApp terminate:nil];
}

- (void)handleGetURLEvent:(NSAppleEventDescriptor *)event withReplyEvent:(NSAppleEventDescriptor *)replyEvent {
    NSString *urlString = [[event paramDescriptorForKeyword:keyDirectObject] stringValue];
    [self routeURLs:urlString ? @[urlString] : @[]];
}

// Files opened with LinkSwitch arrive here rather than as a GetURL event: it is
// registered as a viewer of HTML documents, which is what a default browser is
// asked to open when a local .html file is double-clicked.
- (void)application:(NSApplication *)application openURLs:(NSArray<NSURL *> *)urls {
    NSMutableArray<NSString *> *urlStrings = [NSMutableArray array];
    for (NSURL *url in urls) {
        [urlStrings addObject:url.absoluteString];
    }
    [self routeURLs:urlStrings];
}

- (void)applicationWillFinishLaunching:(NSNotification *)notification {
    [[NSAppleEventManager sharedAppleEventManager] setEventHandler:self
                                                       andSelector:@selector(handleGetURLEvent:withReplyEvent:)
                                                     forEventClass:kInternetEventClass
                                                        andEventID:kAEGetURL];
}

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    // If not handling a URL (e.g. launched directly), open config file
    if (!_isHandlingURL) {
        NSString *configPath = ConfigPath();
        if (![[NSFileManager defaultManager] fileExistsAtPath:configPath]) {
            // Create directory if needed
            NSString *configDir = [configPath stringByDeletingLastPathComponent];
            [[NSFileManager defaultManager] createDirectoryAtPath:configDir withIntermediateDirectories:YES attributes:nil error:nil];
            // Create empty config
            [[NSFileManager defaultManager] createFileAtPath:configPath contents:[NSData data] attributes:nil];
        }
        [[NSWorkspace sharedWorkspace] openFile:configPath];
        [NSApp terminate:nil];
    }
}

@end

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        NSApplication *app = [NSApplication sharedApplication];
        AppDelegate *delegate = [[AppDelegate alloc] init];
        [app setDelegate:delegate];
        [app run];
    }
    return 0;
}
