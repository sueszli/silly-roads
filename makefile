CLANG_PATH := /opt/homebrew/opt/llvm/bin/clang++
NCPU := $(shell sysctl -n hw.ncpu)

BUILD_DIR := /tmp/build
LEAKS_BUILD_DIR := /tmp/leaks-build
TEST_BUILD_DIR := /tmp/test-build
RELEASE_BUILD_DIR := /tmp/release-build

.PHONY: run
run: lint
	cmake -B $(BUILD_DIR) -S . -DCMAKE_CXX_COMPILER=$(CLANG_PATH)
	cmake --build $(BUILD_DIR) -j$(NCPU)
	ASAN_OPTIONS=detect_leaks=1 LSAN_OPTIONS=suppressions=$(PWD)/suppressions-asan.txt $(BUILD_DIR)/binary

.PHONY: run-release
run-release: lint
	cmake -B $(RELEASE_BUILD_DIR) -S . -DCMAKE_CXX_COMPILER=$(CLANG_PATH) -DCMAKE_BUILD_TYPE=Release -DDISABLE_ASAN=ON -DDISABLE_UBSAN=ON -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON
	cmake --build $(RELEASE_BUILD_DIR) -j$(NCPU)
	$(RELEASE_BUILD_DIR)/binary

.PHONY: run-leaks
run-leaks: lint
	cmake -B $(LEAKS_BUILD_DIR) -S . -DDISABLE_ASAN=ON
	cmake --build $(LEAKS_BUILD_DIR) -j$(NCPU)
	codesign -s - -f --entitlements entitlements.plist $(LEAKS_BUILD_DIR)/binary
	leaks --atExit --list --groupByType -- $(LEAKS_BUILD_DIR)/binary

.PHONY: test
test: lint
	cmake -B $(TEST_BUILD_DIR) -S . -DBUILD_TESTS=ON
	cmake --build $(TEST_BUILD_DIR) -j$(NCPU)
	cd $(TEST_BUILD_DIR) && ctest --output-on-failure

#
# utils
#

.PHONY: lint
lint:
	cppcheck --enable=all --std=c++23 --language=c++ --suppressions-list=suppressions-cppcheck.txt --check-level=exhaustive --inconclusive --inline-suppr -I src/ src/

.PHONY: fmt
fmt:
	uvx --from cmakelang cmake-format --dangle-parens --line-width 500 -i CMakeLists.txt
	find . -name "*.cpp" -o -name "*.h" | xargs clang-format -i
