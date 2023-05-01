#include <fuse.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h> 

#define FILENAME_SIZE 256

int lfs_getattr( const char *, struct stat * );
int lfs_readdir( const char *, void *, fuse_fill_dir_t, off_t, struct fuse_file_info * );
int lfs_mknod(const char *path, mode_t mode, dev_t device);
int lfs_mkdir(const char *path, mode_t mode);
int lfs_unlink(const char *);
int lfs_rmdir(const char *path);
int lfs_truncate(const char *path, off_t offset);
int lfs_open( const char *, struct fuse_file_info * );
int lfs_read( const char *, char *, size_t, off_t, struct fuse_file_info * );
int lfs_release(const char *path, struct fuse_file_info *fi);
int lfs_write(const char *, const char *, size_t, off_t, struct fuse_file_info *);
int lfs_rename(const char *from, const char *to);
int lfs_utime(const char *filename, const struct utimbuf *times);
struct LinkedListNode *findTokenInCurrent(struct LinkedListNode *current, char *token);


static struct fuse_operations lfs_oper = {
    .getattr    = lfs_getattr,
    .readdir    = lfs_readdir,
    .mkdir 	    = lfs_mkdir,
    .rmdir 	    = lfs_rmdir,
    .mknod      = lfs_mknod,
    .unlink     = lfs_unlink,
    .truncate   = lfs_truncate,
    .open       = lfs_open,
    .read   	= lfs_read,
    .release 	= lfs_release,
    .write 	    = lfs_write,
    .rename 	= lfs_rename,
    .utime      = lfs_utime
};

struct lfs_entry { 
    char name[FILENAME_SIZE];
    struct LinkedListNode *parent;
    size_t size;
    bool isFile; 
    char *contents;     //file attribute (isFile)
    struct LinkedList *entries; //directory attribute (!isFile)
    int id;                 //metadata
    u_int64_t actime;  //metadata
    u_int64_t modtime;     //metadata
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

// global variables 
struct LinkedListNode *root;
int CURRENT_ID = 0;

// auxiliary methods
int generateId() {
    return CURRENT_ID++;
}

struct LinkedListNode *findEntry(const char *path) {
    // printf("----------find-entry----------\n");
    // printf("path is: %s\n", path);
    struct LinkedListNode *current = root; // starts at root
    size_t len = strlen(path) + 1;
    char *current_path = malloc(len);
    strncpy(current_path, path, len);

    if (strcmp(path, "/") == 0) { 
      //   printf("given path is root\n");
      //   printf("----------find-entry----------\n");
        return root;
    }

    char *token = strtok(current_path, "/");
    // printf("Token: %s\n", token);
    // printf("Current name: %s\n", current->entry->name);

    while (token != NULL) {
        current = findTokenInCurrent(current, token);
        token = strtok(NULL, "/");
    //     printf("Token: %s\n", token);
    //     if (current) {
    //         printf("Current name: %s\n", current->entry->name);
    //     }
    }

    free(current_path);
    // if (!current) { printf("Current does not exist as an entry\n"); }
    // printf("----------find-entry----------\n");
    return current;
}

struct LinkedListNode *findTokenInCurrent(struct LinkedListNode *current, char *token) {
    // printf("----------find-token----------\n");
    // printf("Current name: %s\n", current->entry->name);
    current = current->entry->entries->head;

    if (current == NULL) { 
    //     printf("Current does not exist\n");
    //     printf("----------find-token----------\n");
        return NULL;
    }

    while (current != NULL && strcmp(current->entry->name, token) != 0) {
        // printf("Token: %s\n", token);
        // printf("Current name: %s\n", current->entry->name);
        current = current->next;
    }

    // if (!current) { printf("Token not found current == NULL\n"); }
    // printf("----------find-token----------\n");
    return current;
}	

int makeEntry(const char *path, bool isFile) {
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
        if (!new_node) { return -EFAULT; }
        new_node->entry = malloc(sizeof(struct lfs_entry));
        if (!new_node->entry) { return -EFAULT; }
        
        // set attributes of the entry
        strcpy(new_node->entry->name, tmp);
        new_node->entry->size = 0;
        new_node->entry->isFile = isFile;
        new_node->entry->actime = time(NULL);
        new_node->entry->modtime = time(NULL);
        new_node->entry->id = generateId();
        new_node->entry->parent = parent;
        if (!isFile) {
            new_node->entry->entries = malloc(sizeof(struct LinkedList));
            if (new_node->entry->entries == NULL)  { return -EFAULT; }
            new_node->entry->entries->head = NULL;
            new_node->entry->entries->tail = NULL;
            new_node->entry->entries->num_entries = 0;
        } else {
            new_node->entry->contents = NULL;
        }
        
        new_node->next = NULL;
        new_node->prev = parent->entry->entries->tail;
        if (new_node->prev) {
            new_node->prev->next = new_node;
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

    

    
    // save to disk FIXME
    
    return 0;
}

int removeEntry(struct LinkedListNode *current) {
    //printf("----------remove-entry----------\n");
    if (!current) {
      //  printf("Current does not exist\n"); 
      //  printf("-------remove-entry---------\n");
        return -ENOENT; 
    }
    if (!current->entry->isFile && current->entry->entries != NULL && current->entry->entries->num_entries != 0) {
        //printf("Folder not empty\n"); 
        //printf("-------remove-entry---------\n");
        return -ENOTEMPTY; 
    } // if there are files or dirs don't remove

    struct LinkedListNode *parent = current->entry->parent;
    //printf("TRYING TO DO SOME REARANGEMENTS!?!?!\n");
    if (parent->entry->entries->head == current && parent->entry->entries->tail == current) { // current == head == tail
        //printf("first\n");
        parent->entry->entries->head = NULL;
        parent->entry->entries->tail = NULL;
    } else if (current == parent->entry->entries->head) { // current == head and more elements 
        //printf("second\n");
        parent->entry->entries->head = current->next;
        current->next->prev = NULL;
    } else if (current == parent->entry->entries->tail) { // current == tail and previous elements
        //printf("thrid\n");
        parent->entry->entries->tail = current->prev;
        current->prev->next = NULL;
    } else {
      //  printf("fourth\n");
        current->prev->next = current->next;
        current->next->prev = current->prev;
    }

    --parent->entry->entries->num_entries;
    //printf("----------remove-entry----------\n");
    return 0;
}

int rmThisEntry(struct LinkedListNode *current) {
    int result = removeEntry(current);
    updateDirSizesToRoot(current->entry->parent, -current->entry->size);
    if (result < 0) { return result; }
    size_t pSize = current->entry->parent->entry->size;
    pSize = pSize - current->entry->size;
    if (!current->entry->isFile) { // is dir
        free(current->entry->entries->head);
        free(current->entry->entries->tail);
        free(current->entry->entries);
    } else {
        free(current->entry->contents);
    }
    free(current->entry);
    free(current);
    
    
    // save to disk FIXME

    return 0;
}

void updateDirSizesToRoot(struct LinkedListNode *parent, size_t size) {
    // printf("----------UPDATING-DIR-SIZES----------\n");
    // printf("PARENT: %s\n", parent->entry->name);
    // printf("SIZE: %ld\n", size);
    while (parent != NULL) {
        parent->entry->size += size;
        parent = parent->entry->parent;
    }
    // printf("----------UPDATING-DIR-SIZES----------\n");
}

void updateChildrenToParent(struct LinkedListNode *new_node) {
    // printf("-----parent-update----\n");
    struct LinkedListNode *child = new_node->entry->entries->head;
    while (child != NULL) {
        child->entry->parent = new_node;
        child = child->next;
    }
}

void dfsDelete(struct LinkedListNode* node) {
    if (node == NULL) { return; }
    struct LinkedListNode *next = NULL;
    while (node != NULL) {
        next = node->next;
        // printf("NAME: %s\n", node->entry->name);
        if (node == root) {
            // printf("THIS IS ROOT!\n");
            dfsDelete(node->entry->entries->head);
            free(root->entry->entries);
            free(root->entry);
            free(root);
            // printf("deleting... %s!\n", node->entry->name);
            return;
        }
        if (!node->entry->isFile) { dfsDelete(node->entry->entries->head); }
        // printf("deleting... %s!\n", node->entry->name);
        rmThisEntry(node);
        node = next;
    }
}

// struct methods
int lfs_getattr(const char *path, struct stat *stbuf) {
    // printf("----------getattr----------\n");
    // printf("path is: %s\n", path);
    struct LinkedListNode *current = findEntry(path);
    if (current == NULL) { 
        //     printf("Current was not found as entry\n");
        return -ENOENT;
    }
    // printf("Current name: %s\n", current->entry->name);

    memset(stbuf, 0, sizeof(struct stat));
    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
    } else if (current) {
        if (current->entry->isFile) {
            stbuf->st_mode = S_IFREG | 0777;
            stbuf->st_nlink = 1;
        } else {
            stbuf->st_mode = S_IFDIR | 0755;
            stbuf->st_nlink = 2; // plus numbers of subdirs??
        }
    } else { return -ENOENT; }
    stbuf->st_ino = current->entry->id;
    stbuf->st_size = current->entry->size;
    stbuf->st_atime = current->entry->actime;
    stbuf->st_mtime = current->entry->modtime;
    // printf("----------getattr----------\n");
    return 0;
}

int lfs_readdir( const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi ) {
    //  (void) offset;
    //  (void) fi;
    // printf("---------readdir----------\n");
    struct LinkedListNode *current;
    if (!(current = findEntry(path))) { 
        // printf("---------readdir----------\n");
        return -ENOENT; 
    } // Exits if no entry

    // printf("readdir: (path=%s)\n", path);
    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);

    if (current->entry->entries) {
        struct LinkedListNode *entryToAdd = current->entry->entries->head;
        while (entryToAdd != NULL) {
            //       printf("displaying this dir: %s\n", entryToAdd->entry->name);
            filler(buf, entryToAdd->entry->name, NULL, 0);
            //       printf("Trying to go to next\n");
            entryToAdd = entryToAdd->next;
            //       printf("entry:::: %s\n", entryToAdd);
        }
    }

    // printf("---------readdir----------\n");
    return 0;
}

int lfs_mkdir(const char *path, mode_t mode) {
    return makeEntry(path, false);
}

int lfs_rmdir(const char *path) {
    struct LinkedListNode *current = findEntry(path);
    return rmThisEntry(current);
}

int lfs_mknod(const char *path, mode_t mode, dev_t device) { 
    return makeEntry(path, true);
}

int lfs_unlink(const char *path) {
    struct LinkedListNode *current = findEntry(path);
    return rmThisEntry(current);
}

int lfs_truncate(const char *path, off_t offset) {
    struct LinkedListNode *current = findEntry(path);
    if (current == NULL) { return -ENOENT; }
    if (!current->entry->isFile) { return -EISDIR; }
    if (current->entry->contents == NULL) { return 0; }
    size_t oldSize = current->entry->size;
    char *content = current->entry->contents;
    // size specifed larger than file size fill rest with \0
    if (oldSize < offset) {
        char *tmp = malloc(offset);
        if (tmp == NULL) { return -EFAULT; }
        memset(tmp, '\0', offset);
        strcpy(tmp, content);
        current->entry->contents = tmp;
        free(content);
    }
    current->entry->contents[offset] = '\0';
    current->entry->size = offset;
    updateDirSizesToRoot(current->entry->parent, -(oldSize - offset));
    return 0;
}

int lfs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    // printf("-------------WRITE-------------\n");
    // printf("Write: buffer = %s\n", buf);
    // printf("Write: size of buffer = %d\n", (int)size);
    // printf("Write: offset = %d\n", (int)offset);
    
    struct LinkedListNode *current = fi->fh;
    size_t oldSize = current->entry->size;
    // Null terminates the buffer 
    char* tmp = malloc(size + 1);
    if (tmp == NULL) { return -EFAULT; }
    strcpy(tmp, buf);
    tmp[size] = '\0'; 
    // sets current files contents to null terminated buffer
    current->entry->contents = tmp;
    current->entry->size = size;
    current->entry->modtime = time(NULL);
    updateDirSizesToRoot(current->entry->parent, -(oldSize - size));
    // printf("-------------WRITE-------------\n");
    return size;
}

int lfs_rename(const char *from, const char *to) {
    // printf("---------rename----------\n");
    struct LinkedListNode *new_node = findEntry(to);
    if (new_node) { return -EEXIST; } // if new_node exists error
    struct LinkedListNode *old_node = findEntry(from);
    if (!old_node) { return -ENOENT; } // if old_node !exists error

    int res = makeEntry(to, old_node->entry->isFile);
    if (res) { return res; }
    new_node = findEntry(to);
    if (!new_node) { return -ENOENT; } // if new_node does not exists error

    new_node->entry->id = old_node->entry->id;
    new_node->entry->size = old_node->entry->size;
    new_node->entry->actime = old_node->entry->actime;
    new_node->entry->modtime = old_node->entry->modtime;
    updateDirSizesToRoot(new_node->entry->parent, new_node->entry->size); 
    if (!old_node->entry->isFile) {
        // printf("-----dir-----\n");
        struct LinkedList *tmp = new_node->entry->entries;
        new_node->entry->entries = old_node->entry->entries;
        old_node->entry->entries = tmp;
        updateChildrenToParent(new_node);
        lfs_rmdir(from);
        // printf("-----dir-----\n");
    } else {
        char *tmp = new_node->entry->contents;
        new_node->entry->contents = old_node->entry->contents;
        old_node->entry->contents = tmp;
        lfs_unlink(from);
    } 
    // printf("---------rename----------\n");
    return 0;
}

//Permission
int lfs_open( const char *path, struct fuse_file_info *fi ) {
    // printf("----------open----------\n");
    // printf("ffopen: (path=%s)\n", path);
    struct LinkedListNode *foundFile;
    if (!(foundFile = findEntry(path))) { return -ENOENT; }
    foundFile->entry->actime = time(NULL);
    fi->fh = foundFile;
    // printf("File found and stored in fi->fh\n");
    // printf("----------open----------\n");
    return 0;
}

int lfs_read( const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi ) {
    struct LinkedListNode *file = fi->fh;
    if (!file->entry->isFile) { return -EISDIR; }
    if (file->entry->contents == NULL) { return 0; }
    memcpy(buf, file->entry->contents, file->entry->size);
    return file->entry->size;
}

int lfs_release(const char *path, struct fuse_file_info *fi) {
    // printf("release: (path=%s)\n", path);

    // When closing file save to disk???

    return 0;
}

int lfs_utime(const char *path, const struct utimbuf *times) {
    struct LinkedListNode *current = findEntry(path);
    current->entry->actime = times->actime;
    current->entry->modtime = times->modtime;
    return 0;
}

int main( int argc, char *argv[] ) {

    // try to load from disk

    // if can load do so

    // else new root {
    root = malloc(sizeof(struct LinkedListNode));
    root->entry = malloc(sizeof(struct lfs_entry));
    strcpy(root->entry->name, "/");
    root->entry->parent = NULL;
    root->entry->size = 0;
    root->entry->isFile = false;
    root->entry->actime = time(NULL);
    root->entry->modtime = time(NULL);
    root->entry->id = generateId();
    root->entry->entries = malloc(sizeof(struct LinkedList));
    root->entry->entries->head = NULL;
    root->entry->entries->tail = NULL;
    root->entry->entries->num_entries = 0;
    // } -> save to disk

    fuse_main( argc, argv, &lfs_oper );

    //save to disk

    dfsDelete(root);

    return 0;
}
