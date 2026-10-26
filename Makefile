CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra

# Every .cpp in src/ except main.cpp (tests have their own main)
LIB_SRC  = $(filter-out src/main.cpp, $(wildcard src/*.cpp))

# make        -> build the program
minidb: src/*.cpp src/*.h
	$(CXX) $(CXXFLAGS) -o minidb src/*.cpp

# make test   -> build and run the tests
test: tokenizer_test
	./tokenizer_test

tokenizer_test: tests/tokenizer_test.cpp $(LIB_SRC) src/*.h
	$(CXX) $(CXXFLAGS) -o tokenizer_test tests/tokenizer_test.cpp $(LIB_SRC)

clean:
	rm -f minidb tokenizer_test

.PHONY: test clean
