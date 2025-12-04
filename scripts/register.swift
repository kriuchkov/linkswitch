import Cocoa
import CoreServices

let bundleID = "com.nkriuchkov.linkswitch" as CFString

print("Attempting to register \(bundleID) as default browser...")

// Force LaunchServices to register the app first
let appUrl = URL(fileURLWithPath: "/Applications/LinkSwitch.app")
LSRegisterURL(appUrl as CFURL, true)

// Attempt to set as default
let httpResult = LSSetDefaultHandlerForURLScheme("http" as CFString, bundleID)
let httpsResult = LSSetDefaultHandlerForURLScheme("https" as CFString, bundleID)

// Try to set for HTML files as well
let htmlResult = LSSetDefaultRoleHandlerForContentType(kUTTypeHTML, .all, bundleID)

print("HTTP Result: \(httpResult)")
print("HTTPS Result: \(httpsResult)")
print("HTML Result: \(htmlResult)")

if httpResult == noErr {
    print("Request sent successfully. Check for a system prompt or check System Settings.")
} else {
    print("Failed with error code: \(httpResult)")
    if httpResult == -54 {
        print("Error -54 is a permission error. This is expected on modern macOS if user interaction is required.")
        print("However, this attempt should have forced the app into the LaunchServices database.")
    }
}
