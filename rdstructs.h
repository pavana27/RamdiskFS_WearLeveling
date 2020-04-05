#define PATH_MAX 256
#define BLOCK_SIZE 512
#define LIFE_TIME 10 // represents L
#define ERROR_LIMIT 0.1 // represents E
#define VERYYOUNG 2
#define YOUNG 4
#define MIDDLEAGE 6
#define OLDAGE 9


typedef enum {usedblock, unusedblock, retiredblock, coldblock} block_status;
typedef enum {used, unused} node_status;

typedef struct
{
	int retired_blocks;
	int young_blocks;
	int middleaged_blocks;
	int old_blocks;
	
} stats;

typedef struct
{
	block_status status;
	char data[BLOCK_SIZE];
	int next_block;
	// number of used cycles for wear leveling
	int used_cycles;
	// number of write errors for wear leveling
	int write_errors;
} block;

typedef struct 
{
	node_status status;
	char path[PATH_MAX];
	int is_dir;
	mode_t mode;
	size_t size;
	int start_block;
	stats stats;
} node;

typedef struct 
{
	size_t block_size;
	size_t nblocks;
	size_t nnodes;
	node* nodes;
	block* blocks;
	int* block_bitmap;
	int* node_bitmap;
} filesystem;