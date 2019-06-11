  /*
 * Name: Francis Kim (solo)
 * Instructor: Professor Patrick
 * testcases for solo: 1, 2, 3, 4, 7, and 8
 * Class: csc452
 * Description:For this phase of the project you will implement a virtual memory (VM) system that supports demand   paging.  
 * The   USLOSS   MMU   is   used   to   configure   a   region   of   virtual   memory   whose contents are process-specific.
 * You will use the USLOSS MMU to implement the virtual memory system. 
 * The basic idea is to use the MMU to implement a single-level page table, 
 * so that each process will have its own page table for the VM region and will therefore have its own view of what the VM region contains. 
 * The fancy features of the MMU, such as the tags and the protection bits, will not be needed.
 */


#include <usyscall.h>
#include <assert.h>
#include <phase5.h>
#include <libuser.h>
#include <string.h>
#include <usloss.h>
  
extern void mbox_create(USLOSS_Sysargs *args_ptr);
extern void mbox_release(USLOSS_Sysargs *args_ptr);
extern void mbox_send(USLOSS_Sysargs *args_ptr);
extern void mbox_receive(USLOSS_Sysargs *args_ptr);
extern void mbox_condsend(USLOSS_Sysargs *args_ptr);
extern void mbox_condreceive(USLOSS_Sysargs *args_ptr);

Process ProcTable[MAXPROC];

FaultMsg faults[MAXPROC]; /* Note that a process can have only
                           * one fault at a time, so we can
                           * allocate the messages statically
                           * and index them by pid. */
VmStats  vmStats;

Frame* frameTable;
extern void* vmRegion;
int mboxPager;
int exit1=0;
int handClock;
int diskCount=0;

static void FaultHandler(int type, void * arg);
//static int Pager(char *buf);
static void vmInit(USLOSS_Sysargs *USLOSS_SysargsPtr);
static void vmDestroy(USLOSS_Sysargs *USLOSS_SysargsPtr);
static int Pager(char *buf);


/*
	setUpFrameTable(int frames)
	initialize the frame table
*/
void setUpFrameTable(int frames){

	for(int i=0; i<frames; i++){
		frameTable[i].page=-1;
		frameTable[i].reference=0;
		frameTable[i].dirty=0;
	}

}

/*
	void setUpVmStats(int mappings, int pages, int frames, int pagers)
	initialize the vm stats 
*/
void setUpVmStats(int mappings, int pages, int frames, int pagers){

   int sector;
   int track;
   int disk;

    memset((char *) &vmStats, 0, sizeof(VmStats));
    diskSizeReal(1,&sector,&track,&disk);
	vmStats.frames = frames;
    vmStats.pages = pages;
    vmStats.diskBlocks = disk*track*sector/USLOSS_MmuPageSize();
	vmStats.pagers = pagers;
    vmStats.freeDiskBlocks = disk*2;
    vmStats.pageSize= USLOSS_MmuPageSize();
}

/*
	void updateInfo(FaultMsg *msg, Frame *frame)
	update informations 
*/
void updateInfo(FaultMsg *msg, Frame *frame){
	msg->frame = handClock;
    msg->pPage = frame->page;
    frame->page = msg->page;   
}

/* 
	Process* getProcTable(int pid)
	return current process table
*/
Process* getProcTable(int pid){

	return &ProcTable[pid%MAXPROC];
}

/*
	Frame* getFrameTable(int index)
	get current frame table
*/
Frame* getFrameTable(int index){
	return &(frameTable[index]);
}

/*
 void ClockAlgorithm(FaultMsg *msg)
 it is clock algorithm which find the empty frame or replaceable frame.
*/
void ClockAlgorithm(FaultMsg *faultMsg){

    // Set up the essential variable
    int newFrame=0; 
	int cleanFrame=0;
	int temp;
	int ref=0;
	int tempCal1;
	int tempCal2;
    Frame *f;
    PTE *pPage,*cPage;
    Process* processId;
 
    processId = faultMsg->id;
	
	int index =0;
	// it will find a empty or replaceable frame, and save true or false into newframe or cleanframe
	while(index < vmStats.frames){

			USLOSS_MmuGetAccess(index,&temp);

			tempCal1 = temp%2;	// calculate dirty value
			tempCal2 = temp%4/2; // calculate reference value
		if( ref>=0){
			frameTable[index].dirty = tempCal2;
			frameTable[index].reference = tempCal1;
		}
        if(frameTable[index].page < 0 && ref >= 0){
            newFrame=true;	// newFrame has true
		}
        else if(frameTable[index].reference == 0 && frameTable[index].dirty == 0 && ref >= 0){
            cleanFrame=true; // cleanFrame has true
		}else{
			ref++;
		}
		index++;	// increase index by 1

	}
    int confirmNewFrame;
	int confirmCleanFrame;
    // go next from clock handler
    // find the empty frame or replacable frame
	do{

        f = getFrameTable(handClock);
		
		if(ref >= 0){
		confirmNewFrame = newFrame;
		confirmCleanFrame = cleanFrame;
		}
        //  Frame is empty
        if(confirmNewFrame == true && ref >= 0){
			if(f->page >= 0){
				ref++;
			}else if(f->page < 0){
				vmStats.freeFrames++;
                faultMsg->isReplace = false;
				ref++;
				faultMsg->isDirty = false;
                break;
            }
			f->reference = 0;
			USLOSS_MmuSetAccess(handClock ,2*f->dirty);
			handClock = (handClock+1)%vmStats.frames;
        }
        //  frame is unreferenced, and clean
        else if(confirmCleanFrame == true && ref >= 0){
			if(ref < 0){
				ref++;
			}else if(f->reference == 0 && f->dirty == 0 && ref >= 0){
                faultMsg->isDirty = false;
				ref++;
                faultMsg->isReplace = true;
                break;
            }
			f->reference = 0;
			USLOSS_MmuSetAccess(handClock ,2*f->dirty);
			handClock = (handClock+1)%vmStats.frames;
        }
        // Frame is on disk operations, print out solo status
        else if(f->reference == 0){
            if(ref >= 0 && f->reference == 0 ){
                USLOSS_Console("Solo phase 5, no disk operations\n");
				USLOSS_Halt(1);
            }
			f->reference = 0;
			USLOSS_MmuSetAccess(handClock ,2*f->dirty);
			handClock = (handClock+1)%vmStats.frames;

        }else{
			f->reference = 0;
			USLOSS_MmuSetAccess(handClock ,2*f->dirty);
			handClock = (handClock+1)%vmStats.frames;
		}

    }while(1);


	updateInfo(faultMsg, f);

    f->pid = getpid();
	
    cPage = &(processId->pageTable[faultMsg->page]);

    if(cPage->state >= 0){
        //nothing
    }else if(cPage->state < 0){
		cPage->state = 0;
		vmStats.new++;
	}

	int faultMsgFrame = faultMsg->frame;
	int cPageDiskBlock = cPage->diskBlock;

	if(ref >= 0){
			cPage->frame = faultMsgFrame;
			faultMsg->curDisk = cPageDiskBlock;
	
			if(faultMsg->isReplace){
				pPage = &(processId->pageTable[faultMsg->pPage]);
				pPage->frame = -1;
				int pPageDiskBlock = pPage->diskBlock;
				faultMsg->pDisk = pPageDiskBlock;
			}
	}else{
			if(cPage->state >= 0){
				//nothing
			}else if(cPage->state < 0){
				cPage->state = 0;
				vmStats.new++;
			}
	}
} /* ClockAlgorithm */


/*
 * void vmDestroyReal(void)
 * Release all the mbox pager and free frame table, it is called from vmDestroy.
 */
void vmDestroyReal(void)
{
		char empty = "";
		CheckMode();
		int current;

		// Kill the pagers here
        exit1 = 1;
        // print vm statistics
        PrintStats();
		int start=0;

		// count until pagers reach
		while(start < vmStats.pagers){
			MboxSend(mboxPager, empty, 0); // send mbox pager with empty string
			waitReal(&current);	
			start++;
		}
        
		if( start >= 0){
			USLOSS_MmuDone();
			vmRegion=NULL;
			MboxRelease(mboxPager); // releast all the mail box
			free(frameTable);	// free frame table
			handClock = 0; // set handClock = 0
			USLOSS_PsrSet(USLOSS_PsrGet() | 0b00011);
		}else if(exit1 == 1){
			USLOSS_PsrSet(USLOSS_PsrGet() | 0b00011);
		}else{
			MboxRelease(mboxPager); // releast all the mail box
			free(frameTable);	// free frame table
			handClock = 0; // set handClock = 0
			USLOSS_PsrSet(USLOSS_PsrGet() | 0b00011);

		}


} /* vmDestroyReal */


/*
 * void vmInitReal(int mappings, int pages, int frames, int pagers)
 * 1. set up the page table
 * 2. Initialize vm system with MMU
 * 3. it is called from vmInit
 */
void *vmInitReal(int mappings, int pages, int frames, int pagers)
{
   int status;
   int dummy;
   

   CheckMode();
   status = USLOSS_MmuInit(mappings, pages, frames, USLOSS_MMU_MODE_TLB);
   if (status != USLOSS_MMU_OK) {
      USLOSS_Console("vmInitReal: couldn't initialize MMU, status %d\n", status);
      abort();
   }

   USLOSS_IntVec[USLOSS_MMU_INT] = FaultHandler;
    exit1 = 0;
	// allocate memory to frame table
	frameTable = malloc(sizeof(Frame) * frames);

   // set up frame table 
	setUpFrameTable(frames);
 
   // create mbox pager
    mboxPager = MboxCreate(MAXPROC, 128);
    handClock = 0;

   // fork pager
   for(int i=0; i<pagers; i++){
	fork1("Pager", Pager, NULL, USLOSS_MIN_STACK, 2);
   }

   // Initialize the vmStats after zero out
   setUpVmStats(mappings, pages, frames, pagers);
   
   return USLOSS_MmuRegion(&dummy);
 } /* vmInitReal */

/*
 *----------------------------------------------------------------------
 *
 * start4 --

 * Initializes the VM system call handlers. 
 *
 * Results:
 *      MMU return status
 *
 * Side effects:
 *      The MMU is initialized.
 *
 *----------------------------------------------------------------------
 */
int
start4(char *arg)
{
    int pid;
    int result;
    int status;

    /* to get user-process access to mailbox functions */
    systemCallVec[SYS_MBOXCREATE]      = (void*)mbox_create;
    systemCallVec[SYS_MBOXRELEASE]     = (void*)mbox_release;
    systemCallVec[SYS_MBOXSEND]        = (void*)mbox_send;
    systemCallVec[SYS_MBOXRECEIVE]     = (void*)mbox_receive;
    systemCallVec[SYS_MBOXCONDSEND]    = (void*)mbox_condsend;
    systemCallVec[SYS_MBOXCONDRECEIVE] = (void*)mbox_condreceive;

    /* user-process access to VM functions */
    systemCallVec[SYS_VMINIT]    = (void*)vmInit;
    systemCallVec[SYS_VMDESTROY] = (void*)vmDestroy; 
    result = Spawn("Start5", start5, NULL, 8*USLOSS_MIN_STACK, 2, &pid);
    if (result != 0) {
        USLOSS_Console("start4(): Error spawning start5\n");
        Terminate(1);
    }

	// Wait for start5 to terminate
    result = Wait(&pid, &status);
    
	if (result != 0) {
        USLOSS_Console("start4(): Error waiting for start5\n");
        Terminate(1);
    }
    Terminate(0);
    return 0; // not reached

} /* start4 */

/*
	int getMappings(USLOSS_Sysargs *USLOSS_SysargsPtr)
	get value of arg1
*/

int getMappings(USLOSS_Sysargs *USLOSS_SysargsPtr){
	return (long) USLOSS_SysargsPtr->arg1;
}

/*
	int getPages(USLOSS_Sysargs *USLOSS_SysargsPtr)
	get value of arg2
*/

int getPages(USLOSS_Sysargs *USLOSS_SysargsPtr){
	return (long) USLOSS_SysargsPtr->arg2;
}

/*
	int getFrames(USLOSS_Sysargs *USLOSS_SysargsPtr)
	get value of arg3
*/

int getFrames(USLOSS_Sysargs *USLOSS_SysargsPtr){
	return (long) USLOSS_SysargsPtr->arg3;
}

/*
	int getPagers(USLOSS_Sysargs *USLOSS_SysargsPtr)
	get value of arg4
*/

int getPagers(USLOSS_Sysargs *USLOSS_SysargsPtr){
	return (long) USLOSS_SysargsPtr->arg4;
}
/*
 *----------------------------------------------------------------------
 *
 * VmInit --
 *
 * Stub for the VmInit system call.
 *
 * Results:
 *      None.
 *
 * Siide effects:
 *      VM system is initialized.
 *
 *----------------------------------------------------------------------
 */
static void vmInit(USLOSS_Sysargs *USLOSS_SysargsPtr)
{
    CheckMode();
	int mappings, pages, frames, pagers;

	// get value of mappings, pages, frames, and pagers
	mappings = getMappings(USLOSS_SysargsPtr);
	pages = getPages(USLOSS_SysargsPtr);
	frames = getFrames(USLOSS_SysargsPtr);
	pagers = getPagers(USLOSS_SysargsPtr);
    
    USLOSS_SysargsPtr->arg4 = (void*) (long) 0;
	
    // arg1 is first byte in address in VM region (execute vmInitReal)
    USLOSS_SysargsPtr->arg1 = vmInitReal(mappings, pages, frames, pagers);

    // set up usermode
    USLOSS_PsrSet(USLOSS_PsrGet() &0b1111110);
} /* vmInit */


/*
 *----------------------------------------------------------------------
 *
 * vmDestroy --
 *
 * Stub for the VmDestroy system call.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      VM system is cleaned up.
 *
 *----------------------------------------------------------------------
 */

static void
vmDestroy(USLOSS_Sysargs *USLOSS_SysargsPtr)
{
   CheckMode();
   // If vmRegion is not null, then execute vmDestroyReal()
   if(vmRegion!=NULL){
	vmDestroyReal();
   }

   // save 0 into arg1 which is mapping
   USLOSS_SysargsPtr->arg1 = (void*)(long) 0;

   USLOSS_PsrSet(USLOSS_PsrGet() & 0b1111110);
} /* vmDestroy */





/*
 *----------------------------------------------------------------------
 *
 * PrintStats --
 *
 *      Print out VM statistics.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      Stuff is printed to the USLOSS_Console.
 *
 *----------------------------------------------------------------------
 */
void
PrintStats(void)
{
     USLOSS_Console("VmStats\n");
     USLOSS_Console("pages:          %d\n", vmStats.pages);
     USLOSS_Console("frames:         %d\n", vmStats.frames);
     USLOSS_Console("diskBlocks:     %d\n", vmStats.diskBlocks);
     USLOSS_Console("freeFrames:     %d\n", vmStats.freeFrames);
     USLOSS_Console("freeDiskBlocks: %d\n", vmStats.freeDiskBlocks);
     USLOSS_Console("switches:       %d\n", vmStats.switches);
     USLOSS_Console("faults:         %d\n", vmStats.faults);
     USLOSS_Console("new:            %d\n", vmStats.new);
     USLOSS_Console("pageIns:        %d\n", vmStats.pageIns);
     USLOSS_Console("pageOuts:       %d\n", vmStats.pageOuts);
     USLOSS_Console("replaced:       %d\n", vmStats.replaced);
} /* PrintStats */

/*
	


 */
void freeFaultMsg(FaultMsg *faultMsg){
	free(faultMsg->writeBuff);
    free(faultMsg->readBuff);
    free(faultMsg);
}

/*
 *
 * static void FaultHandler(int type, void*arg)
 * Saved all information about fault in queue, wakes up the pager, and it blocks
 * until fault received. Also, it works MMU interrupt.
 *
 */
static void FaultHandler(int type /* MMU_INT */, void* arg  /* Offset within VM region */)
{
   int ref=1;
   int mmu;
   FaultMsg *faultMsg;
   Process* id = getProcTable(getpid()); //get current process table;

   if(type == USLOSS_MMU_INT){
		assert(type == USLOSS_MMU_INT);
   }else{
	   assert(type != USLOSS_MMU_INT);
   }
   if(ref <= 1){
   mmu = USLOSS_MmuGetCause();
   assert(mmu == USLOSS_MMU_FAULT);
   vmStats.faults++;
   }else{
	   vmStats.faults--;
   }
   
    // Initialize message for the pager
    int faultMsgSize = sizeof(FaultMsg); 
	faultMsg=malloc(faultMsgSize);
    faultMsg->id=id;
	int pageSize = ((int) (long) arg)/vmStats.pageSize;
    faultMsg->page = pageSize;
	int currentPid = getpid();
    faultMsg->id->pid = currentPid;
	int writeBuffSize = vmStats.pageSize*sizeof(char);
    faultMsg->writeBuff = malloc(writeBuffSize);
	int readBuffSize = vmStats.pageSize*sizeof(char);
    faultMsg->readBuff = malloc(readBuffSize);
    memset(faultMsg->readBuff, 0, vmStats.pageSize); 

     // block mbox and wakes up the pager
    MboxSend(mboxPager, &faultMsg, sizeof(FaultMsg*));
    MboxReceive(id->mBox1,"", 0);

	// if fault message is replaceable, then
    if(faultMsg->isReplace){
        USLOSS_MmuUnmap(0, faultMsg->pPage); // unmapping
    }

    // mapping for frame
    if(faultMsg->isReplace && ref >= 0){
		USLOSS_MmuMap(0, faultMsg->page, faultMsg->frame,USLOSS_MMU_PROT_RW);
	}else{
		USLOSS_MmuMap(0, faultMsg->page, faultMsg->frame,USLOSS_MMU_PROT_RW);
		ref++;
	}

    // block mbox and wakes up the pager
    MboxSend(id->mBox2, "", 0);
    MboxReceive(id->mBox1, "", 0);

    // write the readBuffer to the frame
	int sizeOfPage = vmStats.pageSize;
    memcpy(vmRegion+faultMsg->page*vmStats.pageSize, faultMsg->readBuff, sizeOfPage);
    USLOSS_MmuSetAccess(faultMsg->frame,0);

    // free the malloc
	freeFaultMsg(faultMsg);
    

} /* FaultHandler */


/*
 *----------------------------------------------------------------------
 *
 * Pager 
 *
 * Kernel process that handles page faults and does page replacement.
 *
 * Results:
 * None.
 *
 * Side effects:
 * None.
 *
 *----------------------------------------------------------------------
 */
static int
Pager(char *buf)
{
// initialization
    FaultMsg *faultMsg;
    int mailBox1;
	int mailBox2;
	int mailBox3;
	int mailBox4;
	char empty = "";
    USLOSS_PsrSet(USLOSS_PsrGet() | 0x2);
    do {
        // blocked here
        MboxReceive(mboxPager, &faultMsg,sizeof(void*));

        // check if the process is quiting
        if(exit1){
            quit(1);
	}
        // initialize mail box
        mailBox1=faultMsg->id->mBox1;
        mailBox2=faultMsg->id->mBox2;
        mailBox3=faultMsg->id->mBox3;
        mailBox4=faultMsg->id->mBox4;

        
		// find a empty fram or replacable frame using clock algorithm
        MboxReceive(mailBox3, empty, 0 );
        ClockAlgorithm(faultMsg);

        MboxSend(mailBox3, empty, 0 );
        MboxSend(mailBox1, empty, 0);

        // Read frame from disk
        MboxReceive(mailBox2,empty,0);
        MboxReceive(mailBox4, empty, 0);
  		if(faultMsg->curDisk < 0){
 			//Nothing
		}else if(faultMsg->curDisk >=0){
			vmStats.pageIns++;
			int curdisk1 = faultMsg->curDisk/2;
			int curdisk2 = faultMsg->curDisk%2*8;
            		diskReadReal(1, faultMsg->curDisk/2 , curdisk2, 8, faultMsg->readBuff);
		}
        
        MboxSend(mailBox4,empty, 0);
        MboxSend(mailBox1, empty, 0);
    } while(true);
    return 0;

} /* Pager */
