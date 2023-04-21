#include <fuse.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>

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
    .getattr    = lfs_getattr,
    .readdir    = lfs_readdir,
    .mknod    = NULL, //     = lsf_mknod,
    .mkdir      = lfs_mkdir,
    .unlink   = NULL, //     = lsf_unlink,
    .rmdir    = NULL, //     = lsf_rmdir,
    .truncate = NULL, //     = lsf_truncate,
    .open       = lfs_open,
    .read       = lfs_read,
    .release    = lfs_release,
    .write     = NULL, //     = lsf_write,
    .rename      = NULL, //   = lsf_rename,
    .utime = NULL
};

static struct lfs_entry { 
    char name[256];
    char *parent;
    size_t size;
    bool isFile; 
    char *contents;     //file attribute (isFile)
    struct LinkedList *files;   //directory attribute (!isFile)
    struct LinkedList *subDirs; //directory attribute (!isFile)
    int id;                 //metadata
    u_int64_t access_time;  //metadata
    u_int64_t mod_time;     //metadata
};

static struct LinkedList {
    struct lfs_entry *head;
    struct lfs_entry *tail;
    size_t num_entries;
};

static struct LinkedListNode {
    struct lfs_entry *next;
    struct lfs_entry *prev;
};

int lfs_getattr( const char *path, struct stat *stbuf ) {
    struct lfs_entry *entry = findEntry(path);
    printf("getattr: (path=%s)\n", path);   // Hvis vi mounter filsystemet med -f så printer den!
                                            // Hvis vi mounter med -d så får vi en debug prompt

    memset(stbuf, 0, sizeof(struct stat));
    if(strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
    } else if(entry) {
        if (entry->isFile) {
            stbuf->st_mode = S_IFREG | 0777;
            stbuf->st_nlink = 1;
        } else {
            stbuf->st_mode = S_IFDIR | 0755;
            stbuf->st_nlink = 2;
        }
        stbuf->st_size = entry->size;
        stbuf->st_atime = entry->access_time;
        stbuf->st_mtime = entry->mod_time;
    } else {return -ENOENT;}
    return 0;
}

int lfs_readdir( const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi ) {
    //  (void) offset;
    //  (void) fi;
    printf("readdir: (path=%s)\n", path);

    if(strcmp(path, "/") != 0)
        return -ENOENT;

    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);

    return 0;
}

int lsf_mknod(const char *path, mode_t mode, dev_t device) { return 0; } //FIXME

int lfs_mkdir(const char *path, mode_t mode) { //FIXME
    char *token = strtok(path, "/");
    char *temp;
    struct lfs_entry *currentDir = findInList(root->subDirs, token);

    while (true) {
        temp = *token;
        if(currentDir == NULL) {
            if(token = strtok(NULL, "/") != NULL) {
                return -EEXIST;
            } else { break; }
        } else if (token = strtok(NULL, "/") != NULL) {
            currentDir = findInList(currentDir->subDirs, token);
        }
    }

    if(currentDir == NULL && token == NULL && temp != NULL) {
        //mkdir(temp) FIXME
        return 0;
    }

    return 0;
}

int lsf_unlink(const char *) { return 0; } //FIXME

int lsf_rmdir(const char *path) { return 0; } //FIXME

int lsf_truncate(const char *path, off_t offset) { return 0; } //FIXME



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

int lsf_write(const char *, const char *, size_t, off_t, struct fuse_file_info *) { return 0; } //FIXME

int lsf_rename(const char *from, const char *to) { return 0; } //FIXME


struct *lfs_entry findEntry(path) { return (struct lfs_entry *) NULL; } //FIXME
char *getPath() { return (char *) NULL; } //FIXME

int main( int argc, char *argv[] ) {
    fuse_main( argc, argv, &lfs_oper );

    return 0;
}
// FIXME: TEST PATH INTERPRETATION 
