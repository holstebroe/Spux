// Golden hash helpers: FNV-1a vectors, bit-pattern hashing of floats, and the golden file workflow.
#include <doctest.h>

#include <filesystem>
#include <fstream>

#include "spux/testing/Hash.hpp"

using namespace spux::testing;

TEST_CASE("FNV-1a 64 matches the published test vectors") {
    CHECK(hashBytes("", 0) == 0xcbf29ce484222325ull);
    CHECK(hashBytes("a", 1) == 0xaf63dc4c8601ec8cull);
    CHECK(hashBytes("foobar", 6) == 0x85944171f73967e8ull);
}

TEST_CASE("words and floats are hashed by their little-endian bytes") {
    const std::uint32_t w = 0x04030201u;
    const unsigned char bytes[4] = {1, 2, 3, 4};
    CHECK(hashWords({w}) == hashBytes(bytes, 4));
    CHECK(hashFloats({1.0f}) == hashWords({0x3f800000u}));
    CHECK(hashFloats({0.0f}) != hashFloats({-0.0f})); // a sign of zero is a difference
    Fnv1a64 h;
    h.real(1.0f);
    h.word(5u);
    CHECK(h.value() == hashWords({0x3f800000u, 5u}));
}

TEST_CASE("hashToHex and platformTag") {
    CHECK(hashToHex(0xaf63dc4c8601ec8cull) == "af63dc4c8601ec8c");
    CHECK(hashToHex(1) == "0000000000000001");
    const std::string tag = platformTag();
    CHECK(tag.find('-') != std::string::npos);
    CHECK(goldenPath("g") == "g/" + tag + ".txt");
}

TEST_CASE("a golden file: missing, update, match, mismatch") {
    const auto dir = std::filesystem::temp_directory_path() / "spux_hash_test";
    std::filesystem::create_directories(dir);
    const std::string path = (dir / "g.txt").string();
    std::filesystem::remove(path);
    using R = GoldenFile::Result;

    GoldenFile fresh(path, false);
    CHECK(fresh.verify("kick", 0x1234) == R::Missing);
    CHECK_FALSE(fresh.describe("kick", 0x1234, R::Missing).empty());

    GoldenFile writer(path, true);
    CHECK(writer.updating());
    CHECK(writer.verify("kick", 0x1234) == R::Updated);
    CHECK(writer.verify("bass", 0xabcdef) == R::Updated);
    REQUIRE(writer.save());

    GoldenFile reader(path, false);
    CHECK(reader.verify("kick", 0x1234) == R::Match);
    CHECK(reader.verify("bass", 0xabcdef) == R::Match);
    CHECK(reader.verify("kick", 0x9999) == R::Mismatch);
    CHECK(reader.describe("kick", 0x9999, R::Mismatch).find("0000000000001234") != std::string::npos);
    CHECK(reader.verify("snare", 1) == R::Missing);
    CHECK(reader.find("bass") != nullptr);

    // comments and blank lines are skipped, and an unchanged update writes nothing new
    std::ofstream(path, std::ios::app) << "\n# note\n";
    GoldenFile again(path, true);
    again.verify("kick", 0x1234);
    CHECK(again.save());
    GoldenFile check(path, false);
    CHECK(check.verify("kick", 0x1234) == R::Match);
    std::filesystem::remove_all(dir);
}
