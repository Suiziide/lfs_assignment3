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
int lfs_utime(const char *filename, struct utimbuf *times);
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
    char *contents;     
    struct LinkedList *entries;
    int id; 
    u_int64_t actime;
    u_int64_t modtime;
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
int generateId() { return CURRENT_ID++; }

int saveAux(struct LinkedListNode *node, FILE *fp, char *savePath){
    if (node == NULL) { return 0; }
    size_t len;
    if (node != root) { len = strlen(savePath) + strlen(node->entry->name) + 2; }
    else { len = strlen(savePath) + strlen(node->entry->name) + 1; }
    char *current_savePath = malloc(len);
    if (!current_savePath) { return -EFAULT; }
    strncpy(current_savePath, savePath, len);
    strcat(current_savePath, node->entry->name);
    if (node != root) { current_savePath = strcat(current_savePath, "/"); }
    fprintf(fp, "|");
    fprintf(fp, "%s|" , current_savePath);
    fprintf(fp, "%d|", node->entry->isFile);
    fprintf(fp, "%d|", node->entry->id);
    fprintf(fp, "%ld|", node->entry->size);
    fprintf(fp, "%ld|", node->entry->modtime);
    fprintf(fp, "%ld|", node->entry->actime);
    if (node->entry->isFile && node->entry->contents != NULL) { fprintf(fp, "%s", node->entry->contents); }
    if (!node->entry->isFile && node->entry->entries) {
        if (node->entry->entries->head != NULL) {
            struct LinkedListNode *child = node->entry->entries->head;
            while (child != NULL) {
                saveAux(child, fp, current_savePath);
                child = child->next;
            }
        }
    }
    free(current_savePath);
    return 0;
}

int saveTree(struct LinkedListNode *root){
    char *savePath = "";
    FILE *fp = fopen("disk.img", "wb");
    if (!fp) { return errno; }
    int res;
    if ((res = saveAux(root, fp, savePath)) != 0) {
        fclose(fp);
        return res;
    }
    fprintf(fp, "|");
    fclose(fp);
    return 0;
}

struct LinkedListNode *findEntry(const char *path) {
    if (strcmp(path, "/") == 0) { return root; }
    struct LinkedListNode *current = root;
    size_t len = strlen(path) + 1;
    char *current_path = malloc(len);
    if (!current_path) { return NULL; }
    strncpy(current_path, path, len);
    current_path[len-1] = '\0';
    char *token = strtok(current_path, "/");
    while (token != NULL) {
        current = findTokenInCurrent(current, token);
        token = strtok(NULL, "/");
    }
    free(current_path);
    return current;
}

struct LinkedListNode *findTokenInCurrent(struct LinkedListNode *current, char *token) {
    current = current->entry->entries->head;
    if (current == NULL) { return NULL; }
    while (current != NULL && strcmp(current->entry->name, token) != 0) {
        current = current->next;
    }
    return current;
}	

int makeEntry(const char *path, bool isFile) {
    size_t len = strlen(path) + 1;
    char *current_path = malloc(len);
    if (!current_path) { return -EFAULT; }
    strncpy(current_path, path, len);
    bool seekOp = true;
    struct LinkedListNode *current = root;
    char tmp[FILENAME_SIZE];
    struct LinkedListNode *parent;
    char *token = strtok(current_path, "/");

    while (token != NULL) {
        size_t len = strlen(token) + 1;
        len = (len < FILENAME_SIZE) ? len : FILENAME_SIZE;
        strncpy(tmp, token, len);
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
        if (!new_node) { 
            free(current_path);
            return -EFAULT; 
        }
        new_node->entry = malloc(sizeof(struct lfs_entry));
        if (!new_node->entry) {
            free(current_path);
            free(new_node);
            return -EFAULT; 
        }
        strcpy(new_node->entry->name, tmp);
        new_node->entry->size = 0;
        new_node->entry->isFile = isFile;
        new_node->entry->actime = time(NULL);
        new_node->entry->modtime = time(NULL);
        new_node->entry->id = generateId();
        new_node->entry->parent = parent;
        if (!isFile) {
            new_node->entry->entries = malloc(sizeof(struct LinkedList));
            if (new_node->entry->entries == NULL)  { 
                free(current_path);
                free(new_node->entry);
                free(new_node);
                return -EFAULT;
            }
            new_node->entry->entries->head = NULL;
            new_node->entry->entries->tail = NULL;
            new_node->entry->entries->num_entries = 0;
        } else { new_node->entry->contents = NULL; }
        new_node->next = NULL;
        new_node->prev = parent->entry->entries->tail;
        if (new_node->prev) { new_node->prev->next = new_node; }
        parent->entry->entries->tail = new_node;
        if (!parent->entry->entries->head) { parent->entry->entries->head = new_node; }
        ++parent->entry->entries->num_entries;
    } else {
        free(current_path);
        return -EEXIST;
    }
    free(current_path);
    return 0;
}

void updateDirSizesToRoot(struct LinkedListNode *parent, size_t size) {
    while (parent != NULL) {
        parent->entry->size += size;
        parent = parent->entry->parent;
    }
}

void updateChildrenToParent(struct LinkedListNode *new_node) {
    struct LinkedListNode *child = new_node->entry->entries->head;
    while (child != NULL) {
        child->entry->parent = new_node;
        child = child->next;
    }
}

int removeEntry(struct LinkedListNode *current) {
    if (!current) { return -ENOENT; }
    if (!current->entry->isFile && current->entry->entries != NULL && current->entry->entries->num_entries != 0) {
        return -ENOTEMPTY; 
    } 
    struct LinkedListNode *parent = current->entry->parent;
    if (parent->entry->entries->head == current && parent->entry->entries->tail == current) { 
        parent->entry->entries->head = NULL;
        parent->entry->entries->tail = NULL;
    } else if (current == parent->entry->entries->head) { 
        parent->entry->entries->head = current->next;
        current->next->prev = NULL;
    } else if (current == parent->entry->entries->tail) { 
        parent->entry->entries->tail = current->prev;
        current->prev->next = NULL;
    } else {
        current->prev->next = current->next;
        current->next->prev = current->prev;
    }
    --parent->entry->entries->num_entries;
    return 0;
}

int rmThisEntry(struct LinkedListNode *current) {
    int result = removeEntry(current);
    if (result < 0) { return result; }
    updateDirSizesToRoot(current->entry->parent, -current->entry->size); // maybe move this above the if ???????

    if (!current->entry->isFile) { 
        free(current->entry->entries->head);
        free(current->entry->entries->tail);
        free(current->entry->entries);
    } else { free(current->entry->contents); }
    free(current->entry);
    free(current);
    return 0;
}

void dfsDelete(struct LinkedListNode* node) {
    if (node == NULL) { return; }
    struct LinkedListNode *next = NULL;
    while (node != NULL) {
        next = node->next;
        if (node == root) {
            dfsDelete(node->entry->entries->head);
            free(root->entry->entries);
            free(root->entry);
            free(root);
            return;
        }
        if (!node->entry->isFile) { dfsDelete(node->entry->entries->head); }
        rmThisEntry(node);
        node = next;
    }
}

int loadFromDisk() {
    FILE *fp;
    if (!(fp = fopen("disk.img", "rb"))) { return -1; }
    fseek(fp, 0, SEEK_END);
    int filesize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char fBuf[filesize+1];
    if (fread(fBuf, 1, filesize, fp) != (size_t) filesize) {
        fclose(fp);
        return -EIO;
    }
    fclose(fp);
    fBuf[filesize] = '\0';
    struct LinkedListNode *current = NULL;
    char *path = NULL;
    bool isFile = false;
    int id = 0;
    size_t size = 0;
    u_int64_t actime = time(NULL);
    u_int64_t modtime = time(NULL);
    char *contents = NULL;

    int oldPipe = 0;
    int tokenType = 0;
    int i = 0;
    for (i = 2; i < filesize; ++i) {
        if (fBuf[i] == '|' || i+1 == filesize) {
            char temp[i-oldPipe];
            ++tokenType;
            switch (tokenType) {
                case 1:
                    path = malloc(i - oldPipe);
                    if (!path) { return -EFAULT; }
                    strncpy(path, &fBuf[oldPipe+1], (size_t) (i-oldPipe-1));
                    path[i-oldPipe-1] = '\0';
                    break;
                case 2:
                    isFile = (fBuf[i-1] == '1');
                    break;
                case 3:
                    strncpy(temp, &fBuf[oldPipe+1], (size_t) (i-oldPipe-1));
                    temp[i-oldPipe-1] = '\0';
                    id = atoi(temp);
                    break;
                case 4:
                    strncpy(temp, &fBuf[oldPipe+1], (size_t) (i-oldPipe-1));
                    temp[i-oldPipe-1] = '\0';
                    size = (size_t) atoi(temp);
                    break;
                case 5:
                    strncpy(temp, &fBuf[oldPipe+1], (size_t) (i-oldPipe-1));
                    temp[i-oldPipe-1] = '\0';
                    actime = (u_int64_t) atoi(temp);
                    break;
                case 6:
                    strncpy(temp, &fBuf[oldPipe+1], (size_t) (i-oldPipe-1));
                    temp[i-oldPipe-1] = '\0';
                    modtime = (u_int64_t) atoi(temp);
                    break;
                case 7:
                    if(i-oldPipe <= 1) { break; }
                    contents = malloc(i - oldPipe);
                    if (!contents) {
                        free(path);
                        return -EFAULT;
                    }
                    strncpy(contents, &fBuf[oldPipe+1], (size_t) (i-oldPipe));
                    contents[i-oldPipe-1] = '\0';
                    break;
            }
            if (tokenType == 7) {
                tokenType %= 7;
                makeEntry(path, isFile);
                current = findEntry(path);
                if (!current) {
                    free(path);
                    free(contents);
                    return -ENOENT;
                }
                current->entry->id = id;
                current->entry->size = size;
                current->entry->actime = actime;
                current->entry->modtime = modtime;
                current->entry->contents = contents;
                free(path);
            }
            oldPipe = i;
        }
    }
    return 0;
}

// struct methods
int lfs_getattr(const char *path, struct stat *stbuf) {
    struct LinkedListNode *current = findEntry(path);
    if (current == NULL) { return -ENOENT; }
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
            stbuf->st_nlink = 2;
        }
    } else { return -ENOENT; }
    stbuf->st_ino = current->entry->id;
    stbuf->st_size = current->entry->size;
    stbuf->st_atime = current->entry->actime;
    stbuf->st_mtime = current->entry->modtime;
    return 0;
}

int lfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
    struct LinkedListNode *current;
    if (!(current = findEntry(path))) { return -ENOENT; }
    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);
    if (current->entry->entries) {
        struct LinkedListNode *entryToAdd = current->entry->entries->head;
        while (entryToAdd != NULL) {
            filler(buf, entryToAdd->entry->name, NULL, 0);
            entryToAdd = entryToAdd->next;
        }
    }
    return 0;
}

int lfs_mkdir(const char *path, mode_t mode) {
    int res = makeEntry(path, false);
    if (res != 0) { return res; }
    return saveTree(root);
}

int lfs_rmdir(const char *path) {
    int res = rmThisEntry(findEntry(path));    
    if (res != 0) { return res; }
    return saveTree(root);
}

int lfs_mknod(const char *path, mode_t mode, dev_t device) { 
    int res = makeEntry(path, true);
    if (res != 0) { return res; }
    return saveTree(root);
}

int lfs_unlink(const char *path) {
    int res = rmThisEntry(findEntry(path));    
    if (res != 0) { return res; }
    return saveTree(root);
}

int lfs_truncate(const char *path, off_t offset) {
    struct LinkedListNode *current = findEntry(path);
    if (current == NULL) { return -ENOENT; }
    if (!current->entry->isFile) { return -EISDIR; }
    size_t oldSize = current->entry->size;
    if (current->entry->contents == NULL) {
        current->entry->contents = calloc(offset, sizeof(char));
        if (!current->entry->contents) { return -EFAULT; }
    } else if (oldSize < offset) {
        char *newContent = calloc(offset, sizeof(char));
        if (!newContent) { return -EFAULT; }
        strncpy(newContent, current->entry->contents, oldSize);
        free(current->entry->contents);
        current->entry->contents = newContent;
    } else { current->entry->contents[offset] = '\0'; }
    current->entry->size = offset;
    updateDirSizesToRoot(current->entry->parent, offset - oldSize);
    return saveTree(root);
}

int lfs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    struct LinkedListNode *current = (struct LinkedListNode*) fi->fh;
    size_t oldSize = current->entry->size;
    char* tmp = malloc(size+3);
    if (tmp == NULL) { return -EFAULT; }
    strncpy(tmp, buf, size);
    tmp[size] = '\0';
    free(current->entry->contents);
    current->entry->contents = tmp;
    current->entry->size = size;
    current->entry->modtime = time(NULL);
    updateDirSizesToRoot(current->entry->parent, -(oldSize - size));
    int res = saveTree(root);
    return (res == 0) ? size : res;
}

int lfs_rename(const char *from, const char *to) {
    struct LinkedListNode *new_node = findEntry(to);
    if (new_node) { return -EEXIST; } 
    struct LinkedListNode *old_node = findEntry(from);
    if (!old_node) { return -ENOENT; } 
    int res = makeEntry(to, old_node->entry->isFile);
    if (res) { return res; }
    new_node = findEntry(to);
    new_node->entry->id = old_node->entry->id;
    new_node->entry->size = old_node->entry->size;
    new_node->entry->actime = old_node->entry->actime;
    new_node->entry->modtime = old_node->entry->modtime;
    updateDirSizesToRoot(new_node->entry->parent, new_node->entry->size); 
    if (!old_node->entry->isFile) {
        struct LinkedList *tmp = new_node->entry->entries;
        new_node->entry->entries = old_node->entry->entries;
        old_node->entry->entries = tmp;
        updateChildrenToParent(new_node);
        lfs_rmdir(from);
    } else {
        char *tmp = new_node->entry->contents;
        new_node->entry->contents = old_node->entry->contents;
        old_node->entry->contents = tmp;
        lfs_unlink(from);
    } 
    return 0;
}

//Permission
int lfs_open(const char *path, struct fuse_file_info *fi ) {
    struct LinkedListNode *foundFile;
    if (!(foundFile = findEntry(path))) { return -ENOENT; }
    foundFile->entry->actime = time(NULL);
    fi->fh = (uint64_t) foundFile;
    return 0;
}

int lfs_read(const char* path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    struct LinkedListNode *file = (struct LinkedListNode*) fi->fh;
    if (!file->entry->isFile) { return -EISDIR; }
    if (file->entry->contents == NULL || file->entry->size == 0) { return 0; }
    size_t len = (file->entry->size < size) ? file->entry->size : size;
    strncpy(buf, file->entry->contents, len);
    return len;
}

int lfs_release(const char *path, struct fuse_file_info *fi) { return 0; }

int lfs_utime(const char *path, struct utimbuf *times) {
    struct LinkedListNode *current = findEntry(path);
    current->entry->actime = times->actime;
    current->entry->modtime = times->modtime;
    return saveTree(root);
}

int init() {
    root = malloc(sizeof(struct LinkedListNode));
    if (!root) { return -EFAULT; }
    root->entry = malloc(sizeof(struct lfs_entry));
    if (!root->entry) { 
        free(root);
        return -EFAULT; 
    }
    strcpy(root->entry->name, "/");
    root->entry->parent = NULL;
    root->entry->size = 0;
    root->entry->isFile = false;
    root->entry->actime = time(NULL);
    root->entry->modtime = time(NULL);
    root->entry->id = generateId();
    root->entry->entries = malloc(sizeof(struct LinkedList));
    if (!root->entry->entries) { 
        free(root->entry);
        free(root);
        return -EFAULT; 
    }
    root->entry->entries->head = NULL;
    root->entry->entries->tail = NULL;
    root->entry->entries->num_entries = 0;

    int res = loadFromDisk();
    if (res == -1) {
        printf("Disk.img file does not exists - creating empty one\n");
        res = 0;
    } else if (res != 0) {
        free(root->entry->entries);
        free(root->entry);
        free(root);
    }
    return res;
}

int main(int argc, char *argv[]) {
    int res = init();
    if (res != 0) { return res; }
    fuse_main(argc, argv, &lfs_oper);
    res = saveTree(root);
    dfsDelete(root);
    return res;
}
