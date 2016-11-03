/*
  MyFS. One directory, one file, 1000 bytes of storage. What more do you need?

  This Fuse file system is based largely on the HelloWorld example by Miklos Szeredi <miklos@szeredi.hu> (http://fuse.sourceforge.net/helloworld.html). Additional inspiration was taken from Joseph J. Pfeiffer's "Writing a FUSE Filesystem: a Tutorial" (http://www.cs.nmsu.edu/~pfeiffer/fuse-tutorial/).
*/

#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <errno.h>
#include <fcntl.h>

#include "myfs_lib.h"

// Get file and directory attributes (meta-data).
// Read 'man 2 stat' and 'man 2 chmod'.
static int myfs_getattr(const char *path, struct stat *stbuf){
	write_log("myfs_getattr(path=\"%s\", statbuf=0x%08x)\n", path, stbuf);

  struct my_fcb file_fcb;

  if (find_file(path, &file_fcb) != MYFS_FIND_FOUND) {
    write_log("myfs_getattr - ENOENT");
    return -ENOENT;
  }

  memset(stbuf, 0, sizeof(struct stat));

  stbuf->st_mode = file_fcb.mode;
  stbuf->st_nlink = file_fcb.nlink;
  stbuf->st_uid = file_fcb.uid;
  stbuf->st_gid = file_fcb.gid;
  stbuf->st_size = file_fcb.size;
  stbuf->st_mtime = file_fcb.mtime;
  stbuf->st_ctime = file_fcb.ctime;

	return 0;
}

// Read a directory.
// Read 'man 2 readdir'.
static int myfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi){
	write_log("write_readdir(path=\"%s\", buf=0x%08x, filler=0x%08x, offset=%lld, fi=0x%08x)\n", path, buf, filler, offset, fi);

  struct my_fcb dir_fcb;

  if (find_file(path, &dir_fcb) != MYFS_FIND_FOUND) {
    write_log("myfs_readdir - ENOENT");
    return -ENOENT;
  }

  filler(buf, ".", NULL, 0);
  filler(buf, "..", NULL, 0);

  if (dir_fcb.size > 0) {
    void* dir_data = malloc(dir_fcb.size);
    read_file_data(dir_fcb, dir_data);

    struct my_dir_entry* dir_entry = dir_data;
    filler(buf, dir_entry->name, NULL, 0);
  }

	return 0;
}

// Read a file.
// Read 'man 2 read'.
static int myfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi){
	write_log("myfs_read(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n", path, buf, size, offset, fi);

  struct my_fcb file_fcb;

  if (find_file(path, &file_fcb) != MYFS_FIND_FOUND) {
    write_log("myfs_read - ENOENT");
    return -ENOENT;
  }

  void* file_data = malloc(file_fcb.size);
  read_file_data(file_fcb, file_data);

  memcpy(buf, file_data + offset, size);

	return 0;
}

// This file system only supports one file. Create should fail if a file has been created. Path must be '/<something>'.
// Read 'man 2 creat'.
static int myfs_create(const char *path, mode_t mode, struct fuse_file_info *fi){
  write_log("myfs_create(path=\"%s\", mode=0%03o, fi=0x%08x)\n", path, mode, fi);

  struct my_fcb dir_fcb;
  struct my_fcb file_fcb;

  int result = find_dir_entry(path, &dir_fcb, &file_fcb);

  if (result == MYFS_FIND_NO_DIR) {
    write_log("myfs_create - ENOENT\n");
    return -ENOENT;

  } else if (result == 0) {
    write_log("myfs_create - EEXIST\n");
    return -EEXIST;

  } else {
    create_file(mode, &file_fcb);

    char* file_name = path_file_name(path);
    add_dir_entry(&dir_fcb, &file_fcb, file_name);
    free(file_name);

    return 0;
  }
}

// Set update the times (actime, modtime) for a file. This FS only supports modtime.
// Read 'man 2 utime'.
static int myfs_utime(const char *path, struct utimbuf *ubuf){
  write_log("myfs_utime(path=\"%s\", ubuf=0x%08x)\n", path, ubuf);

  struct my_fcb file_fcb;

  if (find_file(path, &file_fcb) != MYFS_FIND_FOUND) {
    write_log("myfs_getattr - ENOENT");
    return -ENOENT;
  }

  file_fcb.mtime = ubuf->modtime;
  update_file(file_fcb);

  return 0;
}

// Write to a file.
// Read 'man 2 write'
static int myfs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi){
  write_log("myfs_write(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n", path, buf, size, offset, fi);

  struct my_fcb file_fcb;

  if (find_file(path, &file_fcb) != MYFS_FIND_FOUND) {
    write_log("myfs_getattr - ENOENT");
    return -ENOENT;
  }

  void* file_data = malloc(file_fcb.size);
  read_file_data(file_fcb, file_data);

  memcpy(file_data + offset, buf, size);
  write_file_data(file_fcb, file_data, file_fcb.size);

  return 0;
}

// Set the size of a file.
// Read 'man 2 truncate'.
static int myfs_truncate(const char *path, off_t newsize){
  write_log("myfs_truncate(path=\"%s\", newsize=%lld)\n", path, newsize);

  struct my_fcb file_fcb;

  if (find_file(path, &file_fcb) != MYFS_FIND_FOUND) {
    write_log("myfs_getattr - ENOENT");
    return -ENOENT;
  }

  void* old_file_data = malloc(file_fcb.size);
  void* new_file_data = malloc(newsize);

  memset(new_file_data, 0, newsize);

  if (file_fcb.size > newsize) {
    memcpy(new_file_data, old_file_data, newsize);
  } else {
    memcpy(new_file_data, old_file_data, file_fcb.size);
  }

  write_file_data(file_fcb, new_file_data, newsize);

	return 0;
}

// Set permissions.
// Read 'man 2 chmod'.
static int myfs_chmod(const char *path, mode_t mode){
  write_log("myfs_chmod(fpath=\"%s\", mode=0%03o)\n", path, mode);

  return 0;
}

// Set ownership.
// Read 'man 2 chown'.
static int myfs_chown(const char *path, uid_t uid, gid_t gid){
  write_log("myfs_chown(path=\"%s\", uid=%d, gid=%d)\n", path, uid, gid);

  return 0;
}

// Create a directory.
// Read 'man 2 mkdir'.
static int myfs_mkdir(const char *path, mode_t mode){
	write_log("myfs_mkdir: %s\n",path);

  return 0;
}

// Delete a file.
// Read 'man 2 unlink'.
static int myfs_unlink(const char *path){
	write_log("myfs_unlink: %s\n",path);

  return 0;
}

// Delete a directory.
// Read 'man 2 rmdir'.
static int myfs_rmdir(const char *path){
  write_log("myfs_rmdir: %s\n",path);

  return 0;
}

// OPTIONAL - included as an example
// Flush any cached data.
static int myfs_flush(const char *path, struct fuse_file_info *fi){
  write_log("myfs_flush(path=\"%s\", fi=0x%08x)\n", path, fi);

  return 0;
}

// OPTIONAL - included as an example
// Release the file. There will be one call to release for each call to open.
static int myfs_release(const char *path, struct fuse_file_info *fi){
  write_log("myfs_release(path=\"%s\", fi=0x%08x)\n", path, fi);

  return 0;
}

// OPTIONAL - included as an example
// Open a file. Open should check if the operation is permitted for the given flags (fi->flags).
// Read 'man 2 open'.
static int myfs_open(const char *path, struct fuse_file_info *fi){
	write_log("myfs_open(path\"%s\", fi=0x%08x)\n", path, fi);

	return 0;
}

static struct fuse_operations myfs_oper = {
	.getattr = myfs_getattr,
	.readdir = myfs_readdir,
	.open = myfs_open,
	.read = myfs_read,
	.create = myfs_create,
	.utime = myfs_utime,
	.write = myfs_write,
	.truncate = myfs_truncate,
	.flush = myfs_flush,
	.release = myfs_release,
};


// Initialise the in-memory data structures from the store. If the root object (from the store) is empty then create a root fcb (directory)
// and write it to the store. Note that this code is executed outide of fuse. If there is a failure then we have failed toi initlaise the
// file system so exit with an error code.
void init_fs(){
	write_log("init_fs\n");

	// Initialise the store.
	init_store();

	if (root_is_empty) {
		write_log("init_fs: root is empty\n");

    write_log("init_fs: creating root directory\n");

    struct my_fcb root_dir_fcb;
		create_directory(S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH, &root_dir_fcb);

		printf("init_fs: writing updated root object\n");

    memcpy(root_object.id, root_dir_fcb.id, sizeof(uuid_t));
		int rc = write_root();

	 	if (rc != UNQLITE_OK) {
   		error_handler(rc);
		}
	}
}

void shutdown_fs(){
	unqlite_close(pDb);
}

int main(int argc, char *argv[]){
	int fuserc;
	struct myfs_state *myfs_internal_state;

	//Setup the log file and store the FILE* in the private data object for the file system.
	myfs_internal_state = malloc(sizeof(struct myfs_state));
  myfs_internal_state->logfile = init_log_file();

	//Initialise the file system. This is being done outside of fuse for ease of debugging.
	init_fs();

	fuserc = fuse_main(argc, argv, &myfs_oper, myfs_internal_state);

	//Shutdown the file system.
	shutdown_fs();

	return fuserc;
}
