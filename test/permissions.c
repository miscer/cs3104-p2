#include <assert.h>
#include "../myfs_lib.h"

void test_read() {
  struct my_fcb file_fcb = {
    .mode = 0,
    .uid = 1,
    .gid = 1,
  };

  struct my_user user1 = {1, 1};
  struct my_user user2 = {1, 2};
  struct my_user user3 = {2, 1};
  struct my_user user4 = {2, 2};

  assert(!can_read(&file_fcb, user1));
  assert(!can_read(&file_fcb, user2));
  assert(!can_read(&file_fcb, user3));
  assert(!can_read(&file_fcb, user4));

  file_fcb.mode = S_IRUSR;
  assert(can_read(&file_fcb, user1));
  assert(can_read(&file_fcb, user2));
  assert(!can_read(&file_fcb, user3));
  assert(!can_read(&file_fcb, user4));

  file_fcb.mode = S_IRGRP;
  assert(!can_read(&file_fcb, user1));
  assert(!can_read(&file_fcb, user2));
  assert(can_read(&file_fcb, user3));
  assert(!can_read(&file_fcb, user4));

  file_fcb.mode = S_IROTH;
  assert(!can_read(&file_fcb, user1));
  assert(!can_read(&file_fcb, user2));
  assert(!can_read(&file_fcb, user3));
  assert(can_read(&file_fcb, user4));
}

int main() {
  test_read();

  puts("Test passed");

  return 0;
}
