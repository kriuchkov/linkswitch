# LinkSwitch

**LinkSwitch** is an ultra-lightweight native link router for macOS, written in C/Objective-C.
It replaces the default browser and allows you to open links in different browsers based on rules or via a selection menu.

## Inspiration

This project is inspired by [Browserosaurus](https://github.com/will-stone/browserosaurus). ❤️

## Features

- **Instant Launch**: Written in native code, consumes 20MB RAM.
- **Zero Background Usage**: The app runs only when a link is clicked and exits immediately after.
- **Rules**: Flexible configuration via YAML (`~/.config/linkswitch/config.yaml`).
- **Selection Menu**: If no rule is matched, shows a system window with a list of installed browsers.

## Installation

### Option 1: Homebrew (Recommended)

```sh
brew tap kriuchkov/tap
brew install --cask linkswitch
```

### Option 2: Download from Releases

1. Download `LinkSwitch.zip` from [Releases](https://github.com/kriuchkov/linkswitch/releases)
2. Unzip and move `LinkSwitch.app` to `/Applications`
3. Remove the quarantine attribute (required for unsigned apps):

   ```sh
   xattr -cr /Applications/LinkSwitch.app
   ```

4. Create the configuration folder and file:

   ```sh
   mkdir -p ~/.config/linkswitch
   curl -o ~/.config/linkswitch/config.yaml https://raw.githubusercontent.com/kriuchkov/linkswitch/main/config.yaml
   ```

5. Set as Default Browser:
   Open **System Settings** → **Desktop & Dock** → **Default web browser** and select **LinkSwitch**.

### Option 3: Build from Source

1. **Build:**

   ```sh
   make
   ```

2. **Install:**

   ```sh
   make install
   ```

   The command will copy `LinkSwitch.app` to the `/Applications` folder.

3. **Configuration:**
   Create the configuration folder and file:

   ```sh
   mkdir -p ~/.config/linkswitch
   cp config.yaml ~/.config/linkswitch/
   ```

4. **Set as Default Browser:**
   Open **System Settings** → **Desktop & Dock** → **Default web browser** and select **LinkSwitch**.

## Configuration

Example `~/.config/linkswitch/config.yaml`:

```yaml
default: Safari

rules:
  # Open Zoom links in Zoom app
  - match: "zoom.us"
   browser: "zoom.us" 
  
  # Work links - in Chrome
  - match: "github.com|gitlab.com"
   browser: "Google Chrome"
  
  # Local development - in Firefox
  - match: "localhost|127.0.0.1"
   browser: "Firefox"

# List of browsers for the selection menu (if no rule is matched)
# If this list is empty or missing, the app will show only Safari.
browsers:
  - Safari
  - Google Chrome
  - Firefox
  - Brave Browser

```

## Uninstall

Delete `/Applications/LinkSwitch.app`.

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.
