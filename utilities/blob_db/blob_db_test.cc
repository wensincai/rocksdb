// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef ROCKSDB_LITE

#include "utilities/blob_db/blob_db.h"
#include "util/random.h"
#include "util/testharness.h"
#include "util/testutil.h"
#include <cstdlib>

namespace rocksdb {
void gen_random(char *s, const int len) {
    static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";

    for (int i = 0; i < len; ++i) {
        s[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
    }

    s[len] = 0;
}

class BlobDBTest : public testing::Test {
 public:
  BlobDBTest() : db_(nullptr) {
    dbname_ = test::TmpDir() + "/blob_db_test";
    Reopen(BlobDBOptions());
  }

  ~BlobDBTest() {
    if (db_) {
      delete db_;
      db_ = nullptr;
    }
  }

  void Reopen(const BlobDBOptions &bdboptions) {
    if (db_) {
      delete db_;
      db_ = nullptr;
    }

    BlobDBOptions bdb_options = bdboptions;
    bdb_options.blob_dir = "blob_dir";
    Options options;
    BlobDB::DestroyBlobDB(dbname_, options,
      bdb_options);

    DestroyDB(dbname_, options);

    options.create_if_missing = true;
    EXPECT_TRUE(BlobDB::Open(options, bdb_options, dbname_, &db_).ok());
  }

  void insert_blobs() {
    WriteOptions wo;
    ReadOptions ro;
    std::string value;

    ColumnFamilyHandle* dcfh = db_->DefaultColumnFamily();

    for (size_t i = 0; i < 100000; i++) {
      int len = rand() % 16384;
      if (!len)
        continue;

      char *val = new char[len+1];
      gen_random(val, len);

      std::string key("key");
      key += std::to_string(i % 500);

      Slice keyslice(key);
      Slice valslice(val, len+1);

      int ttl = rand() % 86400;

      ASSERT_OK(db_->PutWithTTL(wo, dcfh, keyslice, valslice, ttl));
      delete [] val;
    }

    for (size_t i = 0; i < 10; i++) {
      std::string key("key");
      key += std::to_string(i % 500);
      Slice keyslice(key);
      db_->Delete(wo, dcfh, keyslice);
    }
  }

  BlobDB* db_;
  std::string dbname_;
};  // class BlobDBTest

TEST_F(BlobDBTest, GCTest) {

  BlobDBOptions bdboptions;
  bdboptions.ttl_range = 30;
  bdboptions.gc_file_pct = 100;

  Reopen(bdboptions);

  WriteOptions wo;
  ReadOptions ro;
  std::string value;

  ColumnFamilyHandle* dcfh = db_->DefaultColumnFamily();

  for (size_t i = 0; i < 100; i++) {
    int len = rand() % 16384;
    if (!len)
      continue;

    char *val = new char[len+1];
    gen_random(val, len);

    std::string key("key");
    key += std::to_string(i);

    Slice keyslice(key);
    Slice valslice(val, len+1);

    int ttl = 30;

    ASSERT_OK(db_->PutWithTTL(wo, dcfh, keyslice, valslice, ttl));
    delete [] val;
  }

  sleep(240);
}

TEST_F(BlobDBTest, MultipleWriters) {
  ASSERT_TRUE(db_ != nullptr);

  std::vector<std::thread> workers;
  for (size_t ii = 0; ii < 10; ii++)
    workers.push_back(std::thread(&BlobDBTest::insert_blobs, this));

  for (std::thread &t: workers) {
    if (t.joinable()) {
      t.join();
    }
  }

  sleep(180);
  //ASSERT_OK(db_->PutWithTTL(wo, dcfh, "bar", "v2", 60));
  //ASSERT_OK(db_->Get(ro, dcfh, "foo", &value));
  //ASSERT_EQ("v1", value);
  //ASSERT_OK(db_->Get(ro, dcfh, "bar", &value));
  //ASSERT_EQ("v2", value);
}

#if 0
TEST_F(BlobDBTest, Large) {
  ASSERT_TRUE(db_ != nullptr);

  WriteOptions wo;
  ReadOptions ro;
  std::string value1, value2, value3;
  Random rnd(301);
  ColumnFamilyHandle* dcfh = db_->DefaultColumnFamily();

  value1.assign(8999, '1');
  ASSERT_OK(db_->PutWithTTL(wo, dcfh, "foo", value1, 3600));
  value2.assign(9001, '2');
  ASSERT_OK(db_->PutWithTTL(wo, dcfh, "bar", value2, 3600));
  test::RandomString(&rnd, 13333, &value3);
  ASSERT_OK(db_->PutWithTTL(wo, dcfh, "barfoo", value3, 3600));

  std::string value;
  ASSERT_OK(db_->Get(ro, dcfh, "foo", &value));
  ASSERT_EQ(value1, value);
  ASSERT_OK(db_->Get(ro, dcfh, "bar", &value));
  ASSERT_EQ(value2, value);
  ASSERT_OK(db_->Get(ro, dcfh, "barfoo", &value));
  ASSERT_EQ(value3, value);
}
#endif

}  //  namespace rocksdb

// A black-box test for the ttl wrapper around rocksdb
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

#else
#include <stdio.h>

int main(int argc, char** argv) {
  fprintf(stderr, "SKIPPED as BlobDB is not supported in ROCKSDB_LITE\n");
  return 0;
}

#endif  // !ROCKSDB_LITE
