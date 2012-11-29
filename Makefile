all: test

test: build
	@echo "Testing..."
	PYTHONPATH=build/ testify tests

build:
	python setup.py build --enable-pcre --build-lib=build/

.PHONY=clean
clean:
	python setup.py clean --all
