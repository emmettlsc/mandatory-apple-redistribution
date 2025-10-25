all:
	gcc -fPIC -shared -Wall -g -O0 -o vaapi_logger.so vaapi-logger.c -lva -lva-drm -ldl -lpthread

test: all
	rm -f /tmp/vaapi_hook.log
	LD_PRELOAD=./vaapi_logger.so google-chrome --enable-features=VaapiVideoDecoder --disable-gpu-sandbox &
	@echo "Chrome started. Monitor with: tail -f /tmp/vaapi_hook.log"

clean:
	rm -f vaapi_logger.so

.PHONY: all test clean
