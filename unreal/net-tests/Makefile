# Builds AutocraftNet (../Source/AutocraftNet) and its tests with plain
# clang++, no Unreal, and runs them: `make -C unreal/net-tests`.
# `make -C unreal/net-tests FILTER=bulk` runs only tests whose name contains
# "bulk". The test framework is core-tests' (test.h, main.cpp). Objects go
# to build/. The protocol's files (NetProtocol, NetSnapshot, NetMirror) speak
# the core's types, so AutocraftCore is built alongside.

CXX := clang++
NET := ../Source/AutocraftNet
CORE := ../Source/AutocraftCore
FRAMEWORK := ../core-tests/tests
CXXFLAGS := -std=c++20 -O2 -Wall -Wextra -Wshadow -Werror -ffp-contract=off -fno-exceptions -fno-rtti \
	-I$(NET)/Public -I$(NET)/Private -I$(CORE)/Public -I$(CORE)/ThirdParty -I$(FRAMEWORK) -Itests -MMD -MP

# The core's tracking store speaks SQLite's C API (as in core-tests).
LDLIBS := -lsqlite3

NET_SRC := $(wildcard $(NET)/Private/*.cpp)
CORE_SRC := $(wildcard $(CORE)/Private/*.cpp)
CORE_OBJ := $(patsubst $(CORE)/Private/%.cpp,build/core/%.o,$(CORE_SRC))
TEST_SRC := $(wildcard tests/*.cpp)
NET_OBJ := $(patsubst $(NET)/Private/%.cpp,build/net/%.o,$(NET_SRC)) build/net/monocypher.o
TEST_OBJ := $(patsubst tests/%.cpp,build/tests/%.o,$(TEST_SRC)) build/tests/main.o
BIN := build/net-tests

.PHONY: all run clean
all: run

run: $(BIN)
	./$(BIN) $(FILTER)

$(BIN): $(CORE_OBJ) $(NET_OBJ) $(TEST_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)

build/core/%.o: $(CORE)/Private/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

build/net/%.o: $(NET)/Private/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Monocypher (ThirdParty/monocypher), C as it comes.
build/net/monocypher.o: $(NET)/ThirdParty/monocypher/monocypher.c $(NET)/ThirdParty/monocypher/monocypher.h
	@mkdir -p $(dir $@)
	clang -std=c99 -O2 -c $< -o $@

build/tests/main.o: $(FRAMEWORK)/main.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

build/tests/%.o: tests/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf build

-include $(CORE_OBJ:.o=.d) $(NET_OBJ:.o=.d) $(TEST_OBJ:.o=.d)
