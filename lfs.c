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
    .getattr    = lfs_getattr,
    .readdir    = lfs_readdir,
    .mkdir 	    = lfs_mkdir,
    .rmdir 	    = lsf_rmdir,
    .mknod      = lsf_mknod,
    .unlink     = lsf_unlink,
    .truncate   = lsf_truncate,
    .open       = lfs_open,
    .read   	= lfs_read,
    .release 	= lfs_release,
    .write 	    = lsf_write,
    .rename 	= lsf_rename,
    .utime      = NULL
};

struct lfs_entry { 
    char name[FILENAME_SIZE];
    struct LinkedListNode *parent;
    size_t size;
    bool isFile; 
    char *contents;     //file attribute (isFile)
    struct LinkedList *entries; //directory attribute (!isFile)
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

struct LinkedListNode *findEntry(const char *path) {
    printf("----------find-entry----------\n");
    printf("path is: %s\n", path);
    struct LinkedListNode *current = root; // starts at root
    size_t len = strlen(path) + 1;
    char *current_path = malloc(len);
    strncpy(current_path, path, len);

    if (strcmp(path, "/") == 0) { 
        printf("given path is root\n");
        printf("----------find-entry----------\n");
        return root;
    }
    
    char *token = strtok(current_path, "/");
    printf("Token: %s\n", token);
    printf("Current name: %s\n", current->entry->name);

    while (token != NULL) {
        current = findTokenInCurrent(current, token);
        token = strtok(NULL, "/");
        printf("Token: %s\n", token);
        if (current) {
            printf("Current name: %s\n", current->entry->name);
        }
    }
    
    free(current_path);
    if (!current) { printf("Current does not exist as an entry\n"); }
    printf("----------find-entry----------\n");
    return current;
}

int lfs_getattr(const char *path, struct stat *stbuf) {
    printf("----------getattr----------\n");
    printf("path is: %s\n", path);
    struct LinkedListNode *current = findEntry(path);
    if (current == NULL) { 
        printf("Current was not found as entry\n");
        return -ENOENT;
    }
    printf("Current name: %s\n", current->entry->name);
    
    memset(stbuf, 0, sizeof(struct stat));
    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
    } else if (current) {
        if (current->entry->isFile) {
            stbuf->st_mode = S_IFREG | 0777;
            stbuf->st_nlink = 1;
            stbuf->st_size = current->entry->size;
        } else {
            stbuf->st_mode = S_IFDIR | 0755;
            stbuf->st_nlink = 2; // plus numbers of subdirs??
        }
        stbuf->st_atime = current->entry->access_time;
        stbuf->st_mtime = current->entry->mod_time;
    } else { return -ENOENT; }
    printf("----------getattr----------\n");
    return 0;
}

struct LinkedListNode *findTokenInCurrent(struct LinkedListNode *current, char *token) {
    printf("----------find-token----------\n");
    printf("Current name: %s\n", current->entry->name);
	current = current->entry->entries->head;

    if (current == NULL) { 
        printf("Current does not exist\n");
        printf("----------find-token----------\n");
        return NULL;
    }

	while (current != NULL && strcmp(current->entry->name, token) != 0) {
        printf("Token: %s\n", token);
        printf("Current name: %s\n", current->entry->name);
        current = current->next;
	}

    if (!current) { printf("Token not found current == NULL\n"); }
    printf("----------find-token----------\n");
	return current;
}	

int lfs_readdir( const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi ) {
    //  (void) offset;
    //  (void) fi;
    printf("---------readdir----------\n");
    struct LinkedListNode *current;
    if (!(current = findEntry(path))) { 
        printf("---------readdir----------\n");
        return -ENOENT; 
    } // Exits if no entry

    printf("readdir: (path=%s)\n", path);
    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);

    if (current->entry->entries) {
        struct LinkedListNode *entryToAdd = current->entry->entries->head;
        while (entryToAdd != NULL) {
            printf("displaying this dir: %s\n", entryToAdd->entry->name);
            filler(buf, entryToAdd->entry->name, NULL, 0);
            entryToAdd = entryToAdd->next;
        }
    }

    printf("---------readdir----------\n");
    return 0;
}

int lfs_mkdir(const char *path, mode_t mode) {
    size_t len = strlen(path) + 1;
    char *current_path = malloc(len);
    strncpy(current_path, path, len);
    bool seekOp = true;
    struct LinkedListNode *current = root;
    char tmp[FILENAME_SIZE];
    struct LinkedListNode *parent;
    char *token = strtok(current_path, "/");

    while (token != NULL) {
        strncpy(tmp, token, strlen(token) + 1);
        parent = current;
        // printf("This is token: %s\n", token);
        if (!(current = findTokenInCurrent(current, token))) {
            seekOp = false;
            token = strtok(NULL, "/");
           // printf("this is the new token: %s\n", token);
            break;
        } 
        token = strtok(NULL, "/");
    }
    //printf("Done seeking\n");
    //printf("Token is: %s\n", token);
    //printf("tmp is: %s\n", tmp);
    //printf("parent is: %s\n", parent->entry->name);
    if (!seekOp && token == NULL) {
        //printf("Trying to make a dir...\n");
        // allocate new thing
        struct LinkedListNode *new_node = malloc(sizeof(struct LinkedListNode));
        new_node->entry = malloc(sizeof(struct lfs_entry));
        //^^ Check for success FIX
        // set attributes of the directory
        strcpy(new_node->entry->name, tmp);
        new_node->entry->size = MAX_FILE_SIZE;
        new_node->entry->isFile = false;
        new_node->entry->access_time = time(NULL);
        new_node->entry->mod_time = time(NULL);
        new_node->entry->id = 42; //FIX FUNCTION
        new_node->entry->entries = malloc(sizeof(struct LinkedList));
        new_node->entry->entries->head = NULL;
        new_node->entry->entries->tail = NULL;
        new_node->entry->entries->num_entries = 0;
        new_node->entry->parent = parent;

        new_node->prev = parent->entry->entries->tail;
        if (new_node->prev) {
            new_node->prev->next = new_node; // actually parent->entry->subdirs->tail->next
        }
        parent->entry->entries->tail = new_node;
        if (!parent->entry->entries->head) {
            parent->entry->entries->head = new_node;
        }
        ++parent->entry->entries->num_entries;
    } else {
        free(current_path);
        return -EEXIST;
    }
    //printf("made the node\n");
    free(current_path);
    return 0;
}

int lsf_rmdir(const char *path){
    struct LinkedListNode *current = findEntry(path);
    if (!current) { return -ENOENT; }
    if (current->entry->entries->num_entries != 0) { return -ENOTEMPTY; } // if there are files or dirs don't remove

    struct LinkedListNode *parent = current->entry->parent;

    if (parent->entry->entries->num_entries == 1) { // current == head == tail
        parent->entry->entries->head = NULL;
        parent->entry->entries->tail = NULL;
    } else if (current == parent->entry->entries->head) { // current == head and more elements 
        parent->entry->entries->head = current->next;
        current->next->prev = NULL;
    } else if (current == parent->entry->entries->tail) { // current == tail and previous elements
        parent->entry->entries->tail = current->prev;
        current->prev->next = NULL;
    } else {
        current->prev->next = current->next;
        current->next->prev = current->prev;
    }
    
    free(current->entry->entries->head);
    free(current->entry->entries->tail);
    free(current->entry->entries);
    free(current->entry);
    free(current);

    --parent->entry->entries->num_entries;    
    return 0;
}

int lsf_mknod(const char *path, mode_t mode, dev_t device) { 
    // s
    size_t len = strlen(path) + 1;
    char *current_path = malloc(len);
    strncpy(current_path, path, len);
    bool seekOp = true;
    struct LinkedListNode *current = root;
    char tmp[FILENAME_SIZE];
    struct LinkedListNode *parent;
    char *token = strtok(current_path, "/");

    while (token != NULL) {
        strncpy(tmp, token, strlen(token) + 1);
        parent = current;
        if (!(current = findTokenInCurrent(current, token))) {
            seekOp = false;
            token = strtok(NULL, "/");
            break;
        }
        token = strtok(NULL, "/");
    }
    
    if (!seekOp && token == NULL) {
        struct LinkedListNode *new_node = malloc(sizeof(struct LinkedListNode));
        new_node->entry = malloc(sizeof(struct lfs_entry));
        if (!new_node->entry) { return -EFAULT; }
        // set attributes of the directory
        strcpy(new_node->entry->name, tmp);
        new_node->entry->size = MAX_FILE_SIZE; //FIXME
        new_node->entry->isFile = true;
        new_node->entry->access_time = time(NULL);
        new_node->entry->mod_time = time(NULL);
        new_node->entry->id = 69; //FIXME - FUNCTION
        new_node->entry->entries = NULL;
        new_node->entry->contents = malloc(MAX_FILE_SIZE); // fixme?
        memset(new_node->entry->contents, 's', 100); //FIXME

        new_node->prev = parent->entry->entries->tail;
        if (new_node->prev) {
            new_node->prev->next = new_node; // actually parent->entry->subdirs->tail->next
        }
        parent->entry->entries->tail = new_node;
        if (!parent->entry->entries->head) {
            parent->entry->entries->head = new_node;
        }
        ++parent->entry->entries->num_entries;
    } else {
        free(current_path);
        return -EEXIST;
    }
    free(current_path);
    return 0;
}

int lsf_unlink(const char *path){
    return 0;
}


int lsf_truncate(const char *path, off_t offset){
    return 0;
}

int lsf_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi){
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
    struct LinkedListNode *file = findEntry(path);
    memcpy(buf, file->entry->contents, MAX_FILE_SIZE); // FIXME
    return 100; // FIXME to whetever is actually read
}

int lfs_release(const char *path, struct fuse_file_info *fi) {
    printf("release: (path=%s)\n", path);
    return 0;
}

int main( int argc, char *argv[] ) {
    root = malloc(sizeof(struct LinkedListNode));
    root->entry = malloc(sizeof(struct lfs_entry));
        strcpy(root->entry->name, "/");
        root->entry->size = MAX_FILE_SIZE;
        root->entry->isFile = false;
        root->entry->access_time = time(NULL);
        root->entry->mod_time = time(NULL);
        root->entry->id = 1; //FIXME FUNCTION
        root->entry->entries = malloc(sizeof(struct LinkedList));
        root->entry->entries->head = NULL;
        root->entry->entries->tail = NULL;
        root->entry->entries->num_entries = 0;

    fuse_main( argc, argv, &lfs_oper );

    return 0;
}
