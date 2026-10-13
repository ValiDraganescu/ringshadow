# Builds AutocraftCore (../Source/AutocraftCore), the server's world code
# (world/) and their tests with plain clang++, no Unreal, and runs them:
# `make -C unreal/server test`. `FILTER=snapshot` runs only tests whose name
# contains "snapshot". `make -C unreal/server bench` builds and runs the
# region bench at -O3. Objects go to build/. The tests use core-tests' tiny
# framework (test.h, main.cpp).

CXX := clang++
CORE := ../Source/AutocraftCore
CORE_TESTS := ../core-tests/tests
CXXFLAGS := -std=c++20 -O2 -Wall -Wextra -Wshadow -Werror -ffp-contract=off -fno-exceptions -fno-rtti \
	-I$(CORE)/Public -I$(CORE)/ThirdParty -Iworld -I$(CORE_TESTS) -MMD -MP
# The core's tracking store speaks SQLite's C API (as in core-tests).
LDLIBS := -lsqlite3

CORE_SRC := $(wildcard $(CORE)/Private/*.cpp)
WORLD_SRC := $(wildcard world/*.cpp)
TEST_SRC := $(wildcard tests/*.cpp)
CORE_OBJ := $(patsubst $(CORE)/Private/%.cpp,build/core/%.o,$(CORE_SRC))
WORLD_OBJ := $(patsubst world/%.cpp,build/world/%.o,$(WORLD_SRC))
TEST_OBJ := $(patsubst tests/%.cpp,build/tests/%.o,$(TEST_SRC)) build/tests/main.o
BIN := build/server-tests

.PHONY: all test bench clean
all: test

test: $(BIN)
	./$(BIN) $(FILTER)

$(BIN): $(CORE_OBJ) $(WORLD_OBJ) $(TEST_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)

build/core/%.o: $(CORE)/Private/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

build/world/%.o: world/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

build/tests/%.o: tests/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

build/tests/main.o: $(CORE_TESTS)/main.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# `make bench`: build/regionbench (bench/RegionBench.cpp), everything at -O3.
BENCH_FLAGS := $(subst -O2,-O3,$(CXXFLAGS)) -I../core-tests/bench
BENCH_OBJ := $(patsubst $(CORE)/Private/%.cpp,build/bench/core/%.o,$(CORE_SRC)) \
	$(patsubst world/%.cpp,build/bench/world/%.o,$(WORLD_SRC)) build/bench/RegionBench.o

bench: build/regionbench
	./build/regionbench 30 ../core-tests/fixtures/rally-jam.json

build/regionbench: $(BENCH_OBJ)
	$(CXX) $(BENCH_FLAGS) -o $@ $^ $(LDLIBS)

build/bench/core/%.o: $(CORE)/Private/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(BENCH_FLAGS) -c $< -o $@

build/bench/world/%.o: world/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(BENCH_FLAGS) -c $< -o $@

build/bench/RegionBench.o: bench/RegionBench.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(BENCH_FLAGS) -c $< -o $@

clean:
	rm -rf build

-include $(CORE_OBJ:.o=.d) $(WORLD_OBJ:.o=.d) $(TEST_OBJ:.o=.d) $(BENCH_OBJ:.o=.d)
