CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra

LIB_SRC  = $(filter-out src/main.cpp, $(wildcard src/*.cpp))
HEADERS  = $(wildcard src/*.h)

minidb: src/*.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -o minidb src/*.cpp

test: tokenizer_test parser_test
	./tokenizer_test
	./parser_test

tokenizer_test: tests/tokenizer_test.cpp $(LIB_SRC) $(HEADERS)
	$(CXX) $(CXXFLAGS) -o tokenizer_test tests/tokenizer_test.cpp $(LIB_SRC)

parser_test: tests/parser_test.cpp $(LIB_SRC) $(HEADERS)
	$(CXX) $(CXXFLAGS) -o parser_test tests/parser_test.cpp $(LIB_SRC)

clean:
	rm -f minidb tokenizer_test parser_test

.PHONY: test clean