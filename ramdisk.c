/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.

  gcc -Wall hello.c `pkg-config fuse --cflags --libs` -o hello
*/

#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <fnmatch.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <time.h>

#include "rdstructs.h"
#include <ctype.h>
#include <dirent.h>
#include <libgen.h>
#include <limits.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/xattr.h>

int NNODES;
int NBLOCKS;
int persistent = 0;
FILE *fp;
char filename[PATH_MAX];
char a_path[PATH_MAX];

filesystem f;



/* CHUNK is the size of the memory chunk used by the zlib routines. */

#define CHUNK 0x4000

void nodes_init()
{
	int i;
	for (i = 0; i < f.nnodes; i++) {
		f.nodes[i].status = unused;
		f.nodes[i].size = 0;
		f.nodes[i].start_block = -1;
		f.nodes[i].is_dir = 0;
	}
}

void blocks_init()
{
	int i;
	for (i = 0; i < f.nblocks; i++) {
		f.blocks[i].status = unusedblock;
		f.blocks[i].next_block = -1;
	}
}

int path_search(const char *path)
{
	int i;
	for (i = 0; i < f.nnodes; i++) {
		if ( f.nodes[i].status == used && strcmp(path, f.nodes[i].path) == 0) {
			return i;
		}
	}

	return -ENOENT;
}

int print_info()
{
	int j, i, start, k=0, h=0;
	FILE* fp = fopen ("/home/pavana/Documents/results.txt","w+");
	FILE* fp1 = fopen ("/home/pavana/Documents/stats.txt","w+");
	fprintf( fp, "filename, used_cycles, write_errors \n");
	fprintf( fp1, "filename, young, middleaged, oldaged, retired \n");
	for(i = 0; i < f.nnodes; i++) {
		if ( f.nodes[i].status == used ) {
			printf("\n\n------File: %s---------\n", f.nodes[i].path);
			start = f.nodes[i].start_block;
			stats stats = f.nodes[i].stats;
			fprintf(fp1, "%s, %d, %d, %d, %d \n",f.nodes[i].path, stats.young_blocks, 
					stats.middleaged_blocks, stats.old_blocks, stats.retired_blocks);

			j = 0;
			while(start!=-1) {
				if(f.blocks[start].used_cycles >= LIFE_TIME)
					fprintf(fp, "%s, %d, %d \n",f.nodes[i].path, f.blocks[start].used_cycles, f.blocks[start].write_errors);
				start = f.blocks[start].next_block;
				j+=1;
				if(start == f.blocks[start].next_block) {
					break;
				}
			}
			//printf("------Blocks in file: %d----------\n",j);
			k++;
			h+=j;
		}
	}
	fclose(fp);
	fclose(fp1);
	//printf("------Total Files: %d----------\n",k);
	//printf("------Total Blocks: %d----------\n",h);

	j = 0;
	printf("\n\n------Reading Blocks---\n");
	printf("Blocks: ");
	for(i= 0; i < f.nblocks; i++) {
		if ( f.blocks[i].status == usedblock ) {
			//printf("%d, ", i);
			j+=1;
		}
	}
	printf("\n------Total Blocks: %d----------\n\n",j);

	return 0;
}

node* get_free_node()
{
	int i;
	for (i = 0; i < f.nnodes; i++) {
		if ( f.nodes[i].status == unused ){
			f.nodes[i].status = used;

			f.nodes[i].stats.retired_blocks = 0;
			f.nodes[i].stats.young_blocks=0;
			f.nodes[i].stats.middleaged_blocks=0;
			f.nodes[i].stats.old_blocks=0;
			
			return &f.nodes[i];
		}
	}
	//perror("No more nodes left!!");
	return NULL;
}

int get_free_block_index()
{
	int i;
	for (i = 0; i < f.nblocks; i++) {
		if ( f.blocks[i].status == unusedblock ){
			f.blocks[i].status = usedblock;
			f.blocks[i].used_cycles = 0;
			f.blocks[i].write_errors = 0;
			return i;
		}
	}
	//perror("No more blocks left!!");
	return -1;
}

int hello_init() 
{
    
    f.block_size = BLOCK_SIZE;
    f.nnodes = NNODES;
    f.nblocks = NBLOCKS;
    
    f.nodes = (node *) malloc(f.nnodes*sizeof(node));
    f.blocks = (block *) malloc(f.nblocks*sizeof(block));
    
    if (persistent == 1) {
        int fileExist = access( filename, F_OK );
        
        printf("File exist :: %d", fileExist);

        if (fileExist != -1) {
            fp = fopen(filename, "r");
            printf("Fileno: %i\n", fp->_fileno); 
            fread(f.nodes, sizeof(node), f.nnodes, fp);
            fread(f.blocks, sizeof(block), f.nblocks, fp);
            //print_info();
        }
        else {
            fp = fopen(filename, "w+");
            nodes_init();
            blocks_init();
            fclose(fp);
            fprintf(stderr,"In else block");
			//print_info();
        }
    }
    else {
        nodes_init();
        blocks_init();
    }
    
    return 0;
} 

void hello_destroy(void *v)
{
	print_info();

	if (persistent == 1) {
		fp = fopen(filename, "w");
		if (fp != NULL) {
			fwrite(f.nodes, sizeof(node), f.nnodes, fp);
			fwrite(f.blocks, sizeof(block), f.nblocks, fp);
			fclose(fp);
		}
		else {
			perror("Couldn't open file in write mode");
		}
	}
	
	free(f.nodes);
	free(f.blocks);
} 


int pathmatch(const char *dir, const char *path)
{
	strcpy(a_path, dir);
	strcat(a_path, "/*");
	return fnmatch(a_path, path, FNM_PATHNAME);
}

int hello_getattr(const char *path, struct stat *stbuf)
{

	memset(stbuf, 0, sizeof(struct stat));
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
		stbuf->st_size = f.block_size;
		return 0;
	} 
	
	int i = path_search(path);
	if ( i < 0 )
		return i;

	stbuf->st_mode = f.nodes[i].mode;
	stbuf->st_nlink = 1;
	stbuf->st_size = f.nodes[i].size;	
	return 0;
}

int hello_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi)
{
	(void) offset;
	(void) fi;
	int i,index = -1;
	char *dirpath = "";
	
	if (strcmp(path, "/") != 0 ) {
		for (i = 0; i < f.nnodes; i++) {
			if ( f.nodes[i].is_dir == 1 && f.nodes[i].status == used && strcmp(path, f.nodes[i].path) == 0) {
				index = i;
				dirpath = f.nodes[i].path;
				break;
			}
		}

		if (index == -1)
			return -ENOENT;
	
	}
	
	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);

	for (i = 0; i < f.nnodes; i++) {
		if ( f.nodes[i].status == used && pathmatch(dirpath, f.nodes[i].path) == 0 ) {
			filler(buf, strrchr(f.nodes[i].path, '/')+1 , NULL, 0);
		}
	}

	//print_info();

	return 0;
}

int hello_open(const char *path, struct fuse_file_info *fi)
{
	if ( path_search(path) == -1 ){
		return -ENOENT;	
	}
	return 0;
}

int hello_create(const char *path, mode_t mode, struct fuse_file_info *fi) 
{
	//open file if it exists. if not the create and open file
	if ( path_search(path) == -ENOENT ) {
		//file doesn't exist
		node* file;
		file = get_free_node();
		if (file == NULL) {
			//perror("Cannot create file: No nodes left.");
			return -EDQUOT;
		}
		file->mode = mode;
		strcpy(file->path, path);	
		
	}
	
	return 0;
}



int hello_read(const char *path, char *buf, size_t size, off_t offset,
		      struct fuse_file_info *fi)
{
	//return number of bytes read
	int fileblock, size2,i;

	i = path_search(path);
	if ( i < 0 )
		return i;
	
	if ( size > f.nodes[i].size ) {
		// cant read more than the size of file
		size = f.nodes[i].size;
	}

	size2 = f.block_size;
	fileblock = f.nodes[i].start_block;
	
	if ( fileblock == -1 )
		return 0;
	
	//seek to the offset with fileblock pointer
	while ( offset-size2 > 0 ) {
		fileblock = f.blocks[fileblock].next_block;
		offset -= size2;
	}

	// start actual read
	
	// read partial block
	if ( offset > 0 ) {
		memcpy(buf, f.blocks[fileblock].data + offset , size2-offset);
		fileblock = f.blocks[fileblock].next_block;
	}
	
	// read all other whole blocks
	while ( offset < size ) {	
		if ( offset + size2 > size ){
			size2 = size - offset;
		}

		if ( fileblock == -1 ) {
			//perror("read_error:this is not supposed to happen\n");
			break;
		}
		
		memcpy(buf + offset, f.blocks[fileblock].data , size2);
		fileblock = f.blocks[fileblock].next_block;
		offset += size2;
	}			
	return size;
}

double getErrorProbability() {

	return rand()/(double)RAND_MAX;
}

int updateStats(const char *path) {

	int i = path_search(path);

	if ( i < 0 ) {
		return i;
	}

	int j = f.nodes[i].start_block;
	while (j != -1) {
		int used_cycles = f.blocks[j].used_cycles;
		if(used_cycles <= YOUNG) {
			f.nodes[i].stats.young_blocks ++;
		} else if (used_cycles <= MIDDLEAGE) {
			f.nodes[i].stats.middleaged_blocks ++;
		} else if (used_cycles <= OLDAGE) {
			f.nodes[i].stats.old_blocks ++;
		} else if (used_cycles >= (1.2 * LIFE_TIME) ) {
			f.nodes[i].stats.retired_blocks ++;
		}

		j = f.blocks[j].next_block;
	}
	
	return 0;
}

int hello_write(const char *path, const char *buf, size_t size, off_t offset,
		      struct fuse_file_info *fi)
{
	//return number of bytes written
	int fileblock, last, size2, i;
	i = path_search(path);
	if ( i < 0 )
		return i;
			
	f.nodes[i].size = offset;
	size2 = f.block_size;
	fileblock = f.nodes[i].start_block;

	// file is new, link a block
	if ( fileblock == -1 && size > 0) {
		fileblock = get_free_block_index();
		if (fileblock == -1) {
			//perror("Cannot write to file: No space left.");
			f.nodes[i].size = 0;
			return -EDQUOT;
		}
		f.nodes[i].start_block = fileblock;
	}

	//seek to the offset with fileblock pointer
	while ( offset-size2 > 0 ) {
		fileblock = f.blocks[fileblock].next_block;
		offset -= size2;
	}

	// start actual write
	
	// write partial block
	if ( offset > 0 ) {

		// check if used_cycles for this block < L
		// if yes, generate the error random number e
		// compare e with E,
		// if greated than E, mark error in block
		// regenerate e
		int usedCycles = f.blocks[fileblock].used_cycles;
		bool error = true;
		if (usedCycles == 0 || usedCycles < LIFE_TIME) {
			error = false;
		} else {
			f.blocks[fileblock].write_errors = f.blocks[fileblock].write_errors + 1;
			f.blocks[fileblock].used_cycles = f.blocks[fileblock].used_cycles + 1;
			double errorProb = getErrorProbability(); // this is e
			while(errorProb > ERROR_LIMIT) { // compare with E
				errorProb = getErrorProbability(); // regenerate e
				f.blocks[fileblock].write_errors = f.blocks[fileblock].write_errors + 1; // increment errors count
				f.blocks[fileblock].used_cycles = f.blocks[fileblock].used_cycles + 1;
			}
			error = false;
		}

		if(!error) {

			// if the errors are greater than threshold,
			// call get_free_block_index() to get new block
			// release the old block.
			// increment the used_cycles for each block write.
			f.blocks[fileblock].used_cycles = f.blocks[fileblock].used_cycles + 1;

			memcpy(f.blocks[fileblock].data + offset, buf, size2-offset);
			last = fileblock;
			fileblock = f.blocks[fileblock].next_block;
			offset = size2 - offset;
		}
	}

	// write to all other whole blocks
	while ( offset < size ) {	
		if ( offset + size2 > size ) {
			size2 = size - offset;
		}
		
		if ( fileblock == -1 ) {
			fileblock = get_free_block_index();
			if (fileblock == -1) {
				//perror("Cannot write to file: No space left.");
				errno = -EDQUOT;
				break;
			}
			f.blocks[last].next_block = fileblock;
		}

		// check if used_cycles for this block < L
		// if yes, generate the error random number e
		// compare e with E,
		// if greated than E, mark error in block
		// regenerate e

		int usedCycles = f.blocks[fileblock].used_cycles;
		bool error = true;
		if (usedCycles == 0 || usedCycles < LIFE_TIME) {
			error = false;			
		} else {
			f.blocks[fileblock].write_errors = f.blocks[fileblock].write_errors + 1;
			f.blocks[fileblock].used_cycles = f.blocks[fileblock].used_cycles + 1;
			double errorProb = getErrorProbability(); // this is e
			while(errorProb > ERROR_LIMIT) { // compare with E
				errorProb = getErrorProbability(); // regenerate e
				f.blocks[fileblock].write_errors = f.blocks[fileblock].write_errors + 1; // increment errors count
				f.blocks[fileblock].used_cycles = f.blocks[fileblock].used_cycles + 1;
			}
			error = false;
		}

		if(!error) {

			// if the errors are greater than threshold,
			// call get_free_block_index() to get new block
			// release the old block.
			
			// increment the used_cycles for each block write.
			f.blocks[fileblock].used_cycles = f.blocks[fileblock].used_cycles + 1;

			memcpy(f.blocks[fileblock].data, buf + offset , size2);
			last = fileblock;
			fileblock = f.blocks[fileblock].next_block;
			offset += size2;
		}
	}
	
	f.nodes[i].size += offset;

	updateStats(path);
	
	return offset;	 
}

int hello_mkdir(const char *path, mode_t mode)
{
	if ( path_search(path) == -ENOENT) {
		//directory doesn't exist
		node* dir;
		dir = get_free_node();
		if (dir == NULL) {
			//perror("Cannot create directory: No space left.");
			return -EDQUOT;
		}
		
		dir->is_dir = 1;
		dir->mode = S_IFDIR|mode;
		dir->size = f.block_size;
		strcpy(dir->path, path);	
		return 0;
	}
	
	return -EEXIST;
}

int free_block(int index)
{
	int next;
	f.blocks[index].status = unusedblock;
	next = f.blocks[index].next_block;
	f.blocks[index].next_block = -1;
	return next;
}

int hello_truncate(const char *path, off_t offset)
{
	int fileblock, size2, i;

	i = path_search(path);
	if ( i < 0 )
		return i;
			
	if ( offset >= f.nodes[i].size ) {
		//see man 2 truncate, if offset > size then fill with /0 characters
		return 0;
	}

	size2 = f.block_size;
	fileblock = f.nodes[i].start_block;
	
	//seek to the offset
	while ( offset-size2 > 0 ) {
		fileblock = f.blocks[fileblock].next_block;
		offset -= size2;
	}

	// start actual truncate

	// truncate partial block
	if ( offset > 0 ) {
		memset(f.blocks[fileblock].data + offset, '\0', size2-offset);
		fileblock = f.blocks[fileblock].next_block;
		offset = size2 - offset;
	}
	
	//truncate whole blocks
	while ( fileblock != -1 ) {
		fileblock = free_block(fileblock);
	}
	
	if (offset == 0) {
		f.nodes[i].start_block = -1;	
	}
	
	f.nodes[i].size = offset;
	return 0;	
}

int hello_unlink(const char *path)
{
	int status, i;
	
	status = hello_truncate(path, 0);
	if (status < 0) {
		return status;
	}
	
	i = path_search(path);		
	f.nodes[i].start_block = -1;
	f.nodes[i].status = unused;
		
	return 0;
}

int hello_rmdir(const char *path)
{
	int i, j, index = -1;
	
	if (strcmp(path, "/") == 0) {
		return -EACCES;
	}

	for (i = 0; i < f.nnodes; i++) {
		if ( f.nodes[i].is_dir == 1 && f.nodes[i].status == used && strcmp(path, f.nodes[i].path) == 0) {
			index = i;
			break;
		}
	}

	if ( index == -1 ) {
		return -ENOENT;
	}

	for (j = 0; j < f.nnodes; j++) {
		if ( f.nodes[j].status == used && f.nodes[j].is_dir == 0 && pathmatch(path, f.nodes[j].path) == 0 ) {
			return -ENOTEMPTY;
		}
	}

	f.nodes[i].status = unused;
		
	return 0;

}

int hello_rename(const char *path, const char *newpath)
{
	// works only for files and not directories
	int i = path_search(path);
	if ( i < 0 )
		return i;
	
	if ( f.nodes[i].is_dir == 0) {
		int j = path_search(newpath);
		if ( j>= 0 && f.nodes[j].is_dir == 0) {
			hello_unlink(newpath);
		}	
		strcpy(f.nodes[i].path, newpath);	
	}
	
	return 0;
}

int hello_opendir(const char *path, struct fuse_file_info *fu)
{
	int i;
	if (strcmp("/", path) == 0) {
		return 0;
	}

	for (i = 0; i < f.nnodes; i++) {
		if ( f.nodes[i].is_dir == 1 && f.nodes[i].status == used && strcmp(path, f.nodes[i].path) == 0) {
			return 0;
		}
	}

	return -ENOENT;
}

int hello_flush(const char *path, struct fuse_file_info *f)
{
	return 0;
}



int hello_listxattr(const char *path, char *list, size_t size) {
	
	int retstat = 0;
    
    char *ptr;
    
    retstat = llistxattr(path, list, size);
    if (retstat >= 0) {
	printf("    returned attributes (length %d):\n", retstat);
	if (list != NULL)
	    for (ptr = list; ptr < list + retstat; ptr += strlen(ptr)+1)
		printf("    \"%s\"\n", ptr);
	else
	    printf("    (null)\n");
    }
    
    return retstat;
}

int hello_removexattr(const char *path, const char *name) {
	
	int retstat = 0;
	lremovexattr(path, name);

	return retstat;
}

/** Synchronize file contents
 *
 * If the datasync parameter is non-zero, then only the user data
 * should be flushed, not the meta data.
 *
 * Changed in version 2.2
 */
int hello_fsync(const char *path, int datasync, struct fuse_file_info *fi)
{
    printf("\nhello_fsync(path=\"%s\", datasync=%d)\n", path, datasync);
    
    // some unix-like systems (notably freebsd) don't have a datasync call
	if (datasync)
		return fdatasync(fi->fh);
	else
		return fsync(fi->fh);
}

int hello_fsyncdir(const char *path, int datasync, struct fuse_file_info *fi)
{
    int retstat = 0;
    
    printf("\nhello_fsyncdir(path=\"%s\", datasync=%d)\n", path, datasync);
    fsync(fi->fh);
    return retstat;
}

struct fuse_operations hello_oper = {
	.getattr	= hello_getattr,
	.readdir	= hello_readdir,
	.opendir	= hello_opendir,
	.mkdir		= hello_mkdir,
	.rmdir		= hello_rmdir,
	.create		= hello_create,
	.truncate 	= hello_truncate,
	.open		= hello_open,
	.read		= hello_read,
	.write 		= hello_write,
	.unlink		= hello_unlink,
	.flush		= hello_flush,
	.destroy 	= hello_destroy,
	.rename 	= hello_rename
};

int main(int argc, char *argv[])
{
	if (argc < 3) {
		fprintf(stderr, "Usage: %s <mount-directory> <sizeinMB> [<disk-image>]\n", argv[0]);
		return -1;
	}

	if (argc == 4) {
		strcpy(filename, argv[3]);
		fprintf(stderr,"file name: %s\n", filename);
		persistent = 1;
	}

	size_t size_bytes = atoi(argv[2])*1000000;
	NBLOCKS = size_bytes/(sizeof(node) + sizeof(block));
	NNODES = NBLOCKS;
	
	size_t storage = NBLOCKS*sizeof(block);
	fprintf(stderr,"number of blocks: %d\n", NBLOCKS);
	fprintf(stderr,"number of nodes: %d\n", NNODES);
	fprintf(stderr,"Total space for storage: %lu\n", storage);

	hello_init();

	argc = 2;
	return fuse_main(argc, argv, &hello_oper, NULL);
}