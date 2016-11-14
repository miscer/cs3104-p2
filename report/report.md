% CS3104 P2 Report
% 140015533
% 14 November 2016

# Overview

In this practical my task was to implement a simple user-space file system using FUSE. The file system uses UnQLite for storing the file data and meta-data.

The file system has a tree directory hierarchy where directories can contain both files and directories. Each file and directory has standard meta-data attached to it. Files can be created, updated and deleted. Files have dynamic size up to a certain limit. Hard links and renaming files is supported.

# Design and implementation

## Files

All regular files and directories are treated as files by the file system. Each file has an UUID assigned to it. This UUID is then used as a key in the database.

### Meta-data

Each file carries some meta-data. In my implementation these are:

* User ID
* Group ID
* File mode (permissions)
* Time of last access, modification and change to meta-data
* Number of links pointing to it
* Size of the file in bytes

These are similar to the fields in the standard Linux stat struct. All meta-data is stored in the file control block.

### Data

File data is stored separately from the meta-data. It is split into a number of data blocks of fixed size. This means that file data can be read and written without accessing the whole data at a time, but only small data blocks one at a time.

Each data block can be accessed by its UUID. These UUIDs are stored in an index block, which contains a fixed-size table mapping data block number to its UUID. Because of that there is a limit on file size, which depends on the size of the data block and number of entries in the index block.

### Creating, updating and deleting

When creating a file, its UUID is generated and meta-data (UID, GID, mode) initialised to specified values in the file control block. The FCB is then stored in the database. Additionally, the UUID of the index block of the file is generated. An empty index block is then created and stored in the database.

To update the file meta-data, the FCB struct is modified and immediately stored in the database, replacing the old file control block.

File is deleted by removing all the data blocks, the index block and the file control block from the database. There cannot be any links pointing to the file when it is being deleted, or the file system will not function properly.

## Persistent storage

All persistent data is stored in an UnQLite database. UUIDs from libuuid are used as keys for all blocks. One exception is the root object, which only contains the UUID of the root directory file control block.

### Database access

I have created helper functions that are used to read, write and delete UnQLite objects. They always use UUIDs as keys and handle error return values from the database. This allows me to change the database implementation if needed without modifying other code. It also simplifies other methods by hiding the database API.

### Data structures

All data structures used for storing file data and meta-data are shown in Figure \ref{fig:ondiskstructs}.

![Diagram of the data structures used for storing files and their data \label{fig:ondiskstructs}](figures/ondiskstructs.png)

### Reading and writing

When file data is being read or written, it is done one block at a time. This means that the whole file data never has to be read into memory at once.

Read and write operations are always provided with a buffer, a size and an offset. During read, the data are copied into the buffer from the file starting at the specified offset. During write, they are copied from the buffer into the file at the specified offset.

From the size and offset values we are able to calculate which blocks contain the accessed data by finding the first and last block.

```
first_block = round_down_to(size, BLOCK_SIZE) / BLOCK_SIZE
last_block = round_up_to(size, BLOCK_SIZE) / BLOCK_SIZE - 1
```

Once we have the values, we need to iterate through the blocks in the range and either read from or write to them.

For this I have made two functions that have the block UUID, number (i.e. the index in the index block array), buffer, size and offset as parameters. These functions then calculate the position of the block in relation to the part of the file that is being accessed. There are four cases for the relative position:

* Buffer starts and ends in the block
* Buffer starts in this block and ends in some other block
* Buffer starts in some other block and ends in this block
* Buffer starts and ends in other blocks

The functions then read or write a part of or the whole block as appropriate.

This algorithm allows us to go through the buffer block by block and to avoid copying the whole file or a large part of it into memory to be able to process it.

```
first_block, last_block = get_block_indexes(size, offset)

for (block = first_block; block <= last_block; block++) {
  read_block_to_buffer(index_block[block], block, buffer, size, offset)
}
```

Finally, before writing the system checks if the file is big enough to accommodate the write. If not, the file is truncated to the needed size.

### Truncating

Truncating can either increase or decrease file size. This means we might need to create or remove data blocks for the file.

We can find out how many blocks are currently used by the file and how many are needed for the new file size:

```
old_num_blocks = round_down_to(file_size, BLOCK_SIZE) / BLOCK_SIZE
new_num_blocks = round_down_to(new_size, BLOCK_SIZE) / BLOCK_SIZE
```

From these two values we know if we need to add or remove blocks. When adding, the blocks are first initialised with zeroes as the data and then added to the index block. When removing, the extra blocks are simply deleted from the database.

Finally we update the FCB with the new size so that future read or write operations use the appropriate blocks.

## Directories

The file system supports directories that can contain both files and other directories. Directories are treated as files, but they have the directory bit set in the file mode.

Directory entries are stored in file data blocks. The mechanism used for reading and writing data is the same as with regular files.

### Data structures

Each directory has a header struct that stores the number of directory entries and the offset of the first unused entry in the directory.

Directory entry struct contains the name of the entry and the UUID of the FCB of the file. The entries are not sorted in any way.

Not all directory entries are used. For example, if a file is removed from the directory, the entry is only marked as unused, but not removed from the directory.

For this there are two extra fields in the entry struct. One is for recording whether the entry is currently used. The second one is the offset of the next free entry. This, together with the first unused entry offset in the header, forms a free list of unused directory entries. If there is no next free entry, the offset is set to -1.

The offsets are essentially indexes in the array containing all directory entries. But since this array has a dynamic number of entries, it cannot be declared as an array type.

A diagram showing the layout of directory structs and the free list is in Figure \ref{fig:directorystructs}.

![Diagram of the directory structures \label{fig:directorystructs}](figures/directory.png)

### Creating

Directories are created in the same way as regular files, except that a header is written to the file data. The number of entries is initially zero and the first free entry does not exist, so the value is -1.

### Iterating entries

Directory entries need to be iterated not only when reading the file, but also during tree traversal. Since the directory data structures are quite complicated, I have built functions that hide the internal structure and allow for simple iteration of directory entries.

To start iterating the directory, an iterator object is created. The object stores the offset of the current entry and raw directory data.

The iterator object is then used with a function that returns the pointer to the next directory entry that is used (i.e. it skips unused directory entries). If there are no more directory entries left, it returns NULL.

After iteration is finished, the iterator object is cleaned up in a function that must be called to prevent memory leaks.

```
iterator = iterate_dir_entries(directory)

while ((entry = next_dir_entry(iterator)) != NULL) {
  do_something(entry)
}

clean_dir_iterator(iterator)
```

Internally the `next_dir_method` iterates through directory entries from the current position until it finds an used entry and then returns it. If the iteration ends, NULL is returned as there were no used entries left.

```
while (iter.position < dir_header.items) {
  entry = get_dir_entry(directory, iter.position)

  iter.position++

  if (entry.used) {
    return entry
  }
}

return NULL
```

### Adding and removing entries

To add a new directory entry, the program first tries to find an unused directory entry in the free list. If it finds one, it is removed from the list. If it doesn't find one, a new entry is created at the end of the directory data.

Once there is an entry that can be used, the name and file UUID are copied to the entry and the entry is marked as used.

When removing a directory entry, it is first found in the directory data by the entry name and then simply marked as unused and added to the head of the free list.

### Renaming

File is renamed by removing it from its parent directory and adding it to some other directory, or the same directory, but with a different name. If there already exists a file with the new name, it is unlinked.

## Tree traversal

Traversing the directory tree is crucial to any file system that supports file hierarchy. In my implementation the whole traversal is implemented in one function that takes a file path and finds the file and the parent directory of the file. This functionality was sufficient for implementing all other parts of the program that need it.

For example, to add a file to a directory, the algorithm is used both to locate the directory and to check that the file does not already exist. In this case the returned file would be null, but the returned directory would point to the parent directory, to which is the file then added.

The traversal starts at the root directory. Its UUID is stored in the root object in the database.

```
root_dir = read_file(root_object.id)
```

Now the `file` and `parent` variables are initialised. The `file` variable holds the file we are looking for and the `parent` variable holds the parent directory. `file` is set to point to the root directory - the algorithm can return a directory as the found file.

```
file = root_dir
parent = null
```

Then the path string is split into two parts - before and after the first `/` character. The first path is stored in `entry_name` and will be used to traverse the current directory.

```
entry_name, path = split_path(path)
```

If `entry_name` is null, it means we have exhausted the path and got to the file that is referred to it. However, if it is not null, it means we have not got to the end of the path and that the value of the `file` variable is a directory that needs to be iterated.

```
while (entry_name != NULL) {
  parent = file

  if (!is_directory(parent)) {
    return null, null
  }
  ...
```

During traversal we also need to check the directory permissions. The user needs to have execute rights for every directory in the path in order to be able to access the file.

```
  ...
  if (!can_execute(parent)) {
    return null, null
  }
  ...
```

Next we iterate the directory to find an entry with a matching name

```
  ...
  matching_entry = null

  for (entry in parent) {
    if (entry.name == entry_name) {
      matching_entry = entry
      break
    }
  }
  ...
```

If a matching entry is found, it means that the path is correct so far and we were able to locate the next file needed to continue traversal. The file is read into the `file` variable and path is split into two parts again.

If the file in the `file` variable is the path points to, `entry_name` will become null, the iteration will stop and the file will be returned.

```
  ...
  if (matching_entry) {
    file = read_file(entry.file_id)
    entry_name, path = split_path(path)
  }
  ...
```

If there is no matching entry in the directory, it means that the path is wrong.

However, the path might have pointed to an existing parent directory, but to a non-existent file. In this case we still need to return the parent directory, even though the file does not exist.

```
  ...
  else {
    if (path == null) {
      return null, parent
    } else {
      return null, null
    }
  }
}
...
```

Finally, if everything went well, the found file and parent directory can be returned.

```
...
return file, parent
```

## Hard links

Each file keeps track of how many hard links are pointing to it. This value is then used when unlinking the file to check if the file should be deleted from the database.

The link operation is supported by the file system. It simply adds the linked file's UUID as an entry to another directory and increases the number of links value in the file's FCB.

The unlink operation first removes the file's directory entry and then decrements the number of links. If the number is zero and the file is not open, it is deleted from the database.

## Open files

The file system keeps track of open files (and directories) in a fixed-size array. This means that there is a limit on open files determined by the size of the array.

### Data structures

The array contains the UUIDs of the open files. I have chosen to store only the UUIDs instead of whole FCBs to simplify the code. If the FCBs were stored, they would have to be updated on any change that is made in them by some other call.

### Opening and closing files

When a file or directory is open (with `open`, `create` or `opendir`) its UUID is added to the array and the index in the array is returned as the file handle. This file handle is then passed to subsequent read and write operations by FUSE and the open file's FCB is then loaded by the UUID when needed, instead of traversing the directory tree twice to find it.

File permissions are checked during the open operation and if the user cannot access the file, the open operation fails.

If the same file is open more than once, a different file handle is returned each time. This allows the user to open the file in different modes (read, write, read/write).

After the file is closed, the UUID in the open file array is marked as unused. For this a boolean value is associated with each entry in the array.

### Removed files

It is important to keep track of open files to allow programs to continue reading/writing the files they opened even after the last links to the files have been removed.

For example, a program can create a private file by creating a new file, opening it and then unlinking it.

To be able to do this, the unlink operation does not delete the file from the database if it is still open. The close operation will then delete the file if there are no links pointing to it after it is closed.

## Permissions

The file system checks user permissions for all operations. The access mode is stored in the file control block in the mode field.

The access mode is split into three levels - user, group and others. Each level can have any combination of read, write and execute permissions.

The read permission is needed to read a file or list directory entries. The write permission is needed to make any changes to file data and to add or remove directory entries. The execute permission is needed on a directory to be able to do any operation on the directory entries.

Which level is used is determined by the user and group IDs of the file. If the current user is the owner of the file (their ID is the UID of the file), the user level is used. If the current user's group is owner of the file (ID of the group is the GID of the file), the group level is used. Otherwise the other level is used.

Only the owner of the file is able to change the access mode of the file.

# Testing

## Internal testing

I have written a couple of small programs that use the functions used by the file system. Each program creates its own UnQLite database and initialises the file system (i.e. creates the root object).

For example, to test file truncation the basic test looks like this:

```c
create_file(0, user, &file_fcb);
assert(file_fcb.size == 0);

truncate_file(&file_fcb, 1000);
assert(file_fcb.size == 1000);
```

These parts were tested in this way:

* Reading, writing and deleting database objects
* Creating directories and files
* Updating file meta-data
* Deleting files
* Truncating files
* Adding, removing and iterating directory entries
* Writing and reading file data
* Tree traversal
* Creating and removing hard links
* Opening and closing files
* Path string functions
* Checking file permissions

With this method no FUSE code was run and this enabled me to properly check if the individual functions work correctly.

## External testing

To check the FUSE code I have used shell scripts that tested the functionality. For example, a simple write test script would look like this:

```bash
TEST_DATA=testing

REAL_FILE=./write.txt
TEST_FILE=$1/write.txt

echo ${TEST_DATA} > ${REAL_FILE}
echo ${TEST_DATA} > ${TEST_FILE}

cmp --silent ${REAL_FILE} ${TEST_FILE}

if [ $? -ne 0 ]; then
  echo "FAILED write test"
  exit -1
fi

echo "PASSED write test"
```

I have usually run the test scripts after the tests described in the previous section to confirm that everything is working properly even when running the program with FUSE.

## Large files

To check if the file system can handle the files with the maximum file size and reject bigger files, I have temporarily lowered the maximum file size limit to make it faster to test.

Then I have created a file filled with random data with the maximum file size, copied it to the file system and then using `md5sum` checked if their content is the same.

Then I have created another file that was too big for the system to handle and tried to copy it into the file system, expecting an error to be returned.

# Evaluation

## System limits

### File size

Because I have implemented indexed allocation, the file size is limited, as there is a limited number of entries in an index block.

The data block size and number of entries in an index block is configurable in the source code. At the moment the data block size is $16\,\text{KiB}$ and there are $2^{16}$ entries in an index block. This gives us $2^{16} \times 16\,\text{KiB} = 1\,\text{GiB}$ maximum file size.

### Directory size

The file size limit also applies to directories, as the directory entries are stored the same way as data of regular files. However, the exact values depend on the architecture because of varying sizes of integers.

On my machine the directory header struct takes up 8 bytes and an entry 280 bytes. This means that the maximum number of directory entries, assuming maximum file size of $1\,\text{GiB}$, is:

$$\left \lfloor{\frac{2^{30} - 8}{280}} \right \rfloor = 3834792$$

### Path and file name length

Directory entry name can be any length, but it has to be no more than 255 characters. The length of a path is not limited in the program.

## Possible improvements

### File data indexing

At the moment the index block takes $2^{16} \times 16\,\text{B} = 1\,\text{MiB}$. This could be problematic, since the index block is created for every file, even if it contains a few bytes. This could be improved by incorporating a 2-level indexing or a hybrid scheme similar to UNIX file systems.

### Directory entries

Currently the directory entries are stored in an unsorted array. This makes searching for an entry slow, as we cannot use binary search. While we could make sure the entries are sorted, this would slow down adding entries to the directory. A binary tree could be possibly used to store the entries in a way that would allow efficient adding, removing and search.

# Problems

During development I have faced some obstacles, but I have managed to find a solution to each of them.

At the beginning I found out that FUSE works only on Linux and not on my computer. This was solved easily by creating a virtual machine with Fedora installed using Vagrant. The code was then synchronised between my computer and the VM and compiled and run on the VM.

Second problem was that it was not clear to me how to use the `uuid_t` type and how pointers to it work. However, after finding out that it is actually an array I was able to work with them correctly.

The third major problem was with handling large files. My test code had to allocate the memory for the large file twice (to be able to write to the file system, then read from it and then check if the data is equal). On top of that the file system was also allocating a large block of memory for the data. This resulted in the OS terminating the program. The solution was to optimise the read and write code to not use large blocks of memory.

# Conclusion

I have implemented a simple file system that is able to store files and directories in an object database. The system uses indexed allocation to optimise file access for large files. There are reasonable limits applied to file sizes.

I found this practical quite challenging, however it was a good exercise in how file systems work and are implemented.
