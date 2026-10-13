# Builds AutocraftNet (../Source/AutocraftNet) and its tests with plain
# clang++, no Unreal, and runs them: `make -C unreal/net-tests`.
# `make -C unreal/net-tests FILTER=bulk` runs only tests whose name contains
# "bulk". The test framework is core-tests' (test.h, main.cpp). Objects go
# to build/.

CXX := clang++
NET := ../Source/AutocraftNet
FRAMEWORK := ../core-tests/tests
CXXFLAGS := -std=c++20 -O2 -Wall -Wextra -Wshadow -Werror -ffp-contract=off -fno-exceptions -fno-rtti \
	-I$(NET)/Public -I$(NET)/Private -I$(FRAMEWORK) -Itests -MMD -MP

NET_SRC := $(wildcard $(NET)/Private/*.cpp)
TEST_SRC := $(wildcard tests/*.cpp)
NET_OBJ := $(patsubst $(NET)/Private/%.cpp,build/net/%.o,$(NET_SRC))
TEST_OBJ := $(patsubst tests/%.cpp,build/tests/%.o,$(TEST_SRC)) build/tests/main.o
BIN := build/net-tests

.PHONY: all run clean
all: run

run: $(BIN)
	./$(BIN) $(FILTER)

$(BIN): $(NET_OBJ) $(TEST_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

build/net/%.o: $(NET)/Private/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

build/tests/main.o: $(FRAMEWORK)/main.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

build/tests/%.o: tests/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf build

-include $(NET_OBJ:.o=.d) $(TEST_OBJ:.o=.d)
