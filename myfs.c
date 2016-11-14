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

  // try to find the file by its path and current user
  int result = find_file(path, get_context_user(), &file_fcb);

  if (result == MYFS_FIND_NO_ACCESS) {
    // user cannot access one of the parent directories
    write_log("myfs_getattr - EACCES\n");
    return -EACCES;

  } else if (result != MYFS_FIND_FOUND) {
    // file does not exist
    write_log("myfs_getattr - ENOENT\n");
    return -ENOENT;
  }

  // clear the stat struct
  memset(stbuf, 0, sizeof(struct stat));

  // copy values from the FCB into the stat struct
  stbuf->st_mode = file_fcb.mode;
  stbuf->st_nlink = file_fcb.nlink;
  stbuf->st_uid = file_fcb.uid;
  stbuf->st_gid = file_fcb.gid;
  stbuf->st_size = file_fcb.size;
  stbuf->st_atime = file_fcb.atime;
  stbuf->st_mtime = file_fcb.mtime;
  stbuf->st_ctime = file_fcb.ctime;

  return 0;
}

// Read a directory.
// Read 'man 2 readdir'.
static int myfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi){
  write_log("write_readdir(path=\"%s\", buf=0x%08x, filler=0x%08x, offset=%lld, fi=0x%08x)\n", path, buf, filler, offset, fi);

  // get the FCB of the directory by the file handle returned by opendir
  struct my_fcb dir_fcb;
  get_open_file(fi->fh, &dir_fcb);

  filler(buf, ".", NULL, 0);
  filler(buf, "..", NULL, 0);

  // iterate directory entries
  struct my_dir_iter iter;
  iterate_dir_entries(&dir_fcb, &iter);

  struct my_dir_entry* entry;

  while ((entry = next_dir_entry(&iter)) != NULL) {
    filler(buf, entry->name, NULL, 0);
  }

  clean_dir_iterator(&iter);

  return 0;
}

// Read a file.
// Read 'man 2 read'.
static int myfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi){
  write_log("myfs_read(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n", path, buf, size, offset, fi);

  // get the FCB of the file by the file handle returned by open
  struct my_fcb file_fcb;
  get_open_file(fi->fh, &file_fcb);

  if (file_fcb.size == 0) {
    // file is empty, nothing to see here
    return 0;

  } else if (offset >= file_fcb.size) {
    // cannot read beyond the end of the file
    return 0;

  } else if ((offset + size) > file_fcb.size) {
    // cannot read beyond the end of file, but can read until it
    size = file_fcb.size - offset;

    read_file_data(&file_fcb, buf, size, offset);
    return size;

  } else {
    // read range is ok
    read_file_data(&file_fcb, buf, size, offset);
    return size;
  }
}

// Create a file.
// Read 'man 2 creat'.
static int myfs_create(const char *path, mode_t mode, struct fuse_file_info *fi){
  write_log("myfs_create(path=\"%s\", mode=0%03o, fi=0x%08x)\n", path, mode, fi);

  /** @var Parent directory FCB */
  struct my_fcb dir_fcb;
  /** @var Created file FCB */
  struct my_fcb file_fcb;

  // try to find the file (to check if it exists) and parent directory (to add to)
  int result = find_dir_entry(path, get_context_user(), &dir_fcb, &file_fcb);

  if (result == MYFS_FIND_NO_DIR) {
    // parent directory does not exist
    write_log("myfs_create - ENOENT\n");
    return -ENOENT;

  } else if (result == MYFS_FIND_FOUND) {
    // file already exists
    write_log("myfs_create - EEXIST\n");
    return -EEXIST;

  } else if (
    result == MYFS_FIND_NO_ACCESS ||
    !can_write(&dir_fcb, get_context_user())
  ) {
    // user cannot access ancestor directory, or cannot write to parent directory
    write_log("myfs_create - EACCES\n");
    return -EACCES;

  } else {
    // file does not exist and user has sufficient rights

    // create a new file and write it to the database, save FCB to file_fcb
    create_file(mode, get_context_user(), &file_fcb);

    // get the file name from the path and add the file to the parent directory
    // path string needs to be duplicated because it will be modified by path_file_name
    char* path_dup = strdup(path);
    char* file_name = path_file_name(path_dup);
    int result = link_file(&dir_fcb, &file_fcb, file_name);
    free(path_dup);

    if (result < 0) {
      // the parent directory does not have space left to add the file
      remove_file(&file_fcb);
      write_log("myfs_create - EFBIG\n");
      return -EFBIG;
    }

    // add the file to open file table and get its file handle
    int fh = add_open_file(&file_fcb);

    if (fh < 0) {
      // too many files are open
      write_log("myfs_create - ENFILE\n");
      return -ENFILE;
    }

    // save the file handle for future calls
    fi->fh = fh;

    return 0;
  }
}

// Set update the times (actime, modtime) for a file.
// Read 'man 2 utime'.
static int myfs_utime(const char *path, struct utimbuf *ubuf){
  write_log("myfs_utime(path=\"%s\", ubuf=0x%08x)\n", path, ubuf);

  struct my_fcb file_fcb;

  // try to find the file by its path and current user
  int result = find_file(path, get_context_user(), &file_fcb);

  if (
    result == MYFS_FIND_NO_DIR ||
    result == MYFS_FIND_NO_FILE
  ) {
    // file does not exist
    write_log("myfs_utime - ENOENT\n");
    return -ENOENT;

  } else if (
    result == MYFS_FIND_NO_ACCESS ||
    !can_write(&file_fcb, get_context_user())
  ) {
    // user cannot access an ancestor directory or write to the file
    write_log("myfs_utime - EACCES\n");
    return -EACCES;
  }

  // update the FCB
  file_fcb.atime = ubuf->actime;
  file_fcb.mtime = ubuf->modtime;

  // write the updated FCB into the database
  update_file(&file_fcb);

  return 0;
}

// Write to a file.
// Read 'man 2 write'
static int myfs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi){
  write_log("myfs_write(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n", path, buf, size, offset, fi);

  // get the FCB by the file handle
  struct my_fcb file_fcb;
  get_open_file(fi->fh, &file_fcb);

  if (offset >= MY_MAX_FILE_SIZE) {
    // cannot writer beyond the maximum file size
    write_log("myfs_write - EFBIG\n");
    return -EFBIG;

  } else if ((offset + size) > MY_MAX_FILE_SIZE) {
    // cannot write beyond the maximum file size, but can write until it
    size = MY_MAX_FILE_SIZE - offset;

    write_file_data(&file_fcb, (char*)buf, size, offset);
    return size;

  } else {
    // write range is ok
    write_file_data(&file_fcb, (char*)buf, size, offset);
    return size;
  }
}

// Set the size of a file.
// Read 'man 2 truncate'.
static int myfs_truncate(const char *path, off_t newsize){
  write_log("myfs_truncate(path=\"%s\", newsize=%lld)\n", path, newsize);

  struct my_fcb file_fcb;

  // try to find the file by its path
  int result = find_file(path, get_context_user(), &file_fcb);

  if (
    result == MYFS_FIND_NO_DIR ||
    result == MYFS_FIND_NO_FILE ||
    !is_file(&file_fcb)
  ) {
    // file does not exist, or is not a regular file
    write_log("myfs_truncate - ENOENT\n");
    return -ENOENT;
  }

  else if (
    result == MYFS_FIND_NO_ACCESS ||
    !can_write(&file_fcb, get_context_user())
  ) {
    // cannot access an ancestor directory, or cannot write to the file
    write_log("myfs_getattr - EACCES\n");
    return -EACCES;
  }

  // change the file size
  truncate_file(&file_fcb, newsize);

  return 0;
}

// Set permissions.
// Read 'man 2 chmod'.
static int myfs_chmod(const char *path, mode_t mode){
  write_log("myfs_chmod(fpath=\"%s\", mode=0%03o)\n", path, mode);

  struct my_user user = get_context_user();

  // try to find the file by its path
  struct my_fcb file_fcb;
  int result = find_file(path, user, &file_fcb);

  if (
    result == MYFS_FIND_NO_DIR ||
    result == MYFS_FIND_NO_FILE
  ) {
    // file does not exist
    write_log("myfs_chmod - ENOENT\n");
    return -ENOENT;

  } else if (result == MYFS_FIND_NO_ACCESS) {
    // file cannot be accessed
    write_log("myfs_getattr - EACCES\n");
    return -EACCES;

  } else if (file_fcb.uid != user.uid) {
    // the current user is not the owner of the file
    write_log("myfs_getattr - EPERM\n");
    return -EPERM;
  }

  // update the FCB and write it to the database
  file_fcb.mode = mode;
  file_fcb.ctime = time(0);
  update_file(&file_fcb);

  return 0;
}

// Set ownership.
// Read 'man 2 chown'.
static int myfs_chown(const char *path, uid_t uid, gid_t gid){
  write_log("myfs_chown(path=\"%s\", uid=%d, gid=%d)\n", path, uid, gid);

  struct my_user user = get_context_user();

  // try to find the file by its path
  struct my_fcb file_fcb;
  int result = find_file(path, user, &file_fcb);

  if (
    result == MYFS_FIND_NO_DIR ||
    result == MYFS_FIND_NO_FILE
  ) {
    // file does not exist
    write_log("myfs_chmod - ENOENT\n");
    return -ENOENT;

  } else if (result == MYFS_FIND_NO_ACCESS) {
    // file cannot be accessed
    write_log("myfs_getattr - EACCES\n");
    return -EACCES;
  }

  // update the FCB and write it to the database
  file_fcb.uid = uid;
  file_fcb.gid = gid;
  file_fcb.ctime = time(0);
  update_file(&file_fcb);

  return 0;
}

// Create a directory.
// Read 'man 2 mkdir'.
static int myfs_mkdir(const char *path, mode_t mode){
  write_log("myfs_mkdir: %s\n", path);

  struct my_fcb parent_fcb;
  struct my_fcb dir_fcb;

  // try to find the directory and its parent directory by its path
  int result = find_dir_entry(path, get_context_user(), &parent_fcb, &dir_fcb);

  if (result == MYFS_FIND_NO_DIR) {
    // parent directory does not exist
    write_log("myfs_mkdir - ENOENT\n");
    return -ENOENT;

  } else if (result == MYFS_FIND_FOUND) {
    // directory already exists
    write_log("myfs_mkdir - EEXIST\n");
    return -EEXIST;

  } else if (result == MYFS_FIND_NO_ACCESS) {
    // user cannot access the parent directory
    write_log("myfs_mkdir - EACCES\n");
    return -EACCES;

  } else {
    // create a new directory and write it to the database
    create_directory(mode, get_context_user(), &dir_fcb);

    // get the directory name from the path and add the directory to the parent directory
    // path string needs to be duplicated because it will be modified by path_file_name
    char* path_dup = strdup(path);
    char* dir_name = path_file_name(path_dup);
    int result = link_file(&parent_fcb, &dir_fcb, dir_name);
    free(path_dup);

    if (result < 0) {
      // the parent directory does not have space left to add the directory
      remove_file(&dir_fcb);
      write_log("myfs_mkdir - EFBIG\n");
      return -EFBIG;
    }

    return 0;
  }
}

// Delete a file.
// Read 'man 2 unlink'.
static int myfs_unlink(const char *path){
  write_log("myfs_unlink: %s\n",path);

  struct my_fcb dir_fcb;
  struct my_fcb file_fcb;

  // try to find the file and its parent directory by its path
  int result = find_dir_entry(path, get_context_user(), &dir_fcb, &file_fcb);

  if (
    result == MYFS_FIND_NO_DIR ||
    result == MYFS_FIND_NO_FILE
  ) {
    // file does not exist
    write_log("myfs_unlink - ENOENT\n");
    return -ENOENT;

  } else if (
    result == MYFS_FIND_NO_ACCESS ||
    !can_write(&dir_fcb, get_context_user())
  ) {
    // user cannot access the file or write to the parent directory
    write_log("myfs_unlink - EACCES\n");
    return -EACCES;

  } else if (!is_file(&file_fcb)) {
    // the file is not a regular file
    write_log("myfs_unlink - EPERM\n");
    return -EPERM;
  }

  // get the file name from the path and remove the file from the parent directory
  // path string needs to be duplicated because it will be modified by path_file_name
  // if no other links point to the file and it is not open, it will be deleted
  char* path_dup = strdup(path);
  char* file_name = path_file_name(path_dup);
  unlink_file(&dir_fcb, &file_fcb, file_name);
  free(path_dup);

  return 0;
}

// Delete a directory.
// Read 'man 2 rmdir'.
static int myfs_rmdir(const char *path){
  write_log("myfs_rmdir: %s\n",path);

  struct my_fcb parent_fcb;
  struct my_fcb dir_fcb;

  // try to find the drectory and its parent directory by its path
  int result = find_dir_entry(path, get_context_user(), &parent_fcb, &dir_fcb);

  if (
    result == MYFS_FIND_NO_DIR ||
    result == MYFS_FIND_NO_FILE
  ) {
    // directory does not exist
    write_log("myfs_rmdir - ENOENT\n");
    return -ENOENT;

  } else if (
    result == MYFS_FIND_NO_ACCESS ||
    !can_write(&parent_fcb, get_context_user())
  ) {
    // user cannot access the directory or write to the parent directory
    write_log("myfs_rmdir - EACCES\n");
    return -EACCES;

  } else if (!is_directory(&dir_fcb)) {
    // the found FCB is not a directory
    write_log("myfs_rmdir - ENOTDIR\n");
    return -ENOTDIR;

  } else if (get_directory_size(&dir_fcb) != 0) {
    // the directory is not empty
    write_log("myfs_rmdir - ENOTEMPTY\n");
    return -ENOTEMPTY;
  }

  // get the directory name from the path and remove the directory from the parent directory
  // path string needs to be duplicated because it will be modified by path_file_name
  // if the directory is not open, it will be deleted
  char* path_dup = strdup(path);
  char* file_name = path_file_name(path_dup);
  unlink_file(&parent_fcb, &dir_fcb, file_name);
  free(path_dup);

  return 0;
}

// Create a hard link to the file.
// Read 'man 2 link'.
static int myfs_link(const char* from, const char* to) {
  write_log("myfs_link(from=\"%s\", to=\"%s\")\n", from, to);
  int result;

  // try to find the linked file
  struct my_fcb from_fcb;
  result = find_file(from, get_context_user(), &from_fcb);

  if (result == MYFS_FIND_NO_ACCESS) {
    // user cannot access the linked file's parent directory
    write_log("myfs_link - EACCES\n");
    return -EACCES;

  } else if (result != MYFS_FIND_FOUND) {
    // linked file does not exist
    write_log("myfs_link - ENOENT\n");
    return -ENOENT;

  } else if (is_directory(&from_fcb)) {
    // linked file is a directory
    write_log("myfs_link - EPERM\n");
    return -EPERM;
  }

  struct my_fcb to_fcb;
  struct my_fcb dir_fcb;

  // try to find the file that would be replaced by the link
  result = find_dir_entry(to, get_context_user(), &dir_fcb, &to_fcb);

  if (result == MYFS_FIND_NO_DIR) {
    // parent directory for the link does not exist
    write_log("myfs_link - ENOENT\n");
    return -ENOENT;

  } else if (result == MYFS_FIND_NO_ACCESS) {
    // user cannot access the parent directory
    write_log("myfs_link - EACCES\n");
    return -EACCES;

  } else if (result == MYFS_FIND_FOUND) {
    // there is already a file on this path that would be replaced
    write_log("myfs_link - EEXIST\n");
    return -EEXIST;

  } else {
    // get the link file name from the path and add the link to the parent directory
    // path string needs to be duplicated because it will be modified by path_file_name
    char* to_path_dup = strdup(to);
    char* file_name = path_file_name(to_path_dup);
    result = link_file(&dir_fcb, &from_fcb, file_name);
    free(to_path_dup);

    if (result < 0) {
      // the parent directory does not have space left to add the link
      write_log("myfs_link - EFBIG\n");
      return -EFBIG;
    }

    return 0;
  }
}

// Rename the file.
// Read 'man 2 rename'
static int myfs_rename(const char* from, const char* to) {
  write_log("myfs_rename(from=\"%s\", to=\"%s\")\n", from, to);
  int result;

  // try to find the renamed file
  struct my_fcb from_dir;
  struct my_fcb from_file;
  result = find_dir_entry(from, get_context_user(), &from_dir, &from_file);

  if (
    result == MYFS_FIND_NO_ACCESS ||
    !can_write(&from_dir, get_context_user())
  ) {
    // user cannot access or write to the original parent directory
    write_log("myfs_rename - EACCES\n");
    return -EACCES;
  } else if (result != MYFS_FIND_FOUND) {
    // renamed file does not exist
    write_log("myfs_rename - ENOENT\n");
    return -ENOENT;
  }

  // try to find the file that would be replaced
  struct my_fcb to_dir;
  struct my_fcb to_file;
  result = find_dir_entry(to, get_context_user(), &to_dir, &to_file);

  if (result == MYFS_FIND_NO_DIR) {
    // parent directory does not exist
    write_log("myfs_rename - ENOENT\n");
    return -ENOENT;
  }

  if (
    result == MYFS_FIND_NO_ACCESS ||
    !can_write(&to_dir, get_context_user())
  ) {
    // user cannot access or write to the destination parent directory
    write_log("myfs_rename - EACCES\n");
    return -EACCES;
  }

  // get the file names of the original and renamed files
  char* to_dup = strdup(to);
  char* to_file_name = path_file_name(to_dup);

  char* from_dup = strdup(from);
  char* from_file_name = path_file_name(from_dup);

  // if there is already a file at the destination, remove it
  if (result == MYFS_FIND_FOUND) {
    unlink_file(&to_dir, &to_file, to_file_name);
  }

  // remove the file from original directory and add it to the destination directory
  if (uuid_compare(from_dir.id, to_dir.id) == 0) {
    // if these directories are the same, it means that both from_dir and to_dir
    // contain the same FCB, but if we change one, the other one does not change
    // we need to make sure to use only one of the FCBs in this case
    remove_dir_entry(&from_dir, from_file_name);
    result = add_dir_entry(&from_dir, &from_file, to_file_name);
  } else {
    remove_dir_entry(&from_dir, from_file_name);
    result = add_dir_entry(&to_dir, &from_file, to_file_name);
  }

  free(to_dup);
  free(from_dup);

  if (result < 0) {
    // unable to add directory entry because the directory is too big
    write_log("myfs_rename - EFBIG\n");
    return -EFBIG;
  }

  return 0;
}

// Open a file. Open should check if the operation is permitted for the given flags (fi->flags).
// Read 'man 2 open'.
static int myfs_open(const char *path, struct fuse_file_info *fi){
  write_log("myfs_open(path\"%s\", fi=0x%08x)\n", path, fi);

  struct my_user user = get_context_user();
  struct my_fcb file_fcb;

  // try to find the file by its path
  int result = find_file(path, user, &file_fcb);

  if (
    result == MYFS_FIND_NO_DIR ||
    result == MYFS_FIND_NO_FILE ||
    !is_file(&file_fcb)
  ) {
    // the file does not exist or is not a file
    write_log("myfs_open - ENOENT\n");
    return -ENOENT;

  } else if (
    result == MYFS_FIND_NO_ACCESS ||
    !check_open_flags(&file_fcb, user, fi->flags)
  ) {
    // user cannot access the parent directory, or the opened file with the
    // specified flags
    write_log("myfs_open - EACCES\n");
    return -EACCES;
  }

  // open the file after creating it
  int fh = add_open_file(&file_fcb);

  if (fh < 0) {
    // too many files are open
    write_log("myfs_open - ENFILE\n");
    return -ENFILE;
  }

  // save the file handle for future calls
  fi->fh = fh;

  return 0;
}

// Release the file. There will be one call to release for each call to open.
static int myfs_release(const char *path, struct fuse_file_info *fi){
  write_log("myfs_release(path=\"%s\", fi=0x%08x)\n", path, fi);

  // remove the file from the open file table
  // if there are no other links pointing to it and is not open anywhere else,
  // it will be deleted
  remove_open_file(fi->fh);

  return 0;
}

static int myfs_opendir(const char *path, struct fuse_file_info *fi){
  write_log("myfs_opendir(path\"%s\", fi=0x%08x)\n", path, fi);

  struct my_user user = get_context_user();
  struct my_fcb dir_fcb;

  // try to find the directory by its path
  int result = find_file(path, user, &dir_fcb);

  if (
    result == MYFS_FIND_NO_DIR ||
    result == MYFS_FIND_NO_FILE ||
    !is_directory(&dir_fcb)
  ) {
    // the directory does not exist, or is not a directory
    write_log("myfs_open - ENOENT\n");
    return -ENOENT;

  } else if (
    result == MYFS_FIND_NO_ACCESS ||
    !check_open_flags(&dir_fcb, user, fi->flags)
  ) {
    // the user cannot access a parent directory, or cannot open the directory
    // wit the specified flags
    write_log("myfs_open - EACCES\n");
    return -EACCES;
  }

  // add the directory to the open file table
  int fh = add_open_file(&dir_fcb);

  if (fh < 0) {
    // too many files are open
    write_log("myfs_opendir - ENFILE\n");
    return -ENFILE;
  }

  // save the file for future calls
  fi->fh = fh;

  return 0;
}

// Release the directory. There will be one call to releasedir for each call to opendir.
static int myfs_releasedir(const char *path, struct fuse_file_info *fi){
  write_log("myfs_release(path=\"%s\", fi=0x%08x)\n", path, fi);

  // remove the directory from the open file table
  // if there are no other links pointing to it and is not open anywhere else,
  // it will be deleted
  remove_open_file(fi->fh);

  return 0;
}

static struct fuse_operations myfs_oper = {
  .getattr = myfs_getattr,
  .readdir = myfs_readdir,
  .open = myfs_open,
  .opendir = myfs_opendir,
  .read = myfs_read,
  .create = myfs_create,
  .utime = myfs_utime,
  .write = myfs_write,
  .truncate = myfs_truncate,
  .release = myfs_release,
  .releasedir = myfs_releasedir,
  .unlink = myfs_unlink,
  .mkdir = myfs_mkdir,
  .rmdir = myfs_rmdir,
  .chmod = myfs_chmod,
  .chown = myfs_chown,
  .link = myfs_link,
  .rename = myfs_rename,
};


// Initialise the in-memory data structures from the store. If the root object (from the store) is empty then create a root fcb (directory)
// and write it to the store. Note that this code is executed outide of fuse. If there is a failure then we have failed toi initlaise the
// file system so exit with an error code.
void init_fs(){
  printf("init_fs\n");

  // Initialise the store.
  init_store();

  if (root_is_empty) {
    printf("init_fs: root is empty\n");

    printf("init_fs: creating root directory\n");

    // cannot user FUSE context because it has not been initialised yet
    struct my_user user = {.uid = getuid(), .gid = getgid()};

    // create the root directory
    struct my_fcb root_dir_fcb;
    create_directory(S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH,
      user, &root_dir_fcb);

    // normally the parent directory points to the child directory, but root
    // directory has no parents
    // set the number of links to 1 manually to keep it from being deleted
    // after closing
    root_dir_fcb.nlink = 1;
    update_file(&root_dir_fcb);

    printf("init_fs: writing updated root object\n");

    // save the root object to the database
    uuid_copy(root_object.id, root_dir_fcb.id);
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
