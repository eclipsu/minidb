CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra

LIB_SRC  = $(filter-out src/main.cpp, $(wildcard src/*.cpp))
HEADERS  = $(wildcard src/*.h)

TESTS    = tokenizer_test parser_test btree_insert_test btree_remove_test btree_full_test

minidb: src/*.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -o minidb src/*.cpp

test: $(TESTS)
	./tokenizer_test
	./parser_test
	./btree_insert_test
	./btree_remove_test
	./btree_full_test

# Run just one group:
test-parser: tokenizer_test parser_test
	./tokenizer_test
	./parser_test

test-insert: btree_insert_test     # Hushay
	./btree_insert_test

test-remove: btree_remove_test     # Z
	./btree_remove_test

test-full: btree_full_test         # needs both insert and remove done
	./btree_full_test

test-asan: clean
	$(MAKE) test CXXFLAGS="$(CXXFLAGS) -g -fsanitize=address,undefined -fno-omit-frame-pointer"

$(TESTS): %: tests/%.cpp $(LIB_SRC) $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $@ tests/$@.cpp $(LIB_SRC)

clean:
	rm -f minidb $(TESTS)

.PHONY: test test-parser test-insert test-remove test-full test-asan clean