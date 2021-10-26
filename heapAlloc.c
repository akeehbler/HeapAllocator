///////////////////////////////////////////////////////////////////////////////
//
// Name: Alec Keehbler
// School: UW Madison
//
///////////////////////////////////////////////////////////////////////////////
 
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdio.h>
#include <string.h>
#include "heapAlloc.h"

#define A_BIT_MASK		0x0001
#define P_BIT_MASK		0x0002
#define SIZE_MASK		(~(A_BIT_MASK | P_BIT_MASK))
#define BLK_SIZE(header)	(header->size_status & SIZE_MASK)
#define BLK_ALLOC(header)	(header->size_status & A_BIT_MASK)
#define BLK_PREV(header)	(header->size_status & P_BIT_MASK)
#define NEXT_HEADER(header)	(header + (BLK_SIZE(header) / sizeof(blockHeader)))
#define GET_FOOTER(header) 	(NEXT_HEADER(header) - 1)
#define IS_END_OF_HEAP(header)	(header->size_status == 1)

 
/*
 * This structure serves as the header for each allocated and free block.
 * It also serves as the footer for each free block but only containing size.
 */
typedef struct blockHeader {           
    int size_status;
    /*
    * Size of the block is always a multiple of 8.
    * Size is stored in all block headers and free block footers.
    *
    * Status is stored only in headers using the two least significant bits.
    *   Bit0 => least significant bit, last bit
    *   Bit0 == 0 => free block
    *   Bit0 == 1 => allocated block
    *
    *   Bit1 => second last bit 
    *   Bit1 == 0 => previous block is free
    *   Bit1 == 1 => previous block is allocated
    * 
    * End Mark: 
    *  The end of the available memory is indicated using a size_status of 1.
    * 
    * Examples:
    * 
    * 1. Allocated block of size 24 bytes:
    *    Header:
    *      If the previous block is allocated, size_status should be 27
    *      If the previous block is free, size_status should be 25
    * 
    * 2. Free block of size 24 bytes:
    *    Header:
    *      If the previous block is allocated, size_status should be 26
    *      If the previous block is free, size_status should be 24
    *    Footer:
    *      size_status should be 24
    */
} blockHeader;         

/* Global variable - DO NOT CHANGE. It should always point to the first block,
 * i.e., the block at the lowest address.
 */
blockHeader *heapStart = NULL;     

/* Size of heap allocation padded to round to nearest page size.
 */
int allocsize;

/*
 * Additional global variables may be added as needed below
 */
blockHeader *lastAllocedBlock = NULL;
 
/* 
 * Function for allocating 'size' bytes of heap memory.
 * Argument size: requested size for the payload
 * Returns address of allocated block on success.
 * Returns NULL on failure.
 * This function should:
 * - Check size - Return NULL if not positive or if larger than heap space.
 * - Determine block size rounding up to a multiple of 8 and possibly adding padding as a result.
 * - Use NEXT-FIT PLACEMENT POLICY to chose a free block
 * - Use SPLITTING to divide the chosen free block into two if it is too large.
 * - Update header(s) and footer as needed.
 * Tips: Be careful with pointer arithmetic and scale factors.
 */
void* allocHeap(int size){
	blockHeader *start = NULL;
	blockHeader *curr = NULL;
	//this is the pointer that should have the data
	blockHeader *next;
	//for the free block if splitting
	blockHeader *split = NULL;
	int freeSizeSplit;
	//keeps track if a block is found	
	int found = 0;
	if(size <  1){
		return NULL;
	}
	else if(size > allocsize){
		return NULL;
	}
	//if it is null then this is first loop, so no alloced block yet, start at start
	if(lastAllocedBlock == NULL){
		lastAllocedBlock = heapStart;
	}
	//rounds size to the nearest multiple of 8
	if(size %8 != 0){
		size += 8 - (size % 8);
	}
	start = lastAllocedBlock;
	curr = lastAllocedBlock;
	//loop to find the next fit, first while loop goes to end of the heap
	while(!IS_END_OF_HEAP(curr)){
		//makes sure the block isn't allocated
		if(!(BLK_ALLOC(curr))){
			//this would be perfect size so no need to split
			if(BLK_SIZE(curr) == size){
				
				curr->size_status |= A_BIT_MASK;
				//makes sure that the next block is not the end of heap
				if(!IS_END_OF_HEAP(NEXT_HEADER(curr))){
					curr->size_status |= P_BIT_MASK;	
				}
				//return the current pointer plus 1 which would be a pointer to the start of the data allocated
				return curr + 1;
			}
			//this is if it isn't perfect size, check if the block is greater than the requested size
			//also this means splitting will occur 
			else if(BLK_SIZE(curr) > size){
				next = curr;
				found = 1;
				break;
			}
		}
		curr = NEXT_HEADER(curr);//increments to the next header
	}
	//wraps around to the start
	curr = heapStart;
	//goes from the start of the heap to the lastAllocedBlock
	while(curr != start  && found == 0){
		//checks to make sure the block is not allocated
		if(!(BLK_ALLOC(curr))){
			//this would be perfect size so no need to split
			if(BLK_SIZE(curr) == size){
				curr->size_status |= A_BIT_MASK;
				//if its the start block then must set p bit to 1
				if(curr == heapStart){
					curr->size_status |= P_BIT_MASK;
				}
				//return the current pointer plus 1 which would be a pointer to the start of the data aloocated
				return curr + 1;
			}
			else if(BLK_SIZE(curr) > size){
				next = curr;
				found = 1;
				break;
			}
		}
		curr = NEXT_HEADER(curr); //increments to the next header
	}
	//not found through either loop
	if(found == 0){
		return NULL;
	}
	//this is the free block due to splitting
	freeSizeSplit = BLK_SIZE(next) - size;
	split = next + (size / sizeof(blockHeader));
	split-> size_status = ((freeSizeSplit | P_BIT_MASK) & ~(A_BIT_MASK));
	GET_FOOTER(split) -> size_status = BLK_SIZE(split);
	//allocated the non-perfect size block now
	next->size_status = size | (next->size_status & (A_BIT_MASK | P_BIT_MASK));
	next->size_status |= A_BIT_MASK;
	if(!IS_END_OF_HEAP(NEXT_HEADER(next))){
		NEXT_HEADER(next) ->size_status |= P_BIT_MASK;
	}
	if(next == heapStart){
		next->size_status |= P_BIT_MASK;
	}
	return next + 1;
} 
 
/* 
 * Function for freeing up a previously allocated block.
 * Argument ptr: address of the block to be freed up.
 * Returns 0 on success.
 * Returns -1 on failure.
 * This function should:
 * - Return -1 if ptr is NULL.
 * - Return -1 if ptr is not a multiple of 8.
 * - Return -1 if ptr is outside of the heap space.
 * - Return -1 if ptr block is already freed.
 * - USE IMMEDIATE COALESCING if one or both of the adjacent neighbors are free.
 * - Update header(s) and footer as needed.
 */                    
int freeHeap(void *ptr) { 
     	blockHeader *header;	
	blockHeader *nextHeader;
	blockHeader *prevHeader;

	if(ptr == NULL){
		return -1;
	}
	if((int) ptr % 8 != 0){
		return -1;
	}
	if(ptr < (void*)heapStart){
		return -1;
	}
	if(ptr > ((void*)heapStart + allocsize)){
		return -1;
	}
	//must subtract to get from the payload to the header of the block
	header = ptr - sizeof(blockHeader);
	//if not allocated then return -1
	if(!BLK_ALLOC(header)){
		return -1;
	}
	nextHeader = NEXT_HEADER(header);
	//Frees up the block
	header->size_status &= ~A_BIT_MASK;
	nextHeader->size_status &= ~P_BIT_MASK;
	GET_FOOTER(header)->size_status = BLK_SIZE(header);
	//coalesce  with the next block
	if(!BLK_ALLOC(nextHeader) && !IS_END_OF_HEAP(nextHeader)){
		//adds the next block size and the current one together
		header->size_status = header->size_status + nextHeader-> size_status;
		//sets the footer of the new coalesed block
		GET_FOOTER(header)->size_status = BLK_SIZE(header);
	}
	//coalese with the previous block
	if(!BLK_PREV(header)){
		//goes backwards to get the previous header, incrementing properly
		prevHeader = header - (((header -1)->size_status) /sizeof(blockHeader));
		//sets the previous block size to its size + the current one(the one ahead)
		prevHeader->size_status += header->size_status;
		//Sets the new footer of the coalesed block
		GET_FOOTER(prevHeader)->size_status = BLK_SIZE(prevHeader);
	}	
	return 0;
} 
 
/*
 * Function used to initialize the memory allocator.
 * Intended to be called ONLY once by a program.
 * Argument sizeOfRegion: the size of the heap space to be allocated.
 * Returns 0 on success.
 * Returns -1 on failure.
 */                    
int initHeap(int sizeOfRegion) {    
 
    static int allocated_once = 0; //prevent multiple initHeap calls
 
    int pagesize;  // page size
    int padsize;   // size of padding when heap size not a multiple of page size
    void* mmap_ptr; // pointer to memory mapped area
    int fd;

    blockHeader* endMark;
  
    if (0 != allocated_once) {
        fprintf(stderr, 
        "Error:mem.c: InitHeap has allocated space during a previous call\n");
        return -1;
    }
    if (sizeOfRegion <= 0) {
        fprintf(stderr, "Error:mem.c: Requested block size is not positive\n");
        return -1;
    }

    // Get the pagesize
    pagesize = getpagesize();

    // Calculate padsize as the padding required to round up sizeOfRegion 
    // to a multiple of pagesize
    padsize = sizeOfRegion % pagesize;
    padsize = (pagesize - padsize) % pagesize;

    allocsize = sizeOfRegion + padsize;

    // Using mmap to allocate memory
    fd = open("/dev/zero", O_RDWR);
    if (-1 == fd) {
        fprintf(stderr, "Error:mem.c: Cannot open /dev/zero\n");
        return -1;
    }
    mmap_ptr = mmap(NULL, allocsize, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    if (MAP_FAILED == mmap_ptr) {
        fprintf(stderr, "Error:mem.c: mmap cannot allocate space\n");
        allocated_once = 0;
        return -1;
    }
  
    allocated_once = 1;

    // for double word alignment and end mark
    allocsize -= 8;

    // Initially there is only one big free block in the heap.
    // Skip first 4 bytes for double word alignment requirement.
    heapStart = (blockHeader*) mmap_ptr + 1;

    // Set the end mark
    endMark = (blockHeader*)((void*)heapStart + allocsize);
    endMark->size_status = 1;

    // Set size in header
    heapStart->size_status = allocsize;

    // Set p-bit as allocated in header
    // note a-bit left at 0 for free
    heapStart->size_status += 2;

    // Set the footer
    blockHeader *footer = (blockHeader*) ((void*)heapStart + allocsize - 4);
    footer->size_status = allocsize;
  
    return 0;
} 
                  
/* 
 * Function to be used for DEBUGGING to help you visualize your heap structure.
 * Prints out a list of all the blocks including this information:
 * No.      : serial number of the block 
 * Status   : free/used (allocated)
 * Prev     : status of previous block free/used (allocated)
 * t_Begin  : address of the first byte in the block (where the header starts) 
 * t_End    : address of the last byte in the block 
 * t_Size   : size of the block as stored in the block header
 */                     
void dumpMem() {     
 
    int counter;
    char status[5];
    char p_status[5];
    char *t_begin = NULL;
    char *t_end   = NULL;
    int t_size;

    blockHeader *current = heapStart;
    counter = 1;

    int used_size = 0;
    int free_size = 0;
    int is_used   = -1;

    fprintf(stdout, "************************************Block list***\
                    ********************************\n");
    fprintf(stdout, "No.\tStatus\tPrev\tt_Begin\t\tt_End\t\tt_Size\n");
    fprintf(stdout, "-------------------------------------------------\
                    --------------------------------\n");
  
    while (current->size_status != 1) {
        t_begin = (char*)current;
        t_size = current->size_status;
    
        if (t_size & 1) {
            // LSB = 1 => used block
            strcpy(status, "used");
            is_used = 1;
            t_size = t_size - 1;
        } else {
            strcpy(status, "Free");
            is_used = 0;
        }

        if (t_size & 2) {
            strcpy(p_status, "used");
            t_size = t_size - 2;
        } else {
            strcpy(p_status, "Free");
        }

        if (is_used) 
            used_size += t_size;
        else 
            free_size += t_size;

        t_end = t_begin + t_size - 1;
    
        fprintf(stdout, "%d\t%s\t%s\t0x%08lx\t0x%08lx\t%d\n", counter, status, 
        p_status, (unsigned long int)t_begin, (unsigned long int)t_end, t_size);
    
        current = (blockHeader*)((char*)current + t_size);
        counter = counter + 1;
    }

    fprintf(stdout, "---------------------------------------------------\
                    ------------------------------\n");
    fprintf(stdout, "***************************************************\
                    ******************************\n");
    fprintf(stdout, "Total used size = %d\n", used_size);
    fprintf(stdout, "Total free size = %d\n", free_size);
    fprintf(stdout, "Total size = %d\n", used_size + free_size);
    fprintf(stdout, "***************************************************\
                    ******************************\n");
    fflush(stdout);

    return;  
} 
