#include "myfs.h"

struct my_dir_iter {
  int position;
  void* dir_data;
};

void create_file(mode_t, struct my_fcb*);
void create_directory(mode_t, struct my_fcb*);
void read_file(uuid_t*, struct my_fcb*);
int find_file(const char*, struct my_fcb*);
int find_dir_entry(const char*, struct my_fcb*, struct my_fcb*);
void update_file(struct my_fcb);
void read_file_data(struct my_fcb, void*);
void write_file_data(struct my_fcb*, void*, size_t);
void add_dir_entry(struct my_fcb*, struct my_fcb*, const char*);
void remove_dir_entry(struct my_fcb*, struct my_fcb*);
void iterate_dir_entries(struct my_fcb*, struct my_dir_iter*);
struct my_dir_entry* next_dir_entry(struct my_dir_iter*);
char* path_file_name(const char*);
