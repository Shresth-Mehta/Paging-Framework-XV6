#include "types.h"
#include "stat.h"
#include "user.h"
#include "syscall.h"

#define PAGESIZE 4096


int
main(int argc, char *argv[]){

#if NFU
    int i,j;
    char *page[15];
    printf(1, "\n   ***Testing NFU***\n");
    printf(1, "\n   **Process Starts Executing**\n");
    printf(1,"\nInitial process details:\n");
    printStats();


    printf(1,"\n    **Adding 12 more pages to the process**\n\n");
    //We already have 3 pages for the process in the main memory.
    //We will now allocate 12 more pages to get to the 15 page limit that we had set
    for(i = 0; i<12; i++){
        page[i] = sbrk(PAGESIZE);
        printf(1,"page[%d]=0x%x\n",i,page[i]);
    }
    printf(1,"\nProcess details:\n");
    printStats();

    //Adding one more page i.e page no.15 
    printf(1,"\n    **Allocating 16th page for the process i.e page no. 15**\n\n");
    page[12] = sbrk(PAGESIZE);
    printf(1, "page[12]=0x%x\n", page[13]);
    printf(1,"\nProcess details:\n");
    printStats();
    /*
	For this allocation, NFU will choose to move to 
    disk the page that hasn't been accessed the 
    longest (in this case page 1).
	Afterwards, page 1 is in the swap file, the rest
     are in memory.
	*/
    
    printf(1,"\n    **Allocating 17th page for the process ie page no. 16**\n\n");
	page[13] = sbrk(PAGESIZE);
	printf(1, "page[13]=0x%x\n", page[13]);
	printf(1,"\nProcess details:\n");
    printStats();
    /*For this allocation, NFU will choose to move to 
    disk the page that hasn't been accessed the longest
    (in this case page 3). Now, pages 1 & 3 are in the
    swap file, the rest are in memory. Note that no page
    fault should occur.
    */

    printf(1,"\n    ** Writing to page 3**\n\n");
    page[0][0] = 'a';
    printf(1,"\nProcess details:\n");
    printStats();
    /*For this access, NFU will choose to move to 
    disk the page that hasn't been accessed the longest
    (in this case page 4). Now, pages 1 & 4 are in the
    swap file, the rest are in memory. Thus one page
    fault should occur.
    PGFLT:1 
    */
    
    printf(1,"\n    **Allocating 18th page for the process ie page no. 17**\n\n");
	page[14] = sbrk(PAGESIZE);
	printf(1, "page[14]=0x%x\n", page[14]);
	printf(1,"\nProcess details:\n");
    printStats();
    /*For this allocation, NFU will choose to move to 
    disk the page that hasn't been accessed the longest
    (in this case page 5). Now, pages 1, 4 & 5 are in the
    swap file, the rest are in memory. Note that no page
    fault should occur. 
    */
   // Trying to cause a page fault by accessing the 5th page i.e page no. 4 which is in swap space
    printf(1,"\n    **Writing to pages 4 to 8 which causes page fault for every page**\n\n");
    for (i = 1; i < 6; i++) {
		printf(1, "Writing to address 0x%x\n", page[i]);
		for (j = 0; j < PAGESIZE; j++){
			page[i][j] = 'k';
		}
	}
    printf(1,"\nProcess details:\n");
    printStats();
    /*
    The above snippet will cause a total of 5 page faults.
	Accessing page no. 4 causes a PGFLT, since it is in the
    swap file. It would be hot-swapped with page 6 and next
    number 5 will be accessed which is in swap space and will
    hot-swapped with page 7 and then page 6 and so on. Thus,
    another PGFLT is invoked, and this process repeats 
    a total of 5 times.
    Total number of PGFLTs should thus be 1+5.
	*/

    printf(1,"\n    ***Testing for fork()***\n");
    if(fork() == 0){
        printf(1,"\n    **Running the child process now, PID: %d**\n",getpid());
        printf(1,"\nDetails of all processes\n");
        procDump();
        page[6][0] = 'J';
        printf(1,"\n    **Accessing page number 8 which is in the swap space**\n");
        printf(1,"\nDetails of all processes\n");
        procDump();
        exit();
    }
    else{
        wait();
        printf(1,"\n    **Child has exited. Back to the parent process now**\n");
        printf(1,"\n    **Deallocating all pages from the parent process**\n");
        sbrk(-15*PAGESIZE);
        printf(1,"\nDetails of all processes\n");
        procDump();
        exit();   
    }

#else
    #if FIFO
        printf(1,"  ***Testing for FIFO***\n");
    #elif SCFIFO
        printf(1,"  ***Testing for SCFIFO***\n");
    #endif
    printf(1,"\n    **Process Starts Executing**\n");
    int i,j;
    char *page[14];
    printf(1,"\nInitial process details:\n");
    printStats();
    printf(1,"\n    **Adding 12 more pages to the process**\n\n");
    //We already have 3 pages for the process in the main memory.
    //We will now allocate 12 more pages to get to the 15 page limit that we had set
    for(i = 0; i<12; i++){
        page[i] = sbrk(PAGESIZE);
        printf(1,"page[%d]=0x%x\n",i,page[i]);
    }
    printf(1,"\nProcess details:\n");
    printStats();

    
    //Adding one more page should replace page number 0 in memory with 12. 
    printf(1,"\n    **Allocating 16th page for the process i.e page no. 15**\n\n");
    page[12] = sbrk(PAGESIZE);
    printf(1,"page[%d]=0x%x\n",12,page[12]);
    //sbrk returning 61440 but below statement not getting called
    printf(1,"\nProcess details:\n");
    printStats();
    /*
    FIFO:
    Note that the number of page swaps are 2 
    bcz page 0 is swapped with page 16 and then 
    page fault occurs. Now page 0 will be swapped 
    with page 1. Now swap space has page 1.

    SCFIFO:
    Note that the number of page swaps will be 1
    bcz page 0 is the first candidate, however, 
    it is not swapped as it was used earlier and 
    hence given a second chance. Page 1 is swapped 
    into the memory. Now swap space has page 1. 
    */

   
    printf(1,"\n    **Allocating 17th page for the process i.e page no. 16**\n\n");
    page[13] = sbrk(PAGESIZE);
    printf(1,"page[13]=0x%x",page[13]);
    printf(1,"\nProcess details:\n");
    printStats();
    /* 
    FIFO:
    The number of page swaps should be 4 bcz page 17 
    is swpped for page 2 but page 2 contains user 
    stack so it will be swapped for page 3. Now swap 
    space has 2 pages i.e page 1 and page 3.

    SCFIFO:
    The number of page swaps should be 2 bcz now,
    page 3 will be swapped out into the swap space
    bcz page 2 was accessed. Now swap space contains
    page 1 and page 3. And accessbit for page 2 is
    set to 0. 
    */


    printf(1,"\n    ** Writing to page 3**\n\n");
    page[0][0] = 'a';
    printf(1,"\nProcess details:\n");
    printStats();
    /* 
    FIFO:
    Number of page swaps should be 5 since page 3 is
    in swap space,page fault will occur and page 4
    will be swapped out to swapped space and page 3
    into main memory.

    SCFIFO:
    Number of page swaps should be 3 Since page 3 is 
    in swap space,page fault will occur and page 4 
    will be swapped out to swapped space because 
    the access bit is 0 for page 4.
    */
    

    printf(1,"\n    **Allocating 18th page for the process i.e page no. 17**\n\n");
    page[14] = sbrk(PAGESIZE);
    printf(1,"page[14]=0x%x",page[14]);
    printf(1,"\nProcess details:\n");
    printStats();
    /* 
    FIFO:
    The number of page swaps should be 6 bcz page 17 
    is swpped for page 5. Now swap space has 3 pages
    i.e page 1, 4 and 5.

    SCFIFO:
    The number of page swaps should be 4 bcz now,
    page 5 will be swapped out into the swap space.
    Now swap space contains page 1, 4 and 5. And 
    accessbit for page 2 is
    set to 0. 
    */

    printf(1,"\n    **Trying to cause 6 more page faults by using pages from the swapspace**\n\n");
    for(i=1; i<6; i++){
        printf(1, "Writing to address 0x%x\n", page[i]);
        for(j=0; j<PAGESIZE; j++)
            page[i][j] = 'S';
    }
    printf(1,"\nProcess details:\n");
    printStats();
    /* 
    Note that this function calls page[i][j] which 
    needs the arr allocated but it was swapped out 
    to the disk. Hence, there is a page fault for 
    every iteration.
    
    FIFO: 
    Thus the total page faut count 
    goes to 3+5 = 8.

    SCFIFO:
    The total page fault count goes
    to 1+5 = 6.
    */


    printf(1,"\n    ***Testing for fork()***\n");
    if(fork() == 0){
        printf(1,"\n    **Running the child process now, PID: %d**\n",getpid());
        printf(1,"\nDetails of all processes\n");
        procDump();
        page[7][0] = 'J';
        printf(1,"\n    **Accessing page number 9 which is in swap space**\n");
        printf(1,"\nDetails of all processes\n");
        procDump();
        exit();
    }
    else{
        wait();
        printf(1,"\n    **Child has exited. Back to the parent process now**\n");
        printf(1,"\n    **Deallocating all pages from the parent process**\n");
        sbrk(-15*PAGESIZE);
        printf(1,"\nDetails of all processes\n");
        procDump();
        exit();   
    }
#endif

    return 0;
}