# 
# dev builds
# 

DEFAULT_BUILD_DIR := $(PWD)/build/default
.PHONY: run
run: lint fmt
	cmake -B $(DEFAULT_BUILD_DIR) -S . -DCMAKE_CXX_COMPILER=$(shell which clang++)
	cmake --build $(DEFAULT_BUILD_DIR) -j$(shell sysctl -n hw.ncpu)
	ASAN_OPTIONS=detect_leaks=1 LSAN_OPTIONS=suppressions=$(PWD)/suppressions-asan.txt:print_suppressions=0 $(DEFAULT_BUILD_DIR)/binary

LEAKS_BUILD_DIR := $(PWD)/build/leaks
.PHONY: leaks
leaks: lint fmt
	cmake -B $(LEAKS_BUILD_DIR) -S . -DDISABLE_ASAN=ON
	cmake --build $(LEAKS_BUILD_DIR) -j$(shell sysctl -n hw.ncpu)
	codesign -s - -f --entitlements entitlements.plist $(LEAKS_BUILD_DIR)/binary
	leaks --atExit --list --groupByType \
		-exclude _glfwDestroyWindowCocoa \
		-exclude _glfwPollEventsCocoa \
		-exclude _glfwInitCocoa \
		-exclude _glfwCreateWindowCocoa \
		-exclude __createContextTelemetryDataWithQueueLabelAndCallstack_block_invoke \
		-- $(LEAKS_BUILD_DIR)/binary

# 
# release builds
# 

RELEASE_BUILD_DIR := $(PWD)/build/release
.PHONY: run-release
run-release: lint fmt
	cmake -B $(RELEASE_BUILD_DIR) -S . -DCMAKE_CXX_COMPILER=$(shell which clang++) -DCMAKE_BUILD_TYPE=Release -DDISABLE_ASAN=ON -DDISABLE_UBSAN=ON -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON -DCMAKE_BUILD_TYPE=Release
	cmake --build $(RELEASE_BUILD_DIR) -j$(shell sysctl -n hw.ncpu)
	$(RELEASE_BUILD_DIR)/binary

WASM_BUILD_DIR := $(PWD)/build/wasm
.PHONY: wasm
wasm:
	rm -rf $(WASM_BUILD_DIR)
	mkdir -p $(WASM_BUILD_DIR)
	docker run --rm \
		-v $(PWD):/src \
		-u $(shell id -u):$(shell id -g) \
		emscripten/emsdk:latest \
		/bin/bash -c "emcmake cmake -B build/wasm -S . -DPLATFORM=Web -DCMAKE_BUILD_TYPE=Release -DDISABLE_ASAN=ON -DDISABLE_UBSAN=ON -DCMAKE_EXECUTABLE_SUFFIX='.html' -DCMAKE_EXECUTABLE_SUFFIX_CXX='.html' -DCMAKE_EXE_LINKER_FLAGS='-sASYNCIFY -sUSE_GLFW=3 -sGL_ENABLE_GET_PROC_ADDRESS' && cmake --build build/wasm"
	open http://localhost:8000/binary.html
	python3 -m http.server --directory $(WASM_BUILD_DIR) 8000

# 
# tools
# 

TEST_BUILD_DIR := $(PWD)/build/test
.PHONY: test
test: lint
	cmake -B $(TEST_BUILD_DIR) -S . -DBUILD_TESTS=ON
	cmake --build $(TEST_BUILD_DIR) -j$(shell sysctl -n hw.ncpu)
	cd $(TEST_BUILD_DIR) && ctest --output-on-failure

.PHONY: lint
lint:
	cppcheck --enable=all --std=c++23 --language=c++ --suppressions-list=suppressions-cppcheck.txt --check-level=exhaustive --inconclusive --inline-suppr -I src/ -I $(DEFAULT_BUILD_DIR)/_deps/raylib-src/src src/

.PHONY: fmt
fmt:
	uvx --from cmakelang cmake-format --dangle-parens --line-width 500 -i CMakeLists.txt
	find . -type f -not -path '*/build/*' \( -name "*.cpp" -o -name "*.hpp" \) | xargs clang-format -i

.PHONY: up
up: fmt lint
	git add . && git commit -m up && git push
