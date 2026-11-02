CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -MMD -MP

SRC      = $(wildcard src/*.cpp)
LIB_OBJ  = $(patsubst src/%.cpp, build/%.o, $(filter-out src/main.cpp, $(SRC)))
TESTS    = tokenizer_test parser_test

minidb: build/main.o $(LIB_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

test: $(TESTS)
	@for t in $(TESTS); do ./$$t || exit 1; done

%_test: build/%_test.o $(LIB_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

build/%.o: src/%.cpp | build
	$(CXX) $(CXXFLAGS) -c $< -o $@

build/%.o: tests/%.cpp | build
	$(CXX) $(CXXFLAGS) -c $< -o $@

build:
	mkdir -p build

clean:
	rm -rf build minidb $(TESTS)

-include $(wildcard build/*.d)

.PHONY: test clean
.SECONDARY: