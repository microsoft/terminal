#include <gtest/gtest.h>
#include <string>
#include <vector>
#include "src/buffer/out/Row.cpp"

class SecurityTest : public ::testing::TestWithParam<std::string> {};

TEST_P(SecurityTest, BufferReadsNeverExceedDeclaredLength) {
    // Invariant: Buffer reads never exceed the declared length
    std::string payload = GetParam();
    
    // Create a Row object with a fixed buffer size
    Row row;
    row._chars.resize(10);  // Small fixed buffer
    
    // Convert payload to wchar_t for Row processing
    std::wstring wpayload(payload.begin(), payload.end());
    
    // This should either truncate or reject without buffer overflow
    // We're testing that the actual Row.cpp implementation handles this safely
    row.insertCharacters(0, wpayload);
    
    // If we get here without crashing, the invariant holds
    // Additional check: verify buffer wasn't corrupted
    ASSERT_LE(row._chars.size(), 10);
}

INSTANTIATE_TEST_SUITE_P(
    AdversarialInputs,
    SecurityTest,
    ::testing::Values(
        // Exact exploit case: significantly exceeds buffer
        std::string(100, 'A'),
        // Boundary case: exactly at buffer limit
        std::string(10, 'B'),
        // Valid input: well within buffer
        std::string(5, 'C'),
        // Another adversarial case: 2x buffer size
        std::string(20, 'D')
    )
);

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}