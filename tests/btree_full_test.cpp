// This only passes when BOTH Hushay's and Z's parts are done.
// Run with:  make test-full

#include <csignal>
#include <iostream>
#include <unistd.h>
#include <map>
#include <random>
#include <string>
#include "../src/btree.h"

static void onCrash(int) {
    const char msg[] =
        "\n  CRASH  the test right after the last PASS/FAIL line above crashed\n"
        "         (usually a bad pointer, or reading past the end of a vector).\n"
        "         Run `make test-asan` to see the exact line in your code.\n";
    ssize_t ignored = write(1, msg, sizeof(msg) - 1);
    (void)ignored;
    _exit(1);
}

static int passed = 0;
static int failed = 0;

static void pass(const std::string& name) {
    passed++;
    std::cout << "  PASS  " << name << "\n";
}

static void fail(const std::string& name, const std::string& why) {
    failed++;
    std::cout << "  FAIL  " << name << "\n        " << why << "\n";
}

static Value I(int n) { return Value{ValueType::INT, std::to_string(n)}; }
static Row rowFor(int n, int version) {
    return Row{I(n), Value{ValueType::TEXT, "v" + std::to_string(version)}};
}

static void randomTest(const std::string& name, int t, int ops, int keyRange, unsigned seed) {
    BTree tree(t);
    std::map<int, int> model;
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> pickKey(1, keyRange);
    std::uniform_int_distribution<int> pickOp(0, 99);

    for (int step = 0; step < ops; step++) {
        int k = pickKey(rng);
        int op = pickOp(rng);
        std::string what;

        if (op < 45) {
            bool expect = model.find(k) == model.end();
            bool got = tree.insert(I(k), rowFor(k, step));
            if (expect) model[k] = step;
            what = "insert(" + std::to_string(k) + ")";
            if (got != expect) return fail(name, "step " + std::to_string(step) + ": " + what + " returned the wrong true/false");
        } else if (op < 80) {
            bool expect = model.erase(k) == 1;
            bool got = tree.remove(I(k));
            what = "remove(" + std::to_string(k) + ")";
            if (got != expect) return fail(name, "step " + std::to_string(step) + ": " + what + " returned the wrong true/false");
        } else if (op < 90) {
            bool expect = model.find(k) != model.end();
            bool got = tree.update(I(k), rowFor(k, step));
            if (expect) model[k] = step;
            what = "update(" + std::to_string(k) + ")";
            if (got != expect) return fail(name, "step " + std::to_string(step) + ": " + what + " returned the wrong true/false");
        } else {
            const Row* r = tree.find(I(k));
            auto it = model.find(k);
            what = "find(" + std::to_string(k) + ")";
            if ((r != nullptr) != (it != model.end()))
                return fail(name, "step " + std::to_string(step) + ": " + what + " found/missed the wrong thing");
            if (r && (*r)[1].text != "v" + std::to_string(it->second))
                return fail(name, "step " + std::to_string(step) + ": " + what + " returned an old row");
        }

        std::string why;
        if (!tree.checkValid(&why))
            return fail(name, "step " + std::to_string(step) + ", after " + what + ": " + why);
        if (tree.size() != model.size())
            return fail(name, "step " + std::to_string(step) + ", after " + what + ": size() is " +
                              std::to_string(tree.size()) + " but should be " + std::to_string(model.size()));
    }

    // Final full comparison.
    auto all = tree.traverseInOrder();
    if (all.size() != model.size()) return fail(name, "traverseInOrder has the wrong number of pairs at the end");
    auto it = model.begin();
    for (const auto& kv : all) {
        if (std::stoi(kv.first.text) != it->first || kv.second[1].text != "v" + std::to_string(it->second))
            return fail(name, "traverseInOrder does not match at the end (key " + kv.first.text + ")");
        ++it;
    }
    pass(name);
}

int main() {
    std::cout << std::unitbuf;
    std::signal(SIGSEGV, onCrash);
    std::signal(SIGABRT, onCrash);

    std::cout << "\n-- random insert / remove / update / find vs std::map --\n";
    randomTest("t=2, 3000 ops, keys 1-200",   2, 3000, 200, 1);
    randomTest("t=2, 3000 ops, keys 1-50",    2, 3000, 50,  2);
    randomTest("t=3, 5000 ops, keys 1-500",   3, 5000, 500, 3);
    randomTest("t=4, 5000 ops, keys 1-1000",  4, 5000, 1000, 4);

    std::cout << "\n-- fill up then empty out --\n";
    {
        const std::string name = "insert 1-2000 then remove them all (t=3)";
        BTree tree(3);
        bool ok = true;
        for (int k = 1; k <= 2000 && ok; k++) ok = tree.insert(I(k), rowFor(k, 0));
        for (int k = 1; k <= 2000 && ok; k += 2) ok = tree.remove(I(k));
        for (int k = 2000; k >= 2 && ok; k -= 2) ok = tree.remove(I(k));
        std::string why;
        if (!ok)                                   fail(name, "an insert or remove returned false");
        else if (!tree.checkValid(&why))           fail(name, why);
        else if (tree.size() != 0 || tree.getRoot()) fail(name, "tree should be empty with root == nullptr");
        else                                       pass(name);
    }

    std::cout << "\n" << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}
