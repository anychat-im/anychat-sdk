#include "file_manager.h"

#include "network/http_client.h"

#include <memory>
#include <string>

#include <gtest/gtest.h>

// ===========================================================================
// Fixture
// ===========================================================================
class FileManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        http_ = std::make_shared<anychat::network::HttpClient>("http://localhost:19999");
        mgr_ = std::make_unique<anychat::FileManagerImpl>(http_);
    }

    void TearDown() override {
        mgr_.reset();
        http_.reset();
    }

    std::shared_ptr<anychat::network::HttpClient> http_;
    std::unique_ptr<anychat::FileManagerImpl> mgr_;
};

// ---------------------------------------------------------------------------
// 1. UploadNonExistentFileReportsError
//    upload() with a path that doesn't exist should invoke the on_done
//    callback with ok=false; it must not crash.
// ---------------------------------------------------------------------------
TEST_F(FileManagerTest, UploadNonExistentFileReportsError) {
    bool cb_called = false;
    bool ok_flag = true;

    mgr_->upload(
        "/nonexistent/path/file.bin",
        "file",
        nullptr, // no progress callback
        [&](bool ok, const anychat::FileInfo& /*info*/, const std::string& /*err*/) {
            cb_called = true;
            ok_flag = ok;
        }
    );

    EXPECT_TRUE(cb_called) << "on_done callback should be called";
    EXPECT_FALSE(ok_flag) << "Upload of a non-existent file should fail";
}

// ---------------------------------------------------------------------------
// 2. GetDownloadUrlDoesNotCrash
//    getDownloadUrl() fires an HTTP GET; no server means the callback gets
//    an error response, but no crash should occur.
// ---------------------------------------------------------------------------
TEST_F(FileManagerTest, GetDownloadUrlDoesNotCrash) {
    EXPECT_NO_THROW(mgr_->getDownloadUrl("file-id-999", [](bool /*ok*/, std::string /*url*/, std::string /*err*/) {}));
}

// ---------------------------------------------------------------------------
// 3. GetFileInfoDoesNotCrash
// ---------------------------------------------------------------------------
TEST_F(FileManagerTest, GetFileInfoDoesNotCrash) {
    EXPECT_NO_THROW(mgr_->getFileInfo("file-id-999", [](bool /*ok*/, anychat::FileInfo /*info*/, std::string /*err*/) {}));
}

// ---------------------------------------------------------------------------
// 4. ListFilesDoesNotCrash
// ---------------------------------------------------------------------------
TEST_F(FileManagerTest, ListFilesDoesNotCrash) {
    EXPECT_NO_THROW(mgr_->listFiles("", 1, 20, [](std::vector<anychat::FileInfo> /*files*/, int64_t /*total*/, std::string /*err*/) {}));
}

// ---------------------------------------------------------------------------
// 5. UploadClientLogNonExistentFileReportsError
// ---------------------------------------------------------------------------
TEST_F(FileManagerTest, UploadClientLogNonExistentFileReportsError) {
    bool cb_called = false;
    bool ok_flag = true;

    mgr_->uploadClientLog(
        "/nonexistent/path/client.log",
        nullptr,
        [&](bool ok, const anychat::FileInfo& /*info*/, const std::string& /*err*/) {
            cb_called = true;
            ok_flag = ok;
        },
        24
    );

    EXPECT_TRUE(cb_called) << "on_done callback should be called";
    EXPECT_FALSE(ok_flag) << "Upload of a non-existent log file should fail";
}

// ---------------------------------------------------------------------------
// 6. DeleteFileDoesNotCrash
// ---------------------------------------------------------------------------
TEST_F(FileManagerTest, DeleteFileDoesNotCrash) {
    EXPECT_NO_THROW(mgr_->deleteFile("file-id-999", [](bool /*ok*/, const std::string& /*err*/) {}));
}
