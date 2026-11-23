CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra

LIB_SRC  = $(filter-out src/main.cpp, $(wildcard src/*.cpp))
HEADERS  = $(wildcard src/*.h)

<<<<<<< HEAD
TESTS    = tokenizer_test parser_test btree_insert_test btree_remove_test btree_full_test storage_test
=======
TESTS    = tokenizer_test parser_test btree_insert_test btree_remove_test btree_full_test
>>>>>>> 496ceba8f20c1b93290a49b9f2979e33b742bd2a

minidb: src/*.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -o minidb src/*.cpp

test: $(TESTS)
	./tokenizer_test
	./parser_test
	./btree_insert_test
	./btree_remove_test
	./btree_full_test
<<<<<<< HEAD
	./storage_test
=======
>>>>>>> 496ceba8f20c1b93290a49b9f2979e33b742bd2a

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

<<<<<<< HEAD
test-storage: storage_test         # Dennison
	./storage_test

# make test-asan                -> every test
# make test-asan T=test-storage -> just one group
test-asan: clean
	$(MAKE) $(or $(T),test) CXXFLAGS="$(CXXFLAGS) -g -fsanitize=address,undefined -fno-omit-frame-pointer"
=======
test-asan: clean
	$(MAKE) test CXXFLAGS="$(CXXFLAGS) -g -fsanitize=address,undefined -fno-omit-frame-pointer"
>>>>>>> 496ceba8f20c1b93290a49b9f2979e33b742bd2a

$(TESTS): %: tests/%.cpp $(LIB_SRC) $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $@ tests/$@.cpp $(LIB_SRC)

clean:
	rm -f minidb $(TESTS)
<<<<<<< HEAD
	rm -rf storage_test_tmp

.PHONY: test test-parser test-insert test-remove test-full test-storage test-asan clean
=======

.PHONY: test test-parser test-insert test-remove test-full test-asan clean
>>>>>>> 496ceba8f20c1b93290a49b9f2979e33b742bd2a
