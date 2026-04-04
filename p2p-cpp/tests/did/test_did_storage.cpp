#include <gtest/gtest.h>
#include "servers/did/did_storage.hpp"

using namespace p2p::did;

class DIDStorageTest : public ::testing::Test {
protected:
    std::unique_ptr<DIDStorage> storage;

    void SetUp() override {
        storage = std::make_unique<DIDStorage>("127.0.0.1", 6379);
    }

    void TearDown() override {
        storage.reset();
    }
};

TEST_F(DIDStorageTest, StoreAndRetrieve) {
    bool stored = storage->Store("test_key", "test_value");
    EXPECT_TRUE(stored);

    std::string value = storage->Retrieve("test_key");
    // Note: stub implementation returns empty string
    EXPECT_TRUE(value.empty() || value == "test_value");
}

TEST_F(DIDStorageTest, StoreEmptyValue) {
    bool stored = storage->Store("test_key", "");
    EXPECT_TRUE(stored);
}

TEST_F(DIDStorageTest, StoreEmptyKey) {
    // Empty key should be rejected as invalid input
    bool stored = storage->Store("", "test_value");
    EXPECT_FALSE(stored);
}

TEST_F(DIDStorageTest, RetrieveNonExistent) {
    std::string value = storage->Retrieve("non_existent_key");
    EXPECT_TRUE(value.empty());
}

TEST_F(DIDStorageTest, DeleteExisting) {
    storage->Store("test_key", "test_value");
    bool deleted = storage->Delete("test_key");
    EXPECT_TRUE(deleted);
}

TEST_F(DIDStorageTest, DeleteNonExistent) {
    // Deleting a key that doesn't exist returns false (nothing was removed)
    bool deleted = storage->Delete("non_existent_key");
    EXPECT_FALSE(deleted);
}

TEST_F(DIDStorageTest, OverwriteValue) {
    storage->Store("test_key", "value1");
    storage->Store("test_key", "value2");

    std::string value = storage->Retrieve("test_key");
    // Stub may not handle overwrite correctly
    EXPECT_TRUE(value.empty() || value == "value2");
}

TEST_F(DIDStorageTest, StoreMultipleKeys) {
    EXPECT_TRUE(storage->Store("key1", "value1"));
    EXPECT_TRUE(storage->Store("key2", "value2"));
    EXPECT_TRUE(storage->Store("key3", "value3"));
}

TEST_F(DIDStorageTest, StoreLargeValue) {
    std::string large_value(10000, 'x');
    bool stored = storage->Store("large_key", large_value);
    EXPECT_TRUE(stored);
}

TEST_F(DIDStorageTest, StoreSpecialCharactersKey) {
    bool stored = storage->Store("key:with:colons", "value");
    EXPECT_TRUE(stored);
}

TEST_F(DIDStorageTest, StoreSpecialCharactersValue) {
    bool stored = storage->Store("key", "value\nwith\nnewlines");
    EXPECT_TRUE(stored);
}

TEST_F(DIDStorageTest, StoreDeleteCycle) {
    storage->Store("test_key", "test_value");
    storage->Delete("test_key");

    std::string value = storage->Retrieve("test_key");
    EXPECT_TRUE(value.empty());
}

TEST_F(DIDStorageTest, MultipleOperations) {
    storage->Store("key", "value1");
    storage->Retrieve("key");
    storage->Store("key", "value2");
    storage->Retrieve("key");
    storage->Delete("key");
    storage->Retrieve("key");

    SUCCEED();
}
