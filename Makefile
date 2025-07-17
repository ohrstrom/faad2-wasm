SHELL := bash
.ONESHELL:
.SHELLFLAGS := -eu -o pipefail -c
.DELETE_ON_ERROR:
MAKEFLAGS += --warn-undefined-variables
MAKEFLAGS += --no-builtin-rules

EMSDK_VERSION := 4.0.11

.PHONY: setup-submodules
setup-submodules:
	git submodule init
	git submodule update
	git submodule status


.PHONY: setup-emsdk
setup-emsdk:
	cd emsdk && \
	  ./emsdk install $(EMSDK_VERSION)

.PHONY: setup
setup: setup-submodules setup-emsdk

.PHONY: ensure-headers
ensure-headers:
	cp -u faad2/include/faad.h.in faad2/include/faad.h

.PHONY: patch-libfaad
patch-libfaad:
	patch -p0 --forward --quiet < patch/libfaad.diff || echo "patch failed or already applied"

.PHONY: clean
clean:
	rm -f pkg/faad2_wasm.*

.PHONY: build
build:
	cd emsdk && \
	  ./emsdk activate $(EMSDK_VERSION) && \
	  cd ..
	source emsdk/emsdk_env.sh && \
	  cd faad2 && \
	  emcc \
	  ../src/faad2_wasm.c \
	  libfaad/*.c \
	  -I. -Ilibfaad -Iinclude \
	  -O3 \
	  -DPACKAGE_VERSION="\"2.11.2\"" \
	  -s STACK_SIZE=262144 \
	  -s EXPORTED_FUNCTIONS='["_get_faad_capabilities", "_init_decoder", "_decode_frame", "_malloc", "_free"]' \
	  -s EXPORTED_RUNTIME_METHODS='["cwrap", "getValue", "setValue"]' \
	  -s MODULARIZE=1 \
	  -s EXPORT_NAME="Faad2Module" \
	  -s ALLOW_MEMORY_GROWTH=1 \
	  -s ENVIRONMENT="web" \
	  -o ../pkg/faad2_wasm.mjs
