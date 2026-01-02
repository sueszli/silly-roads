DEFAULT_BUILD_DIR := $(PWD)/build/default
.PHONY: run
run: lint
	cmake -B $(DEFAULT_BUILD_DIR) -S . -DCMAKE_CXX_COMPILER=$(shell which clang++)
	cmake --build $(DEFAULT_BUILD_DIR) -j$(shell sysctl -n hw.ncpu)
	ASAN_OPTIONS=detect_leaks=1 LSAN_OPTIONS=suppressions=$(PWD)/suppressions-asan.txt $(DEFAULT_BUILD_DIR)/binary

RELEASE_BUILD_DIR := $(PWD)/build/release
.PHONY: run-release
run-release: lint
	cmake -B $(RELEASE_BUILD_DIR) -S . -DCMAKE_CXX_COMPILER=$(shell which clang++) -DCMAKE_BUILD_TYPE=Release -DDISABLE_ASAN=ON -DDISABLE_UBSAN=ON -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON
	cmake --build $(RELEASE_BUILD_DIR) -j$(shell sysctl -n hw.ncpu)
	$(RELEASE_BUILD_DIR)/binary

LEAKS_BUILD_DIR := $(PWD)/build/leaks
.PHONY: run-leaks
run-leaks: lint
	cmake -B $(LEAKS_BUILD_DIR) -S . -DDISABLE_ASAN=ON
	cmake --build $(LEAKS_BUILD_DIR) -j$(shell sysctl -n hw.ncpu)
	codesign -s - -f --entitlements entitlements.plist $(LEAKS_BUILD_DIR)/binary
	leaks --atExit --list --groupByType -- $(LEAKS_BUILD_DIR)/binary

TEST_BUILD_DIR := $(PWD)/build/test
.PHONY: test
test: lint
	cmake -B $(TEST_BUILD_DIR) -S . -DBUILD_TESTS=ON
	cmake --build $(TEST_BUILD_DIR) -j$(shell sysctl -n hw.ncpu)
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
