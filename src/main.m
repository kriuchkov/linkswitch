#import <Cocoa/Cocoa.h>
#include "config.h"
#include <sys/stat.h>

@interface AppDelegate : NSObject <NSApplicationDelegate>
@end

@implementation AppDelegate {
    BOOL _isHandlingURL;
}

- (void)openURL:(NSString *)urlString withBrowser:(NSString *)browserName {
    NSURL *url = [NSURL URLWithString:urlString];
    NSWorkspace *workspace = [NSWorkspace sharedWorkspace];
    
    if (browserName) {
        NSString *appPath = [workspace fullPathForApplication:browserName];
        if (appPath) {
            [workspace openURLs:@[url]
                       withAppBundleIdentifier:[[NSBundle bundleWithPath:appPath] bundleIdentifier]
                       options:NSWorkspaceLaunchDefault
                       additionalEventParamDescriptor:nil
                       launchIdentifiers:nil];
        } else {
            NSLog(@"Could not find browser: %@", browserName);
            [workspace openURL:url];
        }
    } else {
        [workspace openURL:url];
    }
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
        [self openURL:urlString withBrowser:browsers[response]];
    }
}

- (void)handleGetURLEvent:(NSAppleEventDescriptor *)event withReplyEvent:(NSAppleEventDescriptor *)replyEvent {
    _isHandlingURL = YES;
    NSString *urlString = [[event paramDescriptorForKeyword:keyDirectObject] stringValue];
    if (!urlString) return;
    
    // Load Config
    NSString *configPath = [NSHomeDirectory() stringByAppendingPathComponent:@".config/linkswitch/config.yaml"];
    
    Config *config = load_config([configPath UTF8String]);
    char *targetBrowser = NULL;
    
    if (config) {
        targetBrowser = find_browser_for_url(config, [urlString UTF8String]);
    }
    
    if (targetBrowser) {
        NSString *browserName = [NSString stringWithUTF8String:targetBrowser];
        [self openURL:urlString withBrowser:browserName];
    } else {
        // No rule matched, show picker
        [self showBrowserPickerForURL:urlString withConfig:config];
    }
    
    if (config) free_config(config);
    
    // Terminate after handling
    [NSApp terminate:nil];
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
        NSString *configPath = [NSHomeDirectory() stringByAppendingPathComponent:@".config/linkswitch/config.yaml"];
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
