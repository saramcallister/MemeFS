/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>
  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.
  gcc -Wall `pkg-config fuse --cflags --libs` fatfs.c -o fatfs
*/
/*
 * FAT filesystem variation used for MemeFS
 * Developed by Sara McAllister, Jake Palanker, and Gavin Yancey
 */
#define FUSE_USE_VERSION 26

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef linux
/* For pread()/pwrite() */
#define _XOPEN_SOURCE 500
#endif

// only available on linux, otherwise don't need.
#ifndef O_PATH
#define O_PATH 0
#endif

#define _GNU_SOURCE
#include <unistd.h>

#include <string.h>
#include <fuse.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <limits.h>


#include "blocklayer.h"

/*
  Filesystem layout:
  The superblock is the first block
  The fat starts at the second block
  and continues as long as is necessary.
  The root directory entry starts in the first block after the fat
  and is pointed to by the superblock.
*/

// 40 KB
#define BLOCK_SIZE BLOCKSIZE
#define FILES_PER_DIR ((BLOCK_SIZE - 4) / (sizeof(struct directory_entry)))
#define FAT_PER_BLOCK (BLOCK_SIZE / sizeof(fat_ptr))
#define MAX_NAME_LEN 32
#define MAX_PATH 4096

#define BLOCKS(x) (((x) + BLOCK_SIZE - 1) / BLOCK_SIZE)

typedef unsigned int fat_ptr;
// block 0 is the superblock,
// so we use a fat_ptr of 0 as a null pointer.
#define FAT_NULL 0

struct fat_superblock {
	fat_ptr free_list;
	fat_ptr root_dir;
	int free_blocks;
	size_t fat_size;
	fat_ptr fat_start;
	fat_ptr fat_end; // for faster append
};

struct directory_entry {
	off_t size;
	fat_ptr start;
	mode_t mode;
	char name[MAX_NAME_LEN];
};

struct directory {
	struct directory_entry files[FILES_PER_DIR];
	int count;
};

typedef union {
	struct directory d;
	char pad[BLOCK_SIZE];
} dir_block;

typedef union {
	struct fat_superblock s;
	char pad[BLOCK_SIZE];
} superblock;

typedef char fat_block[BLOCK_SIZE];

fat_ptr *fat;
superblock sb_data;
char *current_path;

static void read_block_fs(fat_ptr id, void *dest)
{
	int ret;

	if (id == FAT_NULL) {
		printf("fatal: trying to read invalid block\n");
		exit(1);
	}

	ret = read_block(id, (char*) dest);
	if (ret != 0) {
		printf("fatal: did not read entire block");
		exit(1);
	}
	printf("read_block_fs %x\n", id);
}

static void write_block_fs(fat_ptr id, const void *data)
{
	int ret;

	if (id == FAT_NULL) {
		printf("fatal: trying to write to invalid block\n");
		exit(1);
	}

	ret = write_block(id, (char*) data);
	if (ret != 0) {
		printf("fatal: did not write entire block");
		exit(1);
	}
	printf("write_block_fs %x\n", id);
}

static int read_superblock()
{
	int ret;

	ret = read_block(0, (char*) &sb_data);
	
	printf("read_superblock 0: root = %x, free = %x\n",
		sb_data.s.root_dir, sb_data.s.free_list);

	return ret;
}

static void write_superblock()
{
	int ret;

	ret = write_block(0, (char*) &sb_data);
	if (ret != 0) {
		printf("fatal: did not write entire superblock");
		exit(1);
	}
	printf("write_superblock 0: root = %x, free = %x\n",
		sb_data.s.root_dir, sb_data.s.free_list);
}

static void read_fat()
{
	fat_ptr next_block = sb_data.s.fat_start;
	fat_ptr *fat_pos = fat;
	fat_ptr *expected_pos = fat + sb_data.s.fat_size;
	while (fat_pos < expected_pos && next_block != FAT_NULL) {
		read_block_fs(next_block, fat_pos);
		next_block = fat[next_block];
		fat_pos += FAT_PER_BLOCK;
	}
	if (fat_pos != expected_pos) {
		fprintf(stderr, "Error: read_fat %srun by %ld entries\n",
			fat_pos > expected_pos? "over": "under", expected_pos - fat_pos);
	}
	printf("read_fat %lx: [%x, %x, %x, %x, %x, %x, %x, %x, ...]\n", sb_data.s.fat_size,
		fat[0], fat[1], fat[2], fat[3], fat[4], fat[5], fat[6], fat[7]);
}

static void write_fat()
{
	fat_ptr next_block = sb_data.s.fat_start;
	fat_ptr *fat_pos = fat;
	fat_ptr *expected_pos = fat + sb_data.s.fat_size;
	while (fat_pos < expected_pos && next_block != FAT_NULL) {
		write_block_fs(next_block, fat_pos);
		next_block = fat[next_block];
		fat_pos += FAT_PER_BLOCK;
	}
	if (fat_pos != expected_pos) {
		fprintf(stderr, "Error: write_fat %srun by %ld entries\n",
			fat_pos > expected_pos? "over": "under", expected_pos - fat_pos);
	}
	printf("write_fat %lx: [%x, %x, %x, %x, %x, %x, %x, %x, ...]\n", sb_data.s.fat_size,
		fat[0], fat[1], fat[2], fat[3], fat[4], fat[5], fat[6], fat[7]);
}

static void realloc_fat()
{
	// reallocate in-memory fat
	fat = reallocarray(fat, sb_data.s.fat_size + FAT_PER_BLOCK, sizeof(fat_ptr));
	if (fat == NULL) {
		fprintf(stderr, "Error allocating fat: %s\n", strerror(errno));
		exit(1);
	}
	memset(fat + sb_data.s.fat_size, FAT_NULL, FAT_PER_BLOCK * sizeof(fat_ptr));

	// allocate new fat block
	fat_ptr new_block_pos = allocate_block();

	// update previous last block to point to new block
	if (sb_data.s.fat_end != FAT_NULL) {
		fat[sb_data.s.fat_end] = new_block_pos;
	} else {
		sb_data.s.fat_start = new_block_pos;
	}

	// update superblock metadata
	sb_data.s.fat_end = new_block_pos;
	sb_data.s.fat_size += FAT_PER_BLOCK;
	sb_data.s.free_blocks += FAT_PER_BLOCK - 1;
}

// performs no error checking
static fat_ptr nth_block(fat_ptr start, int offset)
{
	int i;

	for(i = 0; i < offset; i++) {
		start = fat[start];
	}
	return start;
}

static fat_ptr last_block(fat_ptr start)
{
	while (fat[start] != FAT_NULL) start = fat[start];
	return start;
}

// assumes num_blocks are free
static fat_ptr alloc_blocks(int num_blocks)
{
	int i;
	fat_ptr new_block;

	// fat_ptr of 0 represents no blocks
	if (num_blocks < 1) return FAT_NULL;
	
	fat_ptr start = allocate_block();
	fat_ptr last_block = start;
	// start at 1 because we've already allocated a block
	for (i = 1; i < num_blocks; i++) {
		new_block = allocate_block();
		while (new_block >= sb_data.s.fat_size) {
			printf("FAT full; expanding!\n");
			realloc_fat();
		}
		fat[last_block] = new_block;
		last_block = new_block;
	}

	fat[last_block] = FAT_NULL;

	sb_data.s.free_blocks -= num_blocks;

	write_superblock();
	write_fat();

	return start;
}

// prepend to free_list
static void free_blocks(fat_ptr start)
{
	for (; start != FAT_NULL; start = fat[start]) {
		free_block(start);
		sb_data.s.free_blocks++;
	}

	write_fat();
	write_superblock();
}

static fat_ptr init_dir(struct directory_entry *de, struct directory_entry *parent_de)
{
	dir_block dir = {0};

	dir.d.count = 2;

	strncpy(dir.d.files[0].name, ".", MAX_NAME_LEN);
	dir.d.files[0].size = de->size;
	dir.d.files[0].mode = de->mode;
	dir.d.files[0].start = de->start;

	strncpy(dir.d.files[1].name, "..", MAX_NAME_LEN);
	dir.d.files[1].size = parent_de->size;
	dir.d.files[1].mode = parent_de->mode;
	dir.d.files[1].start = parent_de->start;

	write_block_fs(de->start, &dir);

	return de->start;
}

static const char *get_name(const char *path)
{
	const char *last_slash = strrchr(path, '/');
	if (last_slash == NULL) {
		fprintf(stderr, "Invalid path: %s", path);
		return NULL;
	}
	return last_slash+1;

}

static int find_in_dir(const char *name, int name_len,
                       fat_ptr *dir_id, dir_block *dir_data)
{
	if (name_len < 0) name_len = strlen(name);

	for(; *dir_id != FAT_NULL; *dir_id = fat[*dir_id]) {
		read_block_fs(*dir_id, dir_data);
		int i;
		for(i = 0; i < dir_data->d.count; i++) {
			if (strncmp(dir_data->d.files[i].name, name, name_len) == 0
					&& dir_data->d.files[i].name[name_len] == '\0') {
				return i;
			}
		}
	}

	return -1;
}

// returns pointer to parent dir or a null pointer for error and sets errno
// if de_out is NULL, last component of path is not evaluated
static fat_ptr resolve_path(const char *path, struct directory_entry *de_out)
{
	printf("resolve_path '%s'\n", path);
	fat_ptr parent_dir_id = sb_data.s.root_dir;
	fat_ptr cur_dir_id = sb_data.s.root_dir;
	struct directory_entry *current_de;
	dir_block cur_dir;

	if (path[0] != '/') {
		// require relative paths
		errno = EINVAL;
		return FAT_NULL;
	}

	if (strcmp(path, "/") == 0) {
		// for the root dir, we need to look up its entry from "/."
		path = "/.";
	}

	while (path != NULL) {
		path++;
		const char* rest_of_path = strchr(path, '/');
		if (de_out == NULL && rest_of_path == NULL) return cur_dir_id;

		int offset = find_in_dir(path, rest_of_path == NULL? -1: rest_of_path - path,
		                         &cur_dir_id, &cur_dir);
		if (offset < 0) {
			errno = ENOENT;
			return FAT_NULL;
		}

		current_de = &cur_dir.d.files[offset];
		parent_dir_id = cur_dir_id;
		cur_dir_id = current_de->start;

		if (!(current_de->mode & S_IFDIR) && rest_of_path != NULL) {
			errno = ENOTDIR;
			return FAT_NULL;
		}
		path = rest_of_path;
	}

	if (de_out != NULL) {
		memcpy(de_out, current_de, sizeof(struct directory_entry));
	}

	return parent_dir_id;
}

// rewrite the dir size everywhere it's specified
// which is a lot :(
static void modify_dir_size(fat_ptr dir_start, off_t size_delta)
{
	dir_block dir_data;
	dir_block subdir_data;
	read_block_fs(dir_start, &dir_data);

	// get size from "." entry (always first)
	off_t old_size = dir_data.d.files[0].size;
	off_t new_size = old_size + size_delta;

	// update "." entry
	dir_data.d.files[0].size = new_size;
	write_block_fs(dir_start, &dir_data);

	// update in parent dir
	fat_ptr parent_ptr = dir_data.d.files[1].start;
	for(; parent_ptr != FAT_NULL; parent_ptr = fat[parent_ptr]) {
		read_block_fs(parent_ptr, &subdir_data);
		int i;
		for(i = 0; i < subdir_data.d.count; i++) {
			if (subdir_data.d.files[i].start == dir_start) {
				// found it!
				if (subdir_data.d.files[i].size != old_size) {
					fprintf(stderr, "size_mismatch: is %lx, should be %lx\n",
						subdir_data.d.files[i].size, old_size);
				}
				subdir_data.d.files[i].size = new_size;
				write_block_fs(parent_ptr, &subdir_data);
				goto update_subdirs;
			}
		}
	}

	fprintf(stderr, "Error: couldn't find directory in parent");

	update_subdirs:;

	// update in all subdirs
	fat_ptr dir_ptr;
	// ignore "." and ".." on first page of dir
	int first_to_update = 2;
	for(dir_ptr = dir_start; dir_ptr != FAT_NULL; dir_ptr = fat[dir_ptr]) {
		read_block_fs(dir_ptr, &dir_data);
		int i;
		for(i = first_to_update; i < dir_data.d.count; i++) {
			if ((dir_data.d.files[i].mode & S_IFMT) == S_IFDIR) {
				read_block_fs(dir_data.d.files[i].start, &subdir_data);
				// update ".." (always second)
				if (subdir_data.d.files[1].size != old_size) {
					fprintf(stderr, "size_mismatch: is %lx, should be %lx\n",
						subdir_data.d.files[1].size, old_size);
				}
				subdir_data.d.files[1].size = new_size;
				write_block_fs(dir_data.d.files[i].start, &subdir_data);
			}
		}
		first_to_update = 0;
	}
}

static int make_file(const char *path, mode_t mode, off_t size, fat_ptr start)
{
	fat_ptr parent_ptr = resolve_path(path, NULL);
	if (parent_ptr == FAT_NULL) return -errno;

	const char *name = get_name(path);
	// max MAX_NAME_LEN chars for file name
	if (strlen(name) >= MAX_NAME_LEN) return -ENAMETOOLONG;

	dir_block parent;
	read_block_fs(parent_ptr, &parent);

	fat_ptr dir_start = parent_ptr;

	// find a block in the dir that has space
	while (parent.d.count == FILES_PER_DIR && fat[parent_ptr] != FAT_NULL) {
		parent_ptr = fat[parent_ptr];
		read_block_fs(parent_ptr, &parent);
	}

	// no space in the directory; add a block
	if (parent.d.count == FILES_PER_DIR) {
		// alloc could move fat
		// so this has to be on separate lines
		fat_ptr new_block = alloc_blocks(1);
		fat[parent_ptr] = new_block;
		write_fat();
		parent_ptr = fat[parent_ptr];
		parent.d.count = 0;

		modify_dir_size(dir_start, BLOCK_SIZE);
	}

	// create it
	struct directory_entry *entry = &parent.d.files[parent.d.count++];
	strncpy(entry->name, name, MAX_NAME_LEN);
	entry->size = size;
	entry->mode = mode;
	entry->start = start;

	// save parent de
	write_block_fs(parent_ptr, &parent);

	return 0;
}

static int fatfs_getattr(const char *path, struct stat *stbuf)
{
	memset(stbuf, 0, sizeof(struct stat));

	struct directory_entry de;
	fat_ptr err = resolve_path(path, &de);
	if (err == FAT_NULL) return -errno;

	stbuf->st_mode = de.mode;
	stbuf->st_size = de.size;
	return 0;
}

static int fatfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		       off_t offset, struct fuse_file_info *fi)
{
	struct directory_entry de;
	fat_ptr err = resolve_path(path, &de);
	if (err == FAT_NULL) return -errno;

	if (!(de.mode & S_IFDIR)) return -ENOTDIR;

	dir_block dir_data;
	fat_ptr dir_ptr;
	for(dir_ptr = de.start; dir_ptr != FAT_NULL; dir_ptr = fat[dir_ptr]) {
		read_block_fs(dir_ptr, &dir_data);

		int i;
		for (i = offset; i < dir_data.d.count; i++) {
			if (filler(buf, dir_data.d.files[i].name, NULL, 0) != 0) return 0;
		}
	}
	return 0;
}

static int fatfs_access(const char* path, int mask)
{
	struct directory_entry de;
	fat_ptr err = resolve_path(path, &de);
	if (err == FAT_NULL) return -errno;

	if (mask == F_OK) {
		return 0;
	} else {
		if ((mask & R_OK) && !(de.mode & S_IRUSR)) return -EACCES;
		if ((mask & W_OK) && !(de.mode & S_IWUSR)) return -EACCES;
		if ((mask & X_OK) && !(de.mode & S_IXUSR)) return -EACCES;
		return 0;
	}
}

static int fatfs_mkdir(const char* path, mode_t mode)
{
	fat_ptr dir_data = alloc_blocks(1);

	int err = make_file(path, S_IFDIR | mode, BLOCK_SIZE, dir_data);
	if (err < 0) return err;

	struct directory_entry de;
	fat_ptr parent_ptr = resolve_path(path, &de);
	if (parent_ptr == FAT_NULL) return -errno;

	dir_block parent_block;
	read_block_fs(parent_ptr, &parent_block);

	// do the rest and save
	init_dir(&de, &parent_block.d.files[0]);

	return 0;
}

static void* fatfs_init(struct fuse_conn_info *conn)
{
	int ret;
	printf("Current Path is %s\n", current_path);

	ret = block_dev_init(current_path);
	if (ret != 0) {
		fprintf(stderr, "Block device did not initialize. Returned %d\n", ret);
		exit(1);
	} 

	ret = read_superblock();
	if (ret != 0) {
		// fs doesn't exist; create it!
		
		// superblock allocation
		allocate_block();
		// we have a block allocated, but not FAT for it
		sb_data.s.free_blocks = -1;

		// fat allocation
		sb_data.s.fat_end = 0;
		sb_data.s.fat_size = 0;
		fat = NULL;
		realloc_fat();

		// init the root dir (with itself as its parent)
		// this writes to disk a lot of things
		struct directory_entry root_entry = {
			.name = "/",
			.size = BLOCK_SIZE,
			.mode = S_IFDIR | 0755,
			.start = alloc_blocks(1),
		};
		sb_data.s.root_dir = root_entry.start;

		init_dir(&root_entry, &root_entry);

		write_superblock();
	} else {
		fat = calloc(sizeof(fat_ptr), sb_data.s.fat_size);
		read_fat();
	}
	return NULL;
}

static void fatfs_destroy(void* private_data)
{
	free(current_path);
	free(fat);
	block_dev_destroy();
}

// part 2

static int fatfs_release(const char* path, struct fuse_file_info *fi)
{
	return 0;
}

static int fatfs_mknod(const char* path, mode_t mode, dev_t rdev)
{
	if (((mode & S_IFMT) & ~(S_IFREG | S_IFLNK)) != 0) {
		// man 2 mknod says this is the error for
		// fs doesn't support filetype
		return -EPERM;
	}

	return make_file(path, mode, 0, FAT_NULL);
}

static int fatfs_create(const char* path, mode_t mode, struct fuse_file_info* fi)
{
	// TODO: check for bad flags in fi
	return fatfs_mknod(path, mode | S_IFREG, 0);
}

static int fatfs_fgetattr(const char* path, struct stat* stbuf, struct fuse_file_info* fi)
{
	return fatfs_getattr(path, stbuf);
}

static int fatfs_open(const char* path, struct fuse_file_info* fi)
{
	if (fi->flags & O_ASYNC) {
		return -EINVAL; // TODO: support these?
	}

	if (fi->flags & O_PATH) {
		return fatfs_access(path, F_OK);
	} else if ((fi->flags & O_ACCMODE) == O_RDONLY) {
		return fatfs_access(path, R_OK);
	} else if ((fi->flags & O_ACCMODE) == O_WRONLY) {
		return fatfs_access(path, W_OK);
	} else if ((fi->flags & O_ACCMODE) == O_RDWR) {
		return fatfs_access(path, R_OK | W_OK);
	} else {
		return -EINVAL;
	}
}

static int fatfs_truncate(const char* path, off_t size)
{
	struct directory_entry de;
	fat_ptr parent_ptr = resolve_path(path, &de);
	if (parent_ptr == FAT_NULL) return -errno;

	// realloc blocks is needed
	if (BLOCKS(size) > BLOCKS(de.size)) {
		printf("%s: %ld blocks, needs %ld (alloc %ld)\n", path, BLOCKS(de.size), BLOCKS(size), BLOCKS(size) - BLOCKS(de.size));
		int blocks_to_alloc = BLOCKS(size) - BLOCKS(de.size);
		//if (blocks_to_alloc > sb_data.s.free_blocks) return -ENOSPC;

		if (de.start == FAT_NULL) {
			de.start = alloc_blocks(blocks_to_alloc);
		} else {
			// alloc could move fat
			// so this has to be on separate lines
			fat_ptr new_blocks = alloc_blocks(blocks_to_alloc);
			fat[last_block(de.start)] = new_blocks;
			write_fat();
		}
	} else if (BLOCKS(size) < BLOCKS(de.size)) {
		printf("%s: %ld blocks, needs %ld\n", path, BLOCKS(de.size), BLOCKS(size));
		int blocks_to_keep = BLOCKS(size);

		fat_ptr to_free;

		if (blocks_to_keep == 0) {
			to_free = de.start;
			de.start = FAT_NULL;
		} else {
			fat_ptr last_block = nth_block(de.start, blocks_to_keep-1);
			to_free = fat[last_block];
			fat[last_block] = -1;
		}

		free_blocks(to_free);
	}

	// update size
	de.size = size;

	// save de (with size and maybe new start)
	dir_block parent_dir;
	int offset = find_in_dir(get_name(path), -1, &parent_ptr, &parent_dir);
	if (offset < 0) return offset;
	memcpy(&parent_dir.d.files[offset], &de, sizeof(struct directory_entry));
	write_block_fs(parent_ptr, &parent_dir);

	return 0;
}

static int fatfs_read(const char* path, char *buf, size_t size, off_t offset, struct fuse_file_info* fi)
{
	struct directory_entry de;
	fat_ptr perr = resolve_path(path, &de);
	if (perr == FAT_NULL) return -errno;

	// check for EOF
	if (offset >= de.size) return 0;

	// check for hitting EOF
	if (offset + (off_t)size > de.size) size = de.size - offset;

	fat_ptr block_ptr = nth_block(de.start, offset / BLOCK_SIZE);
	fat_block data_block;

	// if unaligned read fits in one block, do it and return
	if ((offset % BLOCK_SIZE) + size < BLOCK_SIZE) {
		read_block_fs(block_ptr, &data_block);
		memcpy(buf, data_block + (offset % BLOCK_SIZE), size);
		return size;
	}

	// read first partial block
	size_t so_far = 0;
	if (offset % BLOCK_SIZE != 0) {
		so_far = BLOCK_SIZE - (offset % BLOCK_SIZE);

		read_block_fs(block_ptr, &data_block);
		memcpy(buf, data_block + (offset % BLOCK_SIZE), so_far);
		buf += so_far;

		block_ptr = fat[block_ptr];
	}

	// read all full blocks (if any)
	while((size - so_far) >= BLOCK_SIZE) {
		read_block_fs(block_ptr, buf);
		buf += BLOCK_SIZE;
		so_far += BLOCK_SIZE;

		block_ptr = fat[block_ptr];
	}

	// read last partial block
	if (size > so_far) {
		read_block_fs(block_ptr, &data_block);
		memcpy(buf, data_block, size - so_far);
	}

	return size;
}

static int fatfs_write(const char* path, const char *buf, size_t size, off_t offset, struct fuse_file_info* fi)
{
	struct directory_entry de;
	fat_ptr perr = resolve_path(path, &de);
	if (perr == FAT_NULL) return -errno;

	// check if we need to increase the file size
	if (offset + (off_t)size > de.size) {
		int err = fatfs_truncate(path, offset+size);
		if (err < 0) return err;

		// that might have changed the directory entry
		perr = resolve_path(path, &de);
		if (perr == FAT_NULL) return -errno;
	}

	fat_ptr block_ptr = nth_block(de.start, offset / BLOCK_SIZE);
	fat_block data_block;

	// if unaligned write fits in one block, do it and return
	if ((offset % BLOCK_SIZE) + size < BLOCK_SIZE) {
		read_block_fs(block_ptr, &data_block);
		memcpy(data_block + (offset % BLOCK_SIZE), buf, size);
		write_block_fs(block_ptr, &data_block);
		return size;
	}

	// if unaligned, write first partial block
	size_t so_far = 0;
	if (offset % BLOCK_SIZE != 0) {
		so_far = BLOCK_SIZE - (offset % BLOCK_SIZE);

		read_block_fs(block_ptr, &data_block);
		memcpy(data_block + (offset % BLOCK_SIZE), buf, so_far);
		write_block_fs(block_ptr, &data_block);
		buf += so_far;

		block_ptr = fat[block_ptr];
	}

	// write all full blocks (if any)
	while((size - so_far) >= BLOCK_SIZE) {
		write_block_fs(block_ptr, buf);
		buf += BLOCK_SIZE;
		so_far += BLOCK_SIZE;

		block_ptr = fat[block_ptr];
	}

	// write last partial block
	if (size > so_far) {
		read_block_fs(block_ptr, &data_block);
		memcpy(data_block, buf, size - so_far);
		write_block_fs(block_ptr, &data_block);
	}

	return size;
}

static int delete_de(const char* path)
{
	fat_ptr parent_ptr = resolve_path(path, NULL);
	if (parent_ptr == FAT_NULL) return -errno;

	dir_block parent_data;
	int offset = find_in_dir(get_name(path), -1, &parent_ptr, &parent_data);
	if (offset < 0) return offset;

	parent_data.d.count--;
	if (offset != parent_data.d.count) {
		memcpy(&parent_data.d.files[offset],
		       &parent_data.d.files[parent_data.d.count],
		       sizeof(struct directory_entry));
	}

	if (parent_data.d.count == 0) {
		// TODO: remove empty block in directory
	}

	write_block_fs(parent_ptr, &parent_data);

	return 0;
}

static int fatfs_unlink(const char* path)
{
	struct directory_entry de;
	fat_ptr err = resolve_path(path, &de);
	if (err == FAT_NULL) return -errno;

	free_blocks(de.start);

	return delete_de(path);
}

static int fatfs_rmdir(const char* path)
{
	// make sure it's empty
	struct directory_entry de;
	fat_ptr err = resolve_path(path, &de);
	if (err == FAT_NULL) return -errno;

	dir_block dir_data;
	read_block_fs(de.start, &dir_data);
	if (dir_data.d.count != 2) return -ENOTEMPTY;

	fat_ptr dir_ptr;
	for (dir_ptr = fat[de.start]; dir_ptr != FAT_NULL; dir_ptr = fat[dir_ptr]) {
		read_block_fs(dir_ptr, &dir_data);
		if (dir_data.d.count != 0) return -ENOTEMPTY;
	}

	return fatfs_unlink(path);
}

static int fatfs_symlink(const char* to, const char* from)
{
	fat_ptr data_block = alloc_blocks(1);

	// TODO: what permissions should it have?
	int err = make_file(from, S_IFLNK | 0600, BLOCK_SIZE, data_block);
	if (err < 0) return err;

	fat_block data;
	strncpy(data, to, BLOCK_SIZE);

	write_block_fs(data_block, &data);

	return 0;
}

static int fatfs_readlink(const char* path, char* buf, size_t size)
{
	struct directory_entry de;
	fat_ptr err = resolve_path(path, &de);
	if (err == FAT_NULL) return -errno;

	fat_block data;
	read_block_fs(de.start, &data);

	memcpy(buf, data, size > BLOCK_SIZE? BLOCK_SIZE: size);
	buf[size-1] = '\0';

	return 0;
}

static int fatfs_chmod(const char* path, mode_t mode)
{
	fat_ptr parent_id = resolve_path(path, NULL);
	if (parent_id == FAT_NULL) return -errno;

	dir_block dir_data;
	int offset = find_in_dir(get_name(path), -1, &parent_id, &dir_data);

	if (offset < 0) return -ENOENT;

	dir_data.d.files[offset].mode = (dir_data.d.files[offset].mode & S_IFMT) | mode;

	write_block_fs(parent_id, &dir_data);

	return 0;
}

static int fatfs_statfs(const char* path, struct statvfs* stbuf)
{
	memset(stbuf, 0, sizeof(struct statvfs));

	stbuf->f_bsize = BLOCK_SIZE;
	stbuf->f_frsize = BLOCK_SIZE;
	stbuf->f_blocks = sb_data.s.fat_size;
	stbuf->f_bfree = sb_data.s.free_blocks;
	stbuf->f_bavail = sb_data.s.free_blocks;

	// TODO: should we set these?
	stbuf->f_files = 0;
	stbuf->f_ffree = 0;

	stbuf->f_namemax = MAX_NAME_LEN;

	return 0; // TODO: implement
}

static int fatfs_rename(const char* from, const char* to)
{
	// get permissions
	struct directory_entry de;
	fat_ptr parent_ptr = resolve_path(from, &de);
	if (parent_ptr == FAT_NULL) return -errno;

	int err = make_file(to, de.mode, de.size, de.start);
	if (err < 0) return err;

	return delete_de(from);
}

static struct fuse_operations fatfs_oper = {
// part 1
	.getattr = fatfs_getattr,
	.readdir = fatfs_readdir,
	.access  = fatfs_access,
	.mkdir   = fatfs_mkdir,
// to make it work
	.init    = fatfs_init,
	.destroy = fatfs_destroy,
// part 2
	.release = fatfs_release,
	.create  = fatfs_create,
	.fgetattr = fatfs_fgetattr,
	.mknod    = fatfs_mknod,
	.open     = fatfs_open,
	.read     = fatfs_read,
	.readlink = fatfs_readlink,
	.rmdir    = fatfs_rmdir,
	.statfs   = fatfs_statfs,
	.symlink  = fatfs_symlink,
	.truncate = fatfs_truncate,
	.unlink   = fatfs_unlink,
	.write    = fatfs_write,
// extras
	.chmod    = fatfs_chmod,
	.rename   = fatfs_rename,
};

int main(int argc, char *argv[])
{
#ifdef __GLIBC__
	// this exists on glibc
	current_path = get_current_dir_name();
#else
	// fall back to POSIX
	// (fails if PATH_MAX not defined)
	current_path = malloc(PATH_MAX);
	getcwd(current_path, PATH_MAX);
#endif
	return fuse_main(argc, argv, &fatfs_oper, NULL);
}
