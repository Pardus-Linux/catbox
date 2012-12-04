BUILD_LIB_DIR := "build"

all: test

test: build
	@echo "Testing..."
	PYTHONPATH=${BUILD_LIB_DIR} testify -v --summary tests

build: ${BUILD_LIB_DIR}/catbox.so

${BUILD_LIB_DIR}/catbox.so:
	python setup.py build --enable-pcre --build-lib=${BUILD_LIB_DIR}

.PHONY=clean
clean:
	python setup.py clean --all --build-lib=${BUILD_LIB_DIR}

