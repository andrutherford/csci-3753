/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  Minor modifications and note by Andy Sayler (2012) <www.andysayler.com>

  Source: fuse-2.8.7.tar.gz examples directory
  http://sourceforge.net/projects/fuse/files/fuse-2.X/

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.

  gcc -Wall `pkg-config fuse --cflags` fusexmp.c -o fusexmp `pkg-config fuse --libs`

  Note: This implementation is largely stateless and does not maintain
        open file handels between open and release calls (fi->fh).
        Instead, files are opened and closed as necessary inside read(), write(),
        etc calls. As such, the functions that rely on maintaining file handles are
        not implmented (fgetattr(), etc). Those seeking a more efficient and
        more complete implementation may wish to add fi->fh support to minimize
        open() and close() calls and support fh dependent functions.

*/

#define FUSE_USE_VERSION 28
#define HAVE_SETXATTR

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef linux
/* For pread()/pwrite() */
#define _XOPEN_SOURCE 500
#endif

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <sys/time.h>
#include <limits.h>
#include <stdlib.h>
#include "aes-crypt.h"
#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif

//Was having issues with these in a struct, used global variables instead
char* ROOT_PATH = "";
char* PASSWORD = "";

//Allow for custom mirror point path 
char* get_path(const char* path)
{
    int length = strlen(ROOT_PATH) + strlen(path) + 1;

    //Allocate enough space for full path
    char* full_path = malloc(length * sizeof(char));

    strcpy(full_path, ROOT_PATH);
    strcat(full_path, path);
    return full_path;
}

static int xmp_getattr(const char *short_path, struct stat *stbuf)
{
    char* path = get_path(short_path);
	int res;

	res = lstat(path, stbuf);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_access(const char *short_path, int mask)
{
    char* path = get_path(short_path);
	int res;

	res = access(path, mask);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_readlink(const char *short_path, char *buf, size_t size)
{
    char* path = get_path(short_path);
	int res;

	res = readlink(path, buf, size - 1);
	if (res == -1)
		return -errno;

	buf[res] = '\0';
	return 0;
}


static int xmp_readdir(const char *short_path, void *buf, fuse_fill_dir_t filler,
		       off_t offset, struct fuse_file_info *fi)
{
    char* path = get_path(short_path);
	DIR *dp;
	struct dirent *de;

	(void) offset;
	(void) fi;

	dp = opendir(path);
	if (dp == NULL)
		return -errno;

	while ((de = readdir(dp)) != NULL) {
		struct stat st;
		memset(&st, 0, sizeof(st));
		st.st_ino = de->d_ino;
		st.st_mode = de->d_type << 12;
		if (filler(buf, de->d_name, &st, 0))
			break;
	}

	closedir(dp);
	return 0;
}

static int xmp_mknod(const char *short_path, mode_t mode, dev_t rdev)
{
    char* path = get_path(short_path);
	int res;

	/* On Linux this could just be 'mknod(path, mode, rdev)' but this
	   is more portable */
	if (S_ISREG(mode)) {
		res = open(path, O_CREAT | O_EXCL | O_WRONLY, mode);
		if (res >= 0)
			res = close(res);
	} else if (S_ISFIFO(mode))
		res = mkfifo(path, mode);
	else
		res = mknod(path, mode, rdev);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_mkdir(const char *short_path, mode_t mode)
{
    char* path = get_path(short_path);
	int res;

	res = mkdir(path, mode);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_unlink(const char *short_path)
{
    char* path = get_path(short_path);
	int res;

	res = unlink(path);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_rmdir(const char *short_path)
{
    char* path = get_path(short_path);
	int res;

	res = rmdir(path);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_symlink(const char *from, const char *to)
{
	int res;

	res = symlink(from, to);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_rename(const char *from, const char *to)
{
	int res;

	res = rename(from, to);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_link(const char *from, const char *to)
{
	int res;

	res = link(from, to);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_chmod(const char *short_path, mode_t mode)
{
    char* path = get_path(short_path);
	int res;

	res = chmod(path, mode);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_chown(const char *short_path, uid_t uid, gid_t gid)
{
    char* path = get_path(short_path);
	int res;

	res = lchown(path, uid, gid);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_truncate(const char *short_path, off_t size)
{
    char* path = get_path(short_path);
	int res;

	res = truncate(path, size);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_utimens(const char *short_path, const struct timespec ts[2])
{
    char* path = get_path(short_path);
	int res;
	struct timeval tv[2];

	tv[0].tv_sec = ts[0].tv_sec;
	tv[0].tv_usec = ts[0].tv_nsec / 1000;
	tv[1].tv_sec = ts[1].tv_sec;
	tv[1].tv_usec = ts[1].tv_nsec / 1000;

	res = utimes(path, tv);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_open(const char *short_path, struct fuse_file_info *fi)
{
    char* path = get_path(short_path);
	int res;

	res = open(path, fi->flags);
	if (res == -1) return -errno;

	close(res);
	return 0;
}

static inline int f_size(FILE *file){
    struct stat stats;

    if(fstat(fileno(file), &stats))
    {
        return -1;
    }
    return stats.st_size;
}


//Return whether or not file has an encrypted extended attribute
int xmp_getxattr_encrypted(const char* path){
    char xattr[5];
    getxattr(path, "user.pa5-encfs.encrypted", xattr, sizeof(char)*5);
    return strcmp("true", xattr) == 0;

}

static int xmp_read(const char *short_path, char *buf, size_t size, off_t offset,
		    struct fuse_file_info *fi)
{
	char* path = get_path(short_path);
	int res;
 	FILE *f;
 	FILE *tmp;

 	(void) fi;

    //Open file for read only
    f = fopen(path, "r");

    //Create temp file
    tmp = tmpfile();

    //Default flag set to -1 (ignore). 0 = Decrypt, 1 = Encrypt
    int flag = -1;

    //If extended attribute is encrypted, must decrypt
    if (xmp_getxattr_encrypted(path))
        flag = 0;

    //If do_crypt fails, file is null or temp file is null return error
    if (do_crypt(f, tmp, flag, PASSWORD) == 0 || f == NULL || tmp == NULL)
    {
        return -errno;
    }

   	//Flush temp file
    fflush(tmp);

    //Move pointer to new offset
    fseek(tmp, offset, SEEK_SET);

    //Read out file from temp file
	res = fread(buf, 1, size, tmp);
	if (res == -1)
		res = -errno;

	//Close file and temp file
    fclose(f);
    fclose(tmp);
    

	return res;
}

static int xmp_write(const char *short_path, const char *buf, size_t size,
		     off_t offset, struct fuse_file_info *fi)
{
    char* path = get_path(short_path);
	int res;
	FILE *f;
	FILE *tmp;

	(void) fi;

	//Open file
    f = fopen(path, "r+");

    //Create temp file
    tmp = tmpfile();

    int tmp_num = fileno(tmp);

    //Default flag set to -1 (ignore). 0 = Decrypt, 1 = Encrypt
    int flag = -1;

    if(xmp_access(short_path, R_OK) == 0 && f_size(f) > 0)
    {
        if (xmp_getxattr_encrypted(path))
        {
        	//Must decrypt
            flag = 0;
        }

        //If do_crypt fails, return error
        if (do_crypt(f, tmp, flag, PASSWORD) == 0)
        {
        	return errno;
        }

        // Reset files to beginning
        rewind(f);
        rewind(tmp);
    }


	res = pwrite(tmp_num, buf, size, offset);
	if (res == -1)
		res = -errno;

    //Re-encrypt file

	//Reset flag to ignore
    flag = -1;
    if (xmp_getxattr_encrypted(path))
    	//Must encrypt
        flag = 1;

    //If do_crypt fails return error
    if(do_crypt(tmp, f, flag, PASSWORD) == 0)
    {
    	return errno;
    }

    //Close file and temp file
    fclose(f);
    fclose(tmp);

	return res;
}

static int xmp_statfs(const char *short_path, struct statvfs *stbuf)
{
    char* path = get_path(short_path);
	int res;

	res = statvfs(path, stbuf);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_create(const char* short_path, mode_t mode, struct fuse_file_info* fi) {

    char* path = get_path(short_path);
    (void) fi;

    int res;
    res = creat(path, mode);
    if(res == -1)
	return -errno;

    close(res);

    //Setting name of new attribute user.pa5-encfs.encrypted with value of true
    setxattr(path, "user.pa5-encfs.encrypted", "true", (sizeof(char) * 5), 0);

    return 0;
}


static int xmp_release(const char *short_path, struct fuse_file_info *fi)
{
	/* Just a stub.	 This method is optional and can safely be left
	   unimplemented */
    char* path = get_path(short_path);

	(void) path;
	(void) fi;
	return 0;
}

static int xmp_fsync(const char *short_path, int isdatasync,
		     struct fuse_file_info *fi)
{
	/* Just a stub.	 This method is optional and can safely be left
	   unimplemented */
    char* path = get_path(short_path);

	(void) path;
	(void) isdatasync;
	(void) fi;
	return 0;
}

#ifdef HAVE_SETXATTR
static int xmp_setxattr(const char *short_path, const char *name, const char *value,
			size_t size, int flags)
{
    char* path = get_path(short_path);
	int res = lsetxattr(path, name, value, size, flags);
	if (res == -1)
		return -errno;
	return 0;
}

static int xmp_getxattr(const char *short_path, const char *name, char *value,
			size_t size)
{
    char* path = get_path(short_path);
	int res = lgetxattr(path, name, value, size);
	if (res == -1)
		return -errno;
	return res;
}

static int xmp_listxattr(const char *short_path, char *list, size_t size)
{
    char* path = get_path(short_path);
	int res = llistxattr(path, list, size);
	if (res == -1)
		return -errno;
	return res;
}

static int xmp_removexattr(const char *short_path, const char *name)
{
    char* path = get_path(short_path);
	int res = lremovexattr(path, name);
	if (res == -1)
		return -errno;
	return 0;
}
#endif /* HAVE_SETXATTR */

static struct fuse_operations xmp_oper = {
	.getattr	= xmp_getattr,
	.access		= xmp_access,
	.readlink	= xmp_readlink,
	.readdir	= xmp_readdir,
	.mknod		= xmp_mknod,
	.mkdir		= xmp_mkdir,
	.symlink	= xmp_symlink,
	.unlink		= xmp_unlink,
	.rmdir		= xmp_rmdir,
	.rename		= xmp_rename,
	.link		= xmp_link,
	.chmod		= xmp_chmod,
	.chown		= xmp_chown,
	.truncate	= xmp_truncate,
	.utimens	= xmp_utimens,
	.open		= xmp_open,
	.read		= xmp_read,
	.write		= xmp_write,
	.statfs		= xmp_statfs,
	.create         = xmp_create,
	.release	= xmp_release,
	.fsync		= xmp_fsync,
#ifdef HAVE_SETXATTR
	.setxattr	= xmp_setxattr,
	.getxattr	= xmp_getxattr,
	.listxattr	= xmp_listxattr,
	.removexattr	= xmp_removexattr,
#endif
};

int main(int argc, char *argv[])
{
	umask(0);

	//Throw an error if not enough arguments passed in
	if (argc < 4)
	{
		fprintf(stderr, "usage: %s %s\n", argv[0], "Missing: <Pass Phrase> <Mirror Directory> <Mount Point>");
		return 1;
	}

    // Set password 
    PASSWORD = argv[argc-3];
    //Set root path
    ROOT_PATH = realpath(argv[argc - 2], NULL);

    //Extract password and mirror directory from input arguments, fuse doesn't care about these
	return fuse_main(argc - 2, argv + 2, &xmp_oper, NULL);
}