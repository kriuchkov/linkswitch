<p align="center">
  <img src="assets/logo.png" width="112" height="112" alt="" />
</p>

<h1 align="center">LinkSwitch</h1>

<p align="center">
  An ultra-lightweight native link router for macOS.<br />
  Every link opens in the right browser — by rule, or from a picker.
</p>

<p align="center">
  <a href="https://github.com/kriuchkov/linkswitch/releases">
    <img src="https://img.shields.io/github/v/release/kriuchkov/linkswitch?style=flat-square" alt="Release" />
  </a>
  <a href="LICENSE">
    <img src="https://img.shields.io/github/license/kriuchkov/linkswitch?style=flat-square" alt="License" />
  </a>
  <a href="https://github.com/kriuchkov/linkswitch/actions">
    <img src="https://img.shields.io/github/actions/workflow/status/kriuchkov/linkswitch/build.yml?style=flat-square&label=build" alt="Build" />
  </a>
  <a href="#option-1-homebrew-recommended">
    <img src="https://img.shields.io/badge/homebrew-cask-FBB040?style=flat-square&logo=homebrew&logoColor=white" alt="Homebrew cask" />
  </a>
  <img src="https://img.shields.io/badge/macOS-10.13%2B-000000?style=flat-square&logo=apple&logoColor=white" alt="macOS 10.13 or newer" />
  <img src="https://img.shields.io/badge/C%20%2F%20Objective--C-A8B9CC?style=flat-square&logo=c&logoColor=white" alt="Written in C and Objective-C" />
</p>

<p align="center">
  <a href="#features"><strong>Features</strong></a>
  <span>&nbsp;•&nbsp;</span>
  <a href="#installation"><strong>Installation</strong></a>
  <span>&nbsp;•&nbsp;</span>
  <a href="#configuration"><strong>Configuration</strong></a>
  <span>&nbsp;•&nbsp;</span>
  <a href="#profiles"><strong>Profiles</strong></a>
  <br />
  <a href="#rules"><strong>Rules</strong></a>
  <span>&nbsp;•&nbsp;</span>
  <a href="#browsers-list"><strong>Browsers</strong></a>
  <span>&nbsp;•&nbsp;</span>
  <a href="#inspiration"><strong>Inspiration</strong></a>
  <span>&nbsp;•&nbsp;</span>
  <a href="#license"><strong>License</strong></a>
</p>

**LinkSwitch** is an ultra-lightweight native link router for macOS, written in C/Objective-C. It replaces the default browser and allows you to open links in different browsers based on rules or via a selection menu.

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
   curl -o ~/.config/linkswitch/config.yaml https://raw.githubusercontent.com/kriuchkov/linkswitch/master/config.yaml
   ```

5. Set as Default Browser: Open **System Settings** → **Desktop & Dock** → **Default web browser** and select **LinkSwitch**.

   > **Note:** If LinkSwitch does not appear in the list, launch the application manually once to register it with the system, or run the following command:
   >
   > ```sh
   > /System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister -f /Applications/LinkSwitch.app
   > ```

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

3. **Configuration:** Create the configuration folder and file:

   ```sh
   mkdir -p ~/.config/linkswitch
   cp config.yaml ~/.config/linkswitch/
   ```

4. **Set as Default Browser:** Open **System Settings** → **Desktop & Dock** → **Default web browser** and select **LinkSwitch**. (The `make install` command attempts to register the app automatically, so it should appear in the list immediately.)

## Configuration

Example `~/.config/linkswitch/config.yaml`:

```yaml
default: Safari

# Define browser profiles with specific arguments (optional)
profiles:
  - name: "Chrome Work"
    app: "Google Chrome"
    args: "--profile-directory='Profile 1'"
  - name: "Chrome Personal"
    app: "Google Chrome"
    args: "--profile-directory='Default'"

rules:
  # Open Zoom links in Zoom app
  - match: "zoom.us"
    browser: "zoom.us" 
  
  # Work links - in Chrome Work profile
  - match: "github.com|gitlab.com"
    browser: "Chrome Work"
  
  # Local development - in Firefox
  - match: "localhost|127.0.0.1"
    browser: "Firefox"

# List of browsers for the selection menu (if no rule is matched)
# You can include both regular app names and defined profile names here.
browsers:
  - Safari
  - "Google Chrome"
  - "Chrome Work"
  - Firefox
```

### Profiles

You can define custom profiles for browsers that support command-line arguments (like Chrome, Brave, Edge). This is useful for opening links in specific user profiles.

- `name`: The unique name you will use in `rules` and `browsers` list.
- `app`: The actual application name (e.g., "Google Chrome").
- `args`: Command-line arguments to pass when launching.
- `cmd`: (Optional) A full custom command template. Use `{url}` as a placeholder. This overrides `app` and `args`.

#### Basic Examples

```yaml
profiles:
  # Standard profile (using app name + args)
  - name: "Chrome Work"
    app: "Google Chrome"
    args: "--profile-directory='Profile 1'"

  # Custom command (e.g. Firefox Private Window)
  - name: "Firefox Private"
    cmd: "/Applications/Firefox.app/Contents/MacOS/firefox --private-window '{url}'"
```

#### Example: Secure Sandboxed Chrome

You can use `cmd` to run Chrome inside a strict macOS Sandbox, hiding your personal data (files, apps) and hardware info (serial number, UUID) from the browser.

1. Create a sandbox profile `~/.config/linkswitch/chrome_strict.sb` (see [`resources/chrome_strict.sb.example`](resources/chrome_strict.sb.example) in the repo).
2. Add this profile to your `config.yaml`:

```yaml
profiles:
  - name: Secure Chrome
    cmd: "nohup /usr/bin/sandbox-exec -f ~/.config/linkswitch/chrome_strict.sb '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome' --user-data-dir=/tmp/secure_chrome --no-first-run --password-store=basic --no-sandbox --no-default-browser-check '{url}' >/dev/null 2>&1 &"
```

### Rules

Rules can be defined in two ways:

**1. In `config.yaml`**:

- `match`: A regex string to match against the URL.
    - Setting a match of `.*` to a browser will act as a catch all (only works in config)
- `browser`: The name of the browser (or profile) to open.

**2. Directory-based**:

Create a `rules/` directory next to your config file (`~/.config/linkswitch/rules/`). Each file is named by a browser slug (lowercase, spaces → hyphens) and contains one domain per line. Lines starting with `#` and blank lines are ignored.

```
~/.config/linkswitch/
├── config.yaml
└── rules/
    ├── safari
    ├── google-chrome
    ├── firefox
    └── secure-chrome     # profile names work too
```

Example `rules/safari`:

```
google.com
github.com
gmail.com
```

Directory rules are merged with config rules. **Directory rules take priority**—when a URL matches both a directory rule and a config rule, the directory rule wins. Config rules add additional mappings for domains not covered by the directory.

### Browsers List

This list defines the buttons shown in the picker window when no rule matches the URL. If this list is empty or missing, the app will show only Safari.

```yaml
browsers:
  - Safari
  - Google Chrome
  - Firefox
```

## Inspiration

This project is inspired by [Browserosaurus](https://github.com/will-stone/browserosaurus). ❤️

## Uninstall

Delete `/Applications/LinkSwitch.app`.

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.
