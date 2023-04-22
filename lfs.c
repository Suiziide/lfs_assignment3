#include <fuse.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h> 

#define FILENAME_SIZE 256
#define MAX_FILE_SIZE 4096

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

struct LinkedListNode *findTokenInCurrent(struct LinkedListNode *current, char *token);


static struct fuse_operations lfs_oper = {
    .getattr		= lfs_getattr,
    .readdir    	= lfs_readdir,
    .mknod 		= lsf_mknod,
    .mkdir 		= lfs_mkdir,
    .unlink 		= lsf_unlink,
    .rmdir 		= lsf_rmdir,
    .truncate 		= lsf_truncate,
    .open   		= lfs_open,
    .read   		= lfs_read,
    .release 		= lfs_release,
    .write 		= lsf_write,
    .rename 		= lsf_rename,
    .utime		= NULL
};

struct lfs_entry { 
    char name[FILENAME_SIZE];
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

struct LinkedList {
    struct LinkedListNode *head;
    struct LinkedListNode *tail;
    size_t num_entries;
};

struct LinkedListNode {
    struct LinkedListNode *next;
    struct LinkedListNode *prev;
    struct lfs_entry *entry;

};

struct LinkedListNode *root;

struct LinkedListNode *findEntry(const char *path){
    struct LinkedListNode *current = NULL;
    //find current node
    if(strcmp(path, "/") == 0){
        current = root;
    }else{
        char *token = strtok(path, "/");
        while(token != NULL) {
            current = findTokenInCurrent(current, token);
            token = strtok(NULL, "/");
        }
    }
    return current;
}

int lfs_getattr( const char *path, struct stat *stbuf ) {
    printf("hey");
    struct LinkedListNode *current = findEntry(path);
    printf("getattr: (path=%s)\n", path);   // Hvis vi mounter filsystemet med -f så printer den!
                                            // Hvis vi mounter med -d så får vi en debug prompt

    memset(stbuf, 0, sizeof(struct stat));
    if(strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
    } else if(current) {
        if (current->entry->isFile) {
            stbuf->st_mode = S_IFREG | 0777;
            stbuf->st_nlink = 1;
        } else {
            stbuf->st_mode = S_IFDIR | 0755;
            stbuf->st_nlink = 2;
        }
        stbuf->st_size = current->entry->size;
        stbuf->st_atime = current->entry->access_time;
        stbuf->st_mtime = current->entry->mod_time;
    } else {return -ENOENT;}
    return 0;
}

struct LinkedListNode *findTokenInCurrent(struct LinkedListNode *current, char *token) {
	struct LinkedList *subdirs = current->entry->subDirs;
    current = subdirs->head; // first element of linkedlist
	while(current != NULL && !strcmp(current->entry->name, token)) {
		current = current->next;
	}
	return current;
}	

int lfs_readdir( const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi ) {
//  (void) offset;
//  (void) fi;
    struct LinkedListNode *current = findEntry(path);
    printf("readdir: (path=%s)\n", path);

    if(strcmp(path, "/") != 0)
        return -ENOENT;

    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);

    if(current->entry->subDirs){
        struct LinkedListNode *dirToAdd = current->entry->subDirs->head;
        while(dirToAdd != NULL){
            filler(buf, dirToAdd->entry->name, NULL, 0);
            dirToAdd = dirToAdd->next;
        }
    }

    if(current->entry->files){
        struct LinkedListNode *fileToAdd = current->entry->files->head;
        while(fileToAdd != NULL){
            filler(buf, fileToAdd->entry->name, NULL, 0);
            fileToAdd = fileToAdd->next;
        }
    }
    return 0;
}

int lsf_mknod(const char *path, mode_t mode, dev_t device){
    return 0;
}

int lfs_mkdir(const char *path, mode_t mode) {
    char *token = strtok(path," /");
    bool seekOp = true;
    struct LinkedListNode *current = root; // Root skal være en linkedlistnode der indeholder root entry, det er lidt scuffed.
    char tmp[FILENAME_SIZE];
    while(token != NULL) {
        strncpy(tmp, token, sizeof(token));
        if (!(current = findTokenInCurrent(current, token))) {
            seekOp = false;
            token = strtok(NULL, "/");
            break;
        } 
        token = strtok(NULL, "/");
    }
    if (!seekOp && token == NULL) {
        // allocate new thing
        struct LinkedListNode *new_node = malloc(sizeof(*new_node));
        new_node->entry = malloc(sizeof(new_node->entry));
        //^^ Check for success FIX

        // set attributes of the directory
        // new_entry.name = tmp;
        strcpy(new_node->entry->name, tmp);
        // new_entry.size = SOMESIZE;
        new_node->entry->size = MAX_FILE_SIZE;
        // new_entry.isFile = false;
        new_node->entry->isFile = false;
        // new_entry.access_time = time(NULL);
        new_node->entry->access_time = time(NULL);
        // new_entry.mod_time = time(NULL);
        new_node->entry->mod_time = time(NULL);
        // new_entry.id = GENERATEID();
        new_node->entry->id = 1; //FIX FUNCTION
        // new_entry.files = NULL;
        new_node->entry->files = NULL;
        // new_entry.subDirs = NULL;
        new_node->entry->subDirs = NULL;

        //current.subdirlist->tail = malloc(sizeof(stru))

        // new_entry.prev = current.subdirlist.tail 
        new_node->prev = current->entry->subDirs->tail;
        // current.subdirlist.tail.next = new_entry
        new_node->prev->next = new_node; 
        // current.subdirlist.tail = new_entry
        current->entry->subDirs->tail = new_node;
        // current.size++;
        ++current->entry->subDirs->num_entries;
    } else {
        return -EEXIST;
    }
    return 0;
}

int lsf_unlink(const char *){
    return 0;
}

int lsf_rmdir(const char *path){
    return 0;
}

int lsf_truncate(const char *path, off_t offset){
    return 0;
}

int lsf_write(const char *, const char *, size_t, off_t, struct fuse_file_info *){
    return 0;
}

int lsf_rename(const char *from, const char *to){
    return 0;
}

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
    struct LinkedListNode *root = malloc(sizeof(*root));
    root->entry = malloc(sizeof(root->entry));
    root->entry->files = NULL;
    root->entry->subDirs = NULL;
    fuse_main( argc, argv, &lfs_oper );

    return 0;
}
