## myMemTest.c

myMemTest.c tests the 3 page swapping algorithms and displays the pages in main memory and swap space after every step

(Note: Indexing of pages starts from 0, 1st page means page 0)

### NFU

In the NFU algorithm the page that gets selected to page out is the one that has not been accessed for the longest.



For NFU algorithm myMemTest does the following:

- The process starts
  - Initially 1 page is allocated for process memory
  - Subsequently 2 pages gets allocated for user stack
  - <img src="https://i.imgur.com/Iu8gjBD.png" style="width:400px;"/>

- Filling up main memory of the process by allocating pages
  - Allocating 12 more pages adds all the new pages to main memory and no page faults or page swap occurs
  - <img src="https://i.imgur.com/gQd1HRh.png" style="width:400px;"/>


- Allocating more pages to the process
  - Allocating next page causes page 1 to swap out of memory because page 0 was accessed before but page 1 was not.( page 2 is also accessed in the stack and the stack has not reached page 1, so it was not accessed )
    - Thus causing 1 page swap
    - <img src="https://i.imgur.com/BmZ8BNw.png" style="width:400px;"/>
  - Allocating next page causes page 3 to swap out because page 2 was accessed but page 3 was not.
    - This causes 1 page swap
    - <img src="https://i.imgur.com/2yzgxUP.png" style="width:400px;"/>
  - Page 1 and Page 3 are in swap space



- Accessing paged out page
  - Accessing page 3 causes page fault because it was swapped out
  - Page 4 is selected for page swap and now page 3 is in memory and page 1 and page 4 are in swap space
  - <img src="https://i.imgur.com/ebCOrl1.png" style="width:400px;"/>

- Allocating one more page
  - Allocating another page causes page 5 to swap out of main memory 
  - Not page 1, 4 and 5 are in main memory
  - <img src="https://i.imgur.com/oxuWogL.png" style="width:400px;"/>

- Accessing paged out pages
  - Now accessing page 4 to 8 iteratively causes page faults for first access of each page
  - First page 6 gets swapped out to swap space and page 4 into main memory
  - Then page 7 gets swapped out to swap space and page 5 into main memory
  - Similarly on access of page 6,7 and 8 page fault happens
  - After this page 1, 9 and 10 are in swap space
  - So page fault count increases to 6(1+5)
  - <img src="https://i.imgur.com/VpZAV8v.png" style="width:400px;"/>


- Forking the process
    - On forking the process , copyuvm function is called which copies the page directory to the new process.
  - In order to handle copy on write, a new swap file is created for the child process.
  - <img src="https://i.imgur.com/TsnvQSx.png" style="width:400px;"/>


- Accessing a paged out page in child process
  - On accessing page 9 in child process that was in swap space before forking, page fault occurs and is swapped with page 11 in main memory. Hence the number of page faults and page swap counts for the child process increase to 1. 
  - <img src="https://i.imgur.com/8WGL2NQ.png" style="width:400px;"/>


- Clearing the allocated pages from parent process after child process exits
  - After removing all the allocated pages, initial 3 pages remain
  - page 0 and 2 are in main memory
  - page 1 is in swap space
  - <img src="https://i.imgur.com/Ru5yPMv.png" style="width:400px;"/>





### FIFO

In FIFO algorithm the page that was first allocated in main memory is selected first for swapping ( **First** page **In** main memory **First** page to get swapped **Out** )

- The process starts
  - Initially 1 page is allocated for process memory
  - Subsequently 2 pages gets allocated for user stack
  - <img src="https://i.imgur.com/fxACUNp.png" style="width:400px;"/>


- Filling up main memory of the process by allocation pages
  - Allocating 12 more pages adds all the new pages to main memory and no page faults or page swap occur
  - <img src="https://i.imgur.com/lQfzdAH.png" style="width:400px;"/>



- Allocating more pages to the process
  - Allocating page 16
    - Allocating next page causes page 0 to swap to swap space because it was the first page
    - This causes page fault and page 0 is swapped with page 1 in the memory
    - Thus page 1 is in swap space and 1 page fault has occurred
    - <img src="https://i.imgur.com/uetjBCb.png" style="width:400px;"/>
  - Allocating page 17
    - This causes page 2 to swap to swap space and page fault occurs because user stack is stored in page 2
    - Thus page 3 is hot-swapped with page 2 
    - 1 more page fault occurred
    - <img src="https://i.imgur.com/DOVANOP.png" style="width:400px;"/>
  - Page 1 and Page 3 are in swap space
  - Page Fault Count = 2

- Accessing paged out page
  - Accessing page 3 causes page fault because it was swapped out
  - Page 4 is selected for page swap and now page 3 is in memory and page 1 and page 4 are in swap space
  - Page Fault count = 3
  - <img src="https://i.imgur.com/YLUJ585.png" style="width:400px;"/>


- Allocating one more page
  - Allocating another page causes page 5 to swap out of main memory 
  - Now page 1, 4 and 5 are in main memory
  - <img src="https://i.imgur.com/Nwh6cJ9.png" style="width:400px;"/>


- Accessing paged out pages
  - Now accessing page 4 to 8 iteratively causes page faults for first access of each page
  - First page 6 gets swapped out to swap space and page 4 into main memory
  - Then page 7 gets swapped out to swap space and page 5 into main memory
  - Similarly on access of page 6,7 and 8 page fault happens
  - After this page 1, 9 and 10 are in swap space
  - So page fault count increases to 8(3+5)
  - <img src="https://i.imgur.com/TwmrIwb.png" style="width:400px;"/>



- Forking the process
  - On forking the process , copyuvm function is called which copies the page directory to the new process.
  - In order to handle copy on write, a new swap file is created for the child process.
  - <img src="https://i.imgur.com/O7XmuEY.png" style="width:400px;"/>




- Accessing a paged out page in child process
  - On accessing page 9 in child process that was in swap space before forking, page fault occurs and is swapped with page 11 in main memory
  - <img src="https://i.imgur.com/xkbn243.png" style="width:400px;"/>



- Clearing the allocated pages from parent process after child process exits
  - After removing all the allocated pages, initial 3 pages remain
  - page 0 and 2 are in main memory
  - page 1 is in swap space
  - <img src="https://i.imgur.com/ewOaXzi.png" style="width:400px;"/>



### SCFIFO

In FIFO algorithm the page that was first allocated in main memory will get selected first for swapping, but when accessed the page accessbit is set to 1. When a page is selected to get swapped out and has accessbit 0, it gets swapped out but if it is 1, then the accessbit is set to 0 and the next page is selected for swapping.

- The process starts
  - Initially 1 page is allocated for process memory
  - Subsequently 2 pages gets allocated for user stack
  - <img src="https://i.imgur.com/B9i7Zgw.png" style="width:400px;"/>



- Filling up main memory of the process by allocation pages
  - Allocating 12 more pages adds all the new pages to main memory and no page faults or page swap occurs
  - <img src="https://i.imgur.com/WYH5Kkd.png" style="width:400px;"/>




- Allocating more pages to the process
  - Allocating page 16
    - Allocating next page selects page 0 to swap but page 0 has accessbit set 1, thus accessbit is set to 0
    - Page 1 is then selected to swap and has accessbit set 0 thus gets swapped to swap space
    - Thus page 1 is in swap space
    - <img src="https://i.imgur.com/htr28wf.png" style="width:400px;"/>
  - Allocating page 17
    - This selects page 2 to swap to swap space but page 2 has accessbit set 1, thus accessbit is set to 0
    - Thus page 3 is swapped out of main memory because it had accessbit 0
    - <img src="https://i.imgur.com/CWkd4K1.png" style="width:400px;"/>
  - Page 1 and Page 3 are in swap space
  - Page Fault Count = 0





- Accessing paged out page
  - Accessing page 3 causes page fault because it was swapped out
  - Page 4 is selected for page swap and has accessbit set 1 
  - So now page 3 is in memory and page 4 gets swapped into swap space
  - Now Page 1 and Page 4 are in swap space
  - Page Fault count = 1
  - <img src="https://i.imgur.com/5Rw0bsT.png" style="width:400px;"/>



- Allocating one more page
  - Allocating another page causes page 5 to swap out of main memory 
  - Page 5 had accessbit set 0
  - Now page 1, 4 and 5 are in main memory
  - <img src="https://i.imgur.com/h7wpd7d.png" style="width:400px;"/>



- Accessing paged out pages
  - Now accessing page 4 to 8 iteratively causes page faults for first access of each page
  - First page 6 gets swapped out to swap space and page 4 into main memory because page 6 has accessbit set 0
  - Then page 7 gets swapped out to swap space and page 5 into main memory because page 7 has accessbit set 0
  - Similarly on access of page 6,7 and 8 page fault happens
  - After this page 1, 9 and 10 are in swap space
  - So page fault count increases to 6(1+5)
  - <img src="https://i.imgur.com/HATtNoS.png" style="width:400px;"/>


- Forking the process
  - On forking the process all pages gets duplicated and those pages that were swapped out before remains swapped out
  - <img src="https://i.imgur.com/N1R9ddG.png" style="width:400px;"/>



- Accessing a paged out page in child process
  - On accessing page 9 in child process that was in swap space before forking, page fault occurs and is swapped with page 11 in main memory
  - <img src="https://i.imgur.com/DgWdHTw.png" style="width:400px;"/>



- Clearing the allocated pages from parent process after child process exits
  - After removing all the allocated pages, initial 3 pages remain
  - page 0 and 2 are in main memory
  - page 1 is in swap space
  - <img src="https://i.imgur.com/w4uxk7l.png" style="width:400px;"/>


We give due credits to https://github.com/mit-pdos/xv6-public for the implementation of the kernel of Xv6 OS that we have used in our project. In this project, we extend the strength of memory management unit of Xv6 kernel by implementing support for demand paging. We implement and compare various page replacement schemes.

## Implementation Details
### Running the code:
make clean && make qemu-nox SELECTION=FLAG1 VERBOSE_PRINT=FLAG2
### Structs:
**freepg**: Node of linked lists of pages (max = 15) present in the main memory and the swap space.
- <img src="https://i.imgur.com/p5Vd1Ck.png"/>

**proc:**
- Added arrays of freepg for both main memory and physical memory.
- Added other metadata like # of swaps, # of pages in main memory and swap space, # page faults.
- <img src="https://i.imgur.com/L50HWfO.png" style="width:400px;"/>


### Functions changed:

- **allocproc(void) and exec():** allocproc is responsible for searching an empty process entry in ptable array while exec is responsible for executing it.
<u>Changes done:</u> Initialized all the process meta data to 0 and all the addresses to 0xffffffff. If the NONE flag is not defined, exec creates a swap file for every process other than init and sh.
- **allocuvm():** Responsile for allocating the memory for the process.
<u>Changes done:</u> while allocating changes, tracks if the number of pages in the physical memory exceeds 15. If so, calls the required paging scheme through the **writePagesToSwapFile** function and later updates the number of pages in the processes' meta data using the **recordNewPage** function. Also called in **sbrk()** system call to allow a process to allocate its own pages.
- **deallocuvm():** deallocates from the physical memory and frees both the free_pages and the swap_file_pages arrays of the process it is called for. Decreaments the counts of pages in both main memory and swap space. Also called by **sbrk()** system call when supplied with negtive # of pages to allow a process to deallocate its own pages.
- **fork() system call:** Copies the meta data of a process including swap_space_pages and free_pages arrays, # of pages in swap file and main memory to the child process. However, # page faults and # page swaps are not copied to the child. Creates a separate swap file for the child process to hold copies of the swapped out pages. This is done to enable **copy-on-write**. The codelet for making swap file for child process is:  
    - <img src="https://i.imgur.com/HoIX8AD.png" style="width:400px;"/>

- **exit() system call:** Deletes the meta data associated with the process. Closes the swap file and removes the pointers. Sets all the # entries to 0.
    - <img src="https://i.imgur.com/8cLVS2W.png" style="width:400px;"/>

- **procdump():** Modified with **custom_proc_print()** function to print the counts of pages in swap space and in main memory, page faults, number of swaps and percentage of free pages in the physical memory.  
- **trap(struct trapframe** ***tf):** Modified to call **updateNFUState()** function in case of timer interrupts to increase the "age" of pages when NFU paging scheme is being used. Introduced changes to handle a page fault by defining T_PGFLT for trap 14. On T_PGFLT being called, it executes the supplied service routine handled by the **swapPages()** function. The codelet for handling pagefault is as follows:
    - <img src="https://i.imgur.com/Vcgh5cu.png" style="width:400px;"/>

### New functions created:
- **WritePagesToSwapFile(char** ***va):** This function calls the required paging scheme (FIFO, NFU, SCFIFO) depending upon the flag that was set during make. Incoming page is written into the physical memory and a candidate page (selected according to the paging scheme) is removed from the main memory and wriiten to the swap space.
- **recoredNewPage(char** ***va):** Writes the meta-data of the currently added page into the corresponding entry in the free_pages array of the process. Increases the count of pages in the physical memory.
- **swapPages(char** ***va):** Fetches the page with the given virtual address "va" from the swap space and finds a candidate page to be swapped out of the main memory and into the swap space based on (FIFO, NFU, SCFIFO) according to the flag set during make. 
- **printStats() and procDump() system calls:**  Print the details of the current process and all current processes respectively as per asked by the **Enhanced Details Viewer** section. Call **custom_proc_print()** and **procDump()** functions respectively. Used in myMemTest.c to print the results and do away with **ctrl+P** during execution.

### Extra Observations:
1. If we  add extra tabs to the printf() statements in myMemTest eg: printf("\*\*&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;Process starts executing&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;\*\*"), initial pages allocated to a page are 4 instead of usual 3. The reason for this can be that the page 0 contains program memory and adding extra tabs is equivalent to adding extra spaces which intern will increase the program memory.
2. One problem that we rectified but did not completely understand was that we intially set all the process metadata to 0 in exec function. The problem we encountered with this was that myMemTest would run as expected for the first time. However, during the second call to the program, it would hang on the fork system call.
    - This problem got rectified when we initialized process meta data to 0 in allocproc along with exec. However, it does not work if we remove that piece of code from either of the two functions.
3. The method used by XV6 for checking permissions using the last 12 bits seems to be a striking feature. 
4. Created system calls printStats() and procDump() for printing the paging details of processes in myMemTest.c to waive away the manual work needed to press ctrl+P in order to display results. However, ctrl+P will also work wherever required without any hinderance and will also contain paging details as pointed out by Enhanced details viewer section.
