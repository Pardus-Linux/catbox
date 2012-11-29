all: test


test: build
	@echo "Testing..."

build:
	python setup.py build --enable-pcre

.PHONY=clean
clean:
	python setup.py clean --all
