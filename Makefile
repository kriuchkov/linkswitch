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
	$(CC) $(CFLAGS) -o test_runner src/config.c tests/test_config.c
	./test_runner
	rm test_runner

clean:
	rm -rf $(BUILD_DIR)
	rm -f test_runner

install: bundle
	@echo "Installing to /Applications..."
	rm -rf /Applications/$(APP_NAME)
	cp -r $(APP_BUNDLE) /Applications/
	@echo "Done."
