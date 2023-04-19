#include <fuse.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

int lfs_getattr( const char *, struct stat * );
int lfs_readdir( const char *, void *, fuse_fill_dir_t, off_t, struct fuse_file_info * );
int lsf_mknod(const char *path, mode_t mode, dev_t device);
int lfs_mkdir(const char *path, mode_t mode);
int lsf_unlink(const char *);
int lsf_rmdir(const char *path);
int lsf_truncate(const char *path, off_t offset);
int lfs_open( const char *, struct fuse_file_info * );
int lfs_read( const char *, char *, size_t, off_t, struct fuse_file_info * );
int lfs_release(const char *path, struct fuse_file_info *fi);
int lsf_write(const char *, const char *, size_t, off_t, struct fuse_file_info *);
int lsf_rename(const char *from, const char *to);

static struct fuse_operations lfs_oper = {
	.getattr	= lfs_getattr,
	.readdir	= lfs_readdir,
	.mknod = lsf_mknod,
	.mkdir = lfs_mkdir,
	.unlink = lsf_unlink,
	.rmdir = lsf_rmdir,
	.truncate = lsf_truncate,
	.open	= lfs_open,
	.read	= lfs_read,
	.release = lfs_release,
	.write = lsf_write,
	.rename = lsf_rename,
	.utime = NULL
};

int lfs_getattr( const char *path, struct stat *stbuf ) {
	int res = 0;
	printf("getattr: (path=%s)\n", path); 	// Hvis vi mounter filsystemet med -f så printer den!
											// Hvis vi mounter med -d så får vi en debug prompt

	memset(stbuf, 0, sizeof(struct stat));
	if( strcmp( path, "/" ) == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} else if( /* CHECK if file exist in file structure*/ ) {
		if (/* CHECK if file is a FILE */) {
			stbuf->st_mode = S_IFREG | 0777;
			stbuf->st_nlink = 1;
		} else if ( /* CHECK if file is a DIRECTORY */) {
			stbuf->st_mode = S_IFDIR | 0755;
			stbuf->st_nlink = 2;
		} else {
			res = -ENOENT;
		}
		// alt det her under er taget fra videoen, men det er nok ting stat structet skal bruge.
		// vi skal dog nok lige tjekke op på hvad det er :)
		// han bruger attributes som han har i sin found_file struct, og det har jeg ikke skrevet med
		// det skal vi nok også implementere
		stbuf->st_nlink = 1; 	// det her er i hans video, men jeg tror det er forkert. Jeg forklarer senere.
								// nå så ud til han opdagede fejlen, men rettede den ikke :D
		stbuf->st_size = /* filstørrelsen på den fundne fil */
		stbuf->st_uid = /* owner id */
		stbuf->st_gid = /* owner id */
		stbuf->st_atime = /* last access time */
		stbuf->st_mtime = /* last modified time */
		// free found file struct, hvis vi implementerer det?
	} else
		res = -ENOENT;

	return res;
}

int lfs_readdir( const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi ) {
//	(void) offset;
//	(void) fi;
	printf("readdir: (path=%s)\n", path);

	if(strcmp(path, "/") != 0)
		return -ENOENT;

	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);

	return 0;
}

//int lfs_mkdir(struct fuse_fs *fs, const char *path, mode_t mode){
//	
//}

//Permission
int lfs_open( const char *path, struct fuse_file_info *fi ) {
    printf("open: (path=%s)\n", path);

    // når vi åbner filen er det smart at gemme sin file info struct i fi->fh, da man så ikke behøver at søge
    // efter den hver gang man skal bruge den senere i read og write
	return 0;
}

int lfs_read( const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi ) {
    printf("read: (path=%s)\n", path);
	memcpy( buf, "Hello\n", 6 );
	return 6;
}

int lfs_release(const char *path, struct fuse_file_info *fi) {
	printf("release: (path=%s)\n", path);
	return 0;
}

int main( int argc, char *argv[] ) {
	fuse_main( argc, argv, &lfs_oper );

	return 0;
}
