// Tests for the shachain mutator: its derivation against the BOLT3 test
// vectors, that it reads inputs as the driver does, and that it keeps chains
// consistent except where it corrupts them.
#include "../mutators/shachain.h"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

static size_t raw_mutations = 0;

// Stands in for libFuzzer; appends an unknown tag so its output is not ops.
extern "C" size_t LLVMFuzzerMutate(uint8_t *data, size_t size,
                                   size_t max_size) {
  ++raw_mutations;
  if (size < max_size)
    data[size++] = shachain_mutator::GENERATE + 1;
  return size;
}

namespace {
using namespace shachain_mutator;

Secret SecretFromHex(std::string_view hex) {
  assert(hex.size() == 2 * SECRET_SIZE);
  Secret s;
  for (size_t i = 0; i < SECRET_SIZE; ++i)
    s[i] = std::stoi(std::string(hex.substr(2 * i, 2)), nullptr, 16);
  return s;
}

// BOLT3 Appendix D generation tests, at bolts commit 0ce484d.
void TestGenerationVectors() {
  const Secret zeros{};
  Secret ones;
  ones.fill(0xff);
  Secret alternating;
  alternating.fill(0x01);
  assert(Derive(zeros, 0, 281474976710655) ==
         SecretFromHex("02a40c85b6f28da08dfdbe0926c53fab"
                       "2de6d28c10301f8f7c4073d5e42e3148"));
  assert(Derive(ones, 0, 281474976710655) ==
         SecretFromHex("7cc854b54e3e0dcdb010d7a3fee464a9"
                       "687be6e8db3be6854c475621e007a5dc"));
  assert(Derive(ones, 0, 0xaaaaaaaaaaa) ==
         SecretFromHex("56f4008fb007ca9acf0e15b054d5c9fd"
                       "12ee06cea347914ddbaed70d1c13a528"));
  assert(Derive(ones, 0, 0x555555555555) ==
         SecretFromHex("9015daaeb06dba4ccc05b91b2f73bd54"
                       "405f2be9f217fbacd3c5ac2e62327d31"));
  assert(Derive(alternating, 0, 1) ==
         SecretFromHex("915c75942a26bb3a433a8ce2cb0427c2"
                       "9ec6c1775cfc78328b57f6ba7bfeaa9c"));
}

// The examples from BOLT3 "Efficient Per-commitment Secret Storage".
void TestDerivable() {
  assert(Derivable(0xFFFFFFFFFFF0, 0xFFFFFFFFFFF0));
  assert(Derivable(0xFFFFFFFFFFF0, 0xFFFFFFFFFFF1));
  assert(Derivable(0xFFFFFFFFFFF0, 0xFFFFFFFFFFFF));
  assert(!Derivable(0xFFFFFFFFFFF0, 0xFFFFFFFFFFEF));
  assert(Derivable(0xFFFFFFFFFF08, 0xFFFFFFFFFF0F));
  assert(!Derivable(0xFFFFFFFFFF08, 0xFFFFFFFFFF07));
  assert(!Derivable(0xFFFFFFFFFF08, 0xFFFFFFFFFF10));
  assert(Derivable(0, 0x123456789ABC));
}

void TestParseStopsLikeTheDriver() {
  // generate, restore, lookup, insert, then an unknown tag with room for any
  // payload.
  std::vector<uint8_t> data{GENERATE};
  data.insert(data.end(), SECRET_SIZE, 0x11);
  data.insert(data.end(), {0, 0, 0, 0, 0, 7});
  data.push_back(RESTORE);
  data.insert(data.end(), {LOOKUP, 0, 0, 0, 0, 1, 2});
  data.push_back(INSERT);
  data.insert(data.end(), SECRET_SIZE, 0x22);
  const size_t whole = data.size();
  data.push_back(0x04);
  data.insert(data.end(), OpSize(GENERATE), 0);

  const std::vector<Op> ops = Parse(data.data(), data.size());
  assert(ops.size() == 4);
  assert(ops[0].tag == GENERATE && ops[0].secret[0] == 0x11 &&
         ops[0].index == 7);
  assert(ops[1].tag == RESTORE);
  assert(ops[2].tag == LOOKUP && ops[2].index == 0x0102);
  assert(ops[3].tag == INSERT && ops[3].secret[31] == 0x22);

  std::vector<uint8_t> out(whole);
  assert(Serialize(ops, out.data(), out.size()) == whole);
  assert(std::equal(out.begin(), out.end(), data.begin()));

  // A truncated payload ends the ops too, and Serialize drops the ops that do
  // not fit.
  assert(Parse(data.data(), whole - 1).size() == 3);
  assert(Serialize(ops, out.data(), whole - 1) == whole - OpSize(INSERT));
}

// Whether every insert derives each earlier one it should, assuming all are
// accepted.
bool Consistent(const std::vector<Op> &ops) {
  std::vector<Secret> secrets;
  for (const Op &op : ops)
    if (op.tag == INSERT)
      secrets.push_back(op.secret);
  for (size_t a = 0; a < secrets.size(); ++a)
    for (size_t b = a + 1; b < secrets.size(); ++b)
      if (Derivable(MAX_INDEX - b, MAX_INDEX - a) &&
          Derive(secrets[b], MAX_INDEX - b, MAX_INDEX - a) != secrets[a])
        return false;
  return true;
}

void TestAppendKeepsTheChainConsistent() {
  std::mt19937_64 rng(1);
  std::vector<Op> ops;
  while (ops.size() < 70) {
    Mutate(ops, APPEND_INSERTS, rng);
    Mutate(ops, ADD_LOOKUP, rng);
    assert(Consistent(ops));
  }
}

void TestRerootIsCaughtByTheParent() {
  std::mt19937_64 rng(2);
  std::vector<Op> ops;
  while (ops.size() < 4)
    Mutate(ops, APPEND_INSERTS, rng);
  ops.resize(4);
  const Secret parent = ops[3].secret;

  // The second insert (index ...FE) derives the first; the fourth (...FC)
  // derives both.
  Reroot(ops, 1, rng);
  assert(Derive(ops[1].secret, MAX_INDEX - 1, MAX_INDEX) == ops[0].secret);
  assert(ops[3].secret == parent);
  assert(!Consistent(ops));
  ops.resize(3);
  assert(Consistent(ops));
}

void TestMutatorOutput() {
  constexpr size_t MAX_SIZE = 4096;
  std::vector<uint8_t> data(MAX_SIZE);
  size_t size = 0;
  size_t consistent = 0;
  size_t corrupted = 0;
  size_t without_restore = 0;
  size_t raw = 0;
  uint64_t longest = 0;
  bool seen[GENERATE + 1] = {};
  for (unsigned int seed = 0; seed < 3000; ++seed) {
    const size_t raw_before = raw_mutations;
    size = LLVMFuzzerCustomMutator(data.data(), size, MAX_SIZE, seed);
    assert(size <= MAX_SIZE);
    if (raw_mutations != raw_before) {
      ++raw;
      continue;
    }
    const std::vector<Op> ops = Parse(data.data(), size);
    std::vector<uint8_t> again(MAX_SIZE);
    assert(Serialize(ops, again.data(), MAX_SIZE) == size);

    for (const Op &op : ops)
      seen[op.tag] = true;
    without_restore += std::none_of(
        ops.begin(), ops.end(), [](const Op &op) { return op.tag == RESTORE; });
    const uint64_t inserts = InsertsBefore(ops, ops.size());
    longest = std::max(longest, inserts);
    if (inserts > 1)
      ++(Consistent(ops) ? consistent : corrupted);
  }
  assert(seen[INSERT] && seen[LOOKUP] && seen[RESTORE] && seen[GENERATE]);
  assert(consistent > 0 && corrupted > 0);
  assert(longest >= 32);
  assert(without_restore >= 1000);
  assert(raw >= 3000 / 8 && raw <= 3000 / 2);
}
} // namespace

int main() {
  TestGenerationVectors();
  TestDerivable();
  TestParseStopsLikeTheDriver();
  TestAppendKeepsTheChainConsistent();
  TestRerootIsCaughtByTheParent();
  TestMutatorOutput();
  std::puts("test_shachain: OK");
  return 0;
}
