cask "linkswitch" do
  version "1.0.0"
  sha256 "REPLACE_WITH_SHA256_OF_ZIP"

  url "https://github.com/kriuchkov/linkswitch/releases/download/v#{version}/LinkSwitch.zip"
  name "LinkSwitch"
  desc "Lightweight ultra-lightweight native link router for macOS"
  homepage "https://github.com/kriuchkov/linkswitch"

  app "LinkSwitch.app"

  zap trash: [
    "~/.config/linkswitch",
    "~/Library/Preferences/com.kriuchkov.linkswitch.plist",
  ]
end
