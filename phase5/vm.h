/*
 * vm.h
 */


/*
 * All processes use the same tag.
 */
#define TAG 0

/*
 * Different states for a page.
 */
#define UNUSED 500
#define INCORE 501
/* You'll probably want more states */


/*
 * Page table entry.
 */
typedef struct PTE {
    int  state;      // See above.
    int  frame;      // Frame that stores the page (if any). -1 if none.
    int  diskBlock;  // Disk block that stores the page (if any). -1 if none.
    // Add more stuff here
} PTE;

typedef struct Frame{
    int pid;
    int reference;
    int disk;
    int dirty;
    int page;
} Frame;
/*
 * Per-process information.
 */
typedef struct Process {
    int  numPages;   // Size of the page table.
    int pid;
    PTE  *pageTable; // The page table for the process.
    int mBox1;
    int mBox2;
    int mBox3;
    int mBox4;
    // Add more stuff here */
} Process;

/*
 * Information about page faults. This message is sent by the faulting
 * process to the pager to request that the fault be handled.
 */
typedef struct FaultMsg {
    int frame;
    int pPage;
    int  pid;        // Process with the problem.
    void* writeBuff;
    void* readBuff;
    void *addr;      // Address that caused the fault.
    Process* id;
    int page;
    int curDisk;
    int  replyMbox;  // Mailbox to send reply.
    // Add morie stuff here.
    int isDirty;            // if is Dirty
    int isReplace;      // if it is a frame replaced by other page
    int pDisk;  // previous disk for the previous page

} FaultMsg;

#define CheckMode() assert(USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE)
