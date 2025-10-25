# llm gen
CC=gcc
CFLAGS=-fPIC -shared -Wall -g -O0
LIBS=-lva -lva-drm -ldl
TARGET=vaapi_logger.so
SOURCE=vaapi_logger.c

all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)
	@echo "Built $(TARGET) successfully!"
	@echo ""
	@echo "Usage:"
	@echo "  LD_PRELOAD=./$(TARGET) google-chrome --enable-features=VaapiVideoDecoder"
	@echo ""

test: $(TARGET)
	@echo "Killing any existing Chrome processes..."
	@pkill chrome || true
	@sleep 2
	@echo "Starting Chrome with VA-API logger hook..."
	LD_PRELOAD=./$(TARGET) google-chrome --enable-features=VaapiVideoDecoder --enable-logging --log-level=0 &
	@echo "Chrome started with VA-API hook. Check terminal for log output."
	@echo "Navigate to any video (YouTube, etc.) to see buffer contents logged."

test-mpv: $(TARGET)
	@echo "Testing with mpv (if available)..."
	@if command -v mpv >/dev/null 2>&1; then \
		echo "Starting mpv with VA-API..."; \
		LD_PRELOAD=./$(TARGET) mpv --hwdec=vaapi --vo=gpu test_video.mp4 || echo "No test video found, try: youtube-dl some_video_url"; \
	else \
		echo "mpv not found. Install with: sudo apt install mpv"; \
	fi

# Check if VA-API is working on your system
check-vaapi:
	@echo "Checking VA-API support on your system..."
	@if command -v vainfo >/dev/null 2>&1; then \
		vainfo; \
	else \
		echo "vainfo not found. Install with: sudo apt install libva-utils"; \
	fi

# Debug target - run with strace to see system calls
debug: $(TARGET)
	@echo "Running Chrome with strace to see VA-API calls..."
	@pkill chrome || true
	@sleep 2
	strace -e trace=openat,ioctl -o va_strace.log LD_PRELOAD=./$(TARGET) google-chrome --enable-features=VaapiVideoDecoder &
	@echo "Chrome started with strace logging. VA-API calls will be in va_strace.log"

# Clean build artifacts
clean:
	rm -f $(TARGET)
	rm -f va_strace.log
	@echo "Cleaned build artifacts"

# Install dependencies (Ubuntu/Debian)
install-deps:
	@echo "Installing VA-API development dependencies..."
	sudo apt update
	sudo apt install -y libva-dev libva-drm2 libva-utils build-essential

# Show help
help:
	@echo "Available targets:"
	@echo "  all          - Build the VA-API logger hook"
	@echo "  test         - Build and test with Chrome"
	@echo "  test-mpv     - Build and test with mpv"
	@echo "  check-vaapi  - Check if VA-API is working"
	@echo "  debug        - Run with strace for debugging"
	@echo "  install-deps - Install required dependencies"
	@echo "  clean        - Remove build artifacts"
	@echo "  help         - Show this help"

.PHONY: all test test-mpv check-vaapi debug clean install-deps help
