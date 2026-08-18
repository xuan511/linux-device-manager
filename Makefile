.PHONY: build test demo clean

build:
	cmake --preset debug
	cmake --build --preset debug -j

test: build
	ctest --preset debug --output-on-failure

demo: build
	./scripts/run-demo.sh

clean:
	cmake -E remove_directory build

