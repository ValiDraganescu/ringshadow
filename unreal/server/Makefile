# Builds AutocraftCore (../Source/AutocraftCore), AutocraftNet
# (../Source/AutocraftNet), the server's world (world/: regions, the world map), the server program
# (program/, build/autocraft-server) and the headless client (client/) with
# plain clang++, no Unreal, and the region tool (region/, build/autocraft-region,
# docs/regions.md).
#
#   make -C unreal/server              build/autocraft-server, build/autocraft-region
#   make -C unreal/server test         the tests (FILTER=snapshot: only names with "snapshot")
#   make -C unreal/server bench        the region bench and the network bench, at -O3
#
# Objects go to build/. The tests use core-tests' tiny framework (test.h, main.cpp)
# and net-tests' in-memory network (rig.h).

CXX := clang++
CORE := ../Source/AutocraftCore
NET := ../Source/AutocraftNet
CORE_TESTS := ../core-tests/tests
CXXFLAGS := -std=c++20 -O2 -Wall -Wextra -Wshadow -Werror -ffp-contract=off -fno-exceptions -fno-rtti \
	-I$(CORE)/Public -I$(CORE)/ThirdParty -I$(NET)/Public -I$(NET)/Private -Iworld -Iprogram -Iclient -Iregion \
	-I$(CORE_TESTS) -I../net-tests/tests -MMD -MP
# The core's tracking store speaks SQLite's C API (as in core-tests).
LDLIBS := -lsqlite3

CORE_SRC := $(wildcard $(CORE)/Private/*.cpp)
NET_SRC := $(wildcard $(NET)/Private/*.cpp)
WORLD_SRC := $(wildcard world/*.cpp)
CLIENT_SRC := $(wildcard client/*.cpp)
TEST_SRC := $(wildcard tests/*.cpp)
REGION_SRC := region/Preview.cpp region/StbImageWrite.cpp
CORE_OBJ := $(patsubst $(CORE)/Private/%.cpp,build/core/%.o,$(CORE_SRC))
NET_OBJ := $(patsubst $(NET)/Private/%.cpp,build/net/%.o,$(NET_SRC)) build/net/monocypher.o
WORLD_OBJ := $(patsubst world/%.cpp,build/world/%.o,$(WORLD_SRC))
SERVER_OBJ := build/program/Server.o
CLIENT_OBJ := $(patsubst client/%.cpp,build/client/%.o,$(CLIENT_SRC))
TEST_OBJ := $(patsubst tests/%.cpp,build/tests/%.o,$(TEST_SRC)) build/tests/main.o
REGION_OBJ := $(patsubst region/%.cpp,build/region/%.o,$(REGION_SRC))
REGION_TOOL := build/autocraft-region
LIB_OBJ := $(CORE_OBJ) $(NET_OBJ) $(WORLD_OBJ) $(SERVER_OBJ)
PROGRAM := build/autocraft-server
BIN := build/server-tests

.PHONY: all test bench clean
all: $(PROGRAM) $(REGION_TOOL)

# The end-to-end tests start build/autocraft-server.
test: $(BIN) $(PROGRAM) $(REGION_TOOL)
	./$(BIN) $(FILTER)

$(PROGRAM): $(LIB_OBJ) build/program/main.o
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)

$(BIN): $(LIB_OBJ) $(CLIENT_OBJ) $(REGION_OBJ) $(TEST_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)

$(REGION_TOOL): $(CORE_OBJ) $(REGION_OBJ) build/region/main.o
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)

build/region/%.o: region/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

build/core/%.o: $(CORE)/Private/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

build/net/%.o: $(NET)/Private/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

build/world/%.o: world/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Monocypher (ThirdParty/monocypher), C as it comes.
MONOCYPHER := $(NET)/ThirdParty/monocypher/monocypher.c
build/net/monocypher.o: $(MONOCYPHER) $(NET)/ThirdParty/monocypher/monocypher.h
	@mkdir -p $(dir $@)
	clang -std=c99 -O2 -c $< -o $@

build/bench/net/monocypher.o: $(MONOCYPHER) $(NET)/ThirdParty/monocypher/monocypher.h
	@mkdir -p $(dir $@)
	clang -std=c99 -O3 -c $< -o $@

build/program/%.o: program/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

build/client/%.o: client/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

build/tests/%.o: tests/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

build/tests/main.o: $(CORE_TESTS)/main.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# `make bench`: build/regionbench (bench/RegionBench.cpp), build/netbench
# (bench/NetBench.cpp), build/worldbench (bench/WorldBench.cpp) and
# build/daybench (bench/DayBench.cpp), everything at -O3.
BENCH_FLAGS := $(subst -O2,-O3,$(CXXFLAGS)) -I../core-tests/bench
BENCH_LIB := $(patsubst $(CORE)/Private/%.cpp,build/bench/core/%.o,$(CORE_SRC)) \
	$(patsubst $(NET)/Private/%.cpp,build/bench/net/%.o,$(NET_SRC)) build/bench/net/monocypher.o \
	$(patsubst world/%.cpp,build/bench/world/%.o,$(WORLD_SRC))
BENCH_OBJ := $(BENCH_LIB) build/bench/program/Server.o \
	$(patsubst client/%.cpp,build/bench/client/%.o,$(CLIENT_SRC)) build/bench/RegionBench.o build/bench/NetBench.o \
	build/bench/WorldBench.o build/bench/DayBench.o

bench: build/regionbench build/netbench build/worldbench build/daybench
	./build/regionbench 30 ../core-tests/fixtures/rally-jam.json
	./build/netbench 20
	./build/worldbench 30
	./build/daybench 20

build/daybench: $(BENCH_LIB) build/bench/program/Server.o $(patsubst client/%.cpp,build/bench/client/%.o,$(CLIENT_SRC)) build/bench/DayBench.o
	$(CXX) $(BENCH_FLAGS) -o $@ $^ $(LDLIBS)

build/worldbench: $(BENCH_LIB) build/bench/WorldBench.o
	$(CXX) $(BENCH_FLAGS) -o $@ $^ $(LDLIBS)

build/regionbench: $(BENCH_LIB) build/bench/RegionBench.o
	$(CXX) $(BENCH_FLAGS) -o $@ $^ $(LDLIBS)

build/netbench: $(BENCH_LIB) build/bench/program/Server.o $(patsubst client/%.cpp,build/bench/client/%.o,$(CLIENT_SRC)) build/bench/NetBench.o
	$(CXX) $(BENCH_FLAGS) -o $@ $^ $(LDLIBS)

build/bench/core/%.o: $(CORE)/Private/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(BENCH_FLAGS) -c $< -o $@

build/bench/net/%.o: $(NET)/Private/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(BENCH_FLAGS) -c $< -o $@

build/bench/world/%.o: world/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(BENCH_FLAGS) -c $< -o $@

build/bench/program/%.o: program/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(BENCH_FLAGS) -c $< -o $@

build/bench/client/%.o: client/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(BENCH_FLAGS) -c $< -o $@

build/bench/%.o: bench/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(BENCH_FLAGS) -c $< -o $@

clean:
	rm -rf build

-include $(CORE_OBJ:.o=.d) $(NET_OBJ:.o=.d) $(WORLD_OBJ:.o=.d) $(SERVER_OBJ:.o=.d) $(CLIENT_OBJ:.o=.d) \
	$(TEST_OBJ:.o=.d) $(BENCH_OBJ:.o=.d) build/program/main.d $(REGION_OBJ:.o=.d) build/region/main.d
