CC = clang
CFLAGS = -Wall -Os -flto -fobjc-arc
LDFLAGS = -framework Cocoa -flto -Wl,-dead_strip

BUILD_DIR = build
SRC = src/main.m src/config.c
TARGET = $(BUILD_DIR)/LinkSwitch
APP_NAME = LinkSwitch.app
APP_BUNDLE = $(BUILD_DIR)/$(APP_NAME)
CONTENTS = $(APP_BUNDLE)/Contents
MACOS = $(CONTENTS)/MacOS
RESOURCES = $(CONTENTS)/Resources

all: $(TARGET) bundle

$(TARGET): $(SRC)
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^
	strip $@

bundle: $(TARGET) resources/Info.plist
	mkdir -p $(MACOS)
	mkdir -p $(RESOURCES)
	cp $(TARGET) $(MACOS)/
	cp resources/Info.plist $(CONTENTS)/
	@if [ -f resources/AppIcon.icns ]; then cp resources/AppIcon.icns $(RESOURCES)/; fi
	@echo "Built $(APP_BUNDLE)"

test: src/config.c tests/test_config.c
	$(CC) $(CFLAGS) -I. -o test_runner src/config.c tests/test_config.c
	./test_runner
	rm test_runner

clean:
	rm -rf $(BUILD_DIR)
	rm -f test_runner

install: bundle
	@echo "Installing to /Applications..."
	rm -rf /Applications/$(APP_NAME)
	cp -r $(APP_BUNDLE) /Applications/
	@echo "Registering with Launch Services..."
	/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister -f /Applications/$(APP_NAME)
	@echo "Done."

# Release targets
# Usage: make release VERSION=1.1.0
VERSION ?= $(shell git describe --tags --abbrev=0 2>/dev/null || echo "0.0.0")
HOMEBREW_TAP ?= ../homebrew-tap

release: bundle
	@if [ "$(VERSION)" = "0.0.0" ]; then echo "Usage: make release VERSION=1.1.0"; exit 1; fi
	@echo "Creating release v$(VERSION)..."
	cd $(BUILD_DIR) && zip -r ../build/LinkSwitch.zip $(APP_NAME)
	@echo "Created LinkSwitch.zip"
	@echo ""
	@echo "Next steps:"
	@echo "  1. git tag v$(VERSION)"
	@echo "  2. git push origin v$(VERSION)"
	@echo "  3. Wait for GitHub Actions to create the Release"
	@echo "  4. Update homebrew-tap"

.PHONY: all bundle test clean install release
