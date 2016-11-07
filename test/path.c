#include <assert.h>
#include "../myfs_lib.h"

void test_split() {
  char* path1 = strdup("/");
  assert(path_split(&path1) == NULL);
  assert(path1 == NULL);

  char* path2 = strdup("//");
  assert(path_split(&path2) == NULL);
  assert(path2 == NULL);

  char* path3 = strdup("/hello");
  assert(strcmp(path_split(&path3), "hello") == 0);
  assert(path3 == NULL);

  char* path4 = strdup("hello");
  assert(strcmp(path_split(&path4), "hello") == 0);
  assert(path4 == NULL);

  char* path5 = strdup("/hello/world");
  assert(strcmp(path_split(&path5), "hello") == 0);
  assert(strcmp(path5, "world") == 0);

  char* path6 = strdup("");
  assert(path_split(&path6) == NULL);
  assert(path6 == NULL);

  char* path7 = NULL;
  assert(path_split(&path7) == NULL);
}

void test_file_name() {
  char* path1 = strdup("/");
  assert(path_file_name(path1) == NULL);

  char* path2 = strdup("//");
  assert(path_file_name(path2) == NULL);

  char* path3 = strdup("/hello");
  assert(strcmp(path_file_name(path3), "hello") == 0);

  char* path4 = strdup("hello");
  assert(strcmp(path_file_name(path4), "hello") == 0);

  char* path5 = strdup("/hello/world");
  assert(strcmp(path_file_name(path5), "world") == 0);

  char* path6 = strdup("/hello/");
  assert(strcmp(path_file_name(path6), "hello") == 0);

  char* path7 = strdup("/hello/world/");
  assert(strcmp(path_file_name(path7), "world") == 0);

  char* path8 = strdup("");
  assert(path_file_name(path6) == NULL);
}

int main() {
  test_split();
  test_file_name();

  puts("Test passed");

  return 0;
}
