# Builds AutocraftCore (../Source/AutocraftCore), AutocraftNet
# (../Source/AutocraftNet), the server's world (world/), the server program
# (program/, build/autocraft-server) and the headless client (client/) with
# plain clang++, no Unreal.
#
#   make -C unreal/server              build/autocraft-server
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
	-I$(CORE)/Public -I$(CORE)/ThirdParty -I$(NET)/Public -I$(NET)/Private -Iworld -Iprogram -Iclient \
	-I$(CORE_TESTS) -I../net-tests/tests -MMD -MP
# The core's tracking store speaks SQLite's C API (as in core-tests).
LDLIBS := -lsqlite3

CORE_SRC := $(wildcard $(CORE)/Private/*.cpp)
NET_SRC := $(wildcard $(NET)/Private/*.cpp)
WORLD_SRC := $(wildcard world/*.cpp)
CLIENT_SRC := $(wildcard client/*.cpp)
TEST_SRC := $(wildcard tests/*.cpp)
CORE_OBJ := $(patsubst $(CORE)/Private/%.cpp,build/core/%.o,$(CORE_SRC))
NET_OBJ := $(patsubst $(NET)/Private/%.cpp,build/net/%.o,$(NET_SRC))
WORLD_OBJ := $(patsubst world/%.cpp,build/world/%.o,$(WORLD_SRC))
SERVER_OBJ := build/program/Server.o
CLIENT_OBJ := $(patsubst client/%.cpp,build/client/%.o,$(CLIENT_SRC))
TEST_OBJ := $(patsubst tests/%.cpp,build/tests/%.o,$(TEST_SRC)) build/tests/main.o
LIB_OBJ := $(CORE_OBJ) $(NET_OBJ) $(WORLD_OBJ) $(SERVER_OBJ)
PROGRAM := build/autocraft-server
BIN := build/server-tests

.PHONY: all test bench clean
all: $(PROGRAM)

# The end-to-end tests start build/autocraft-server.
test: $(BIN) $(PROGRAM)
	./$(BIN) $(FILTER)

$(PROGRAM): $(LIB_OBJ) build/program/main.o
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)

$(BIN): $(LIB_OBJ) $(CLIENT_OBJ) $(TEST_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)

build/core/%.o: $(CORE)/Private/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

build/net/%.o: $(NET)/Private/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

build/world/%.o: world/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

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

# `make bench`: build/regionbench (bench/RegionBench.cpp) and build/netbench
# (bench/NetBench.cpp), everything at -O3.
BENCH_FLAGS := $(subst -O2,-O3,$(CXXFLAGS)) -I../core-tests/bench
BENCH_LIB := $(patsubst $(CORE)/Private/%.cpp,build/bench/core/%.o,$(CORE_SRC)) \
	$(patsubst $(NET)/Private/%.cpp,build/bench/net/%.o,$(NET_SRC)) \
	$(patsubst world/%.cpp,build/bench/world/%.o,$(WORLD_SRC))
BENCH_OBJ := $(BENCH_LIB) build/bench/program/Server.o \
	$(patsubst client/%.cpp,build/bench/client/%.o,$(CLIENT_SRC)) build/bench/RegionBench.o build/bench/NetBench.o

bench: build/regionbench build/netbench
	./build/regionbench 30 ../core-tests/fixtures/rally-jam.json
	./build/netbench 20

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
	$(TEST_OBJ:.o=.d) $(BENCH_OBJ:.o=.d) build/program/main.d
