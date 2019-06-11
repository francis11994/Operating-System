  /* ------------------------------------------------------------------------
   phase1.c

   Francis Kim and Andrew Carlisle

   University of Arizona
   Computer Science 452
   Fall 2015

   ------------------------------------------------------------------------ */

#include "phase1.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "kernel.h"

/* ------------------------- Prototypes ----------------------------------- */
       int     sentinel (char *);
extern int     start1 (char *);
       void    dispatcher(void);
       void    launch();
static void    checkDeadlock();
       int     blockMe(int newStatus);
       int     unblockProc(int pid);
       int     removeInReadyList(int pid);
       void    makeReadyList(int procSlot);
       void    changeStatus(int pid, int status);
       procPtr checkChildQuit();
static int     joinBlockMe();
static void    unjoinblockProc(int pid);
static void    removeChild(int childPid);
       void    dumpProcesses(void);
static void    initializeProcessTable(void);
       int     zap(int pid);
       int     isZapped(void);
static void    zapBlockMe(void);

/* -------------------------- Globals ------------------------------------- */

// Patrick's debugging global variable...
int debugflag = 0;

// The number of processes currently in the process table
int numProcesses = 0;

// the process table
procStruct ProcTable[MAXPROC];

// Process lists
static procPtr ReadyList;

// current process ID
procPtr Current;

// the next pid to be assigned
unsigned int nextPid = SENTINELPID;

void clock_handler(){
}

int readTime(){
	// unit is always 0
        int status=0;
	int unit = 0;
	int dev = 0;
	int result = USLOSS_DeviceInput(dev, unit, &status);

	if(result == USLOSS_DEV_INVALID){
		USLOSS_Console("USLOSS_DEV_INVALID....Halt...\n");
		USLOSS_Halt(1);
	}

	return	status;
}

int getpid(){
	
	int pid = Current->pid;

	return pid;
}
void changeStatus(int pid, int status){
	int slot = 0;
	while(ProcTable[slot].pid != pid){
		slot++;
	}

	ProcTable[slot].status = status;
}

procPtr checkChildQuit(){

    if (DEBUG && debugflag) {
        USLOSS_Console("    checkChildQuit: started\n");
    }

	
	procPtr child = Current->quitChildList;

	if(child == NULL){
		return child;
	}else{
		Current->quitChildList = child->nextQuitSibling;
		child->nextQuitSibling = NULL;	
		return child;
	}
}
void connectChild(int procSlot){

	procPtr parent = Current;
        procPtr child = &ProcTable[procSlot];

        if(parent->childProcPtr == NULL){
                parent->childProcPtr = child;
                child->childProcPtr = NULL;
        }else if(parent->childProcPtr->priority > child->priority){
                child->nextSiblingPtr = parent->childProcPtr;
                parent->childProcPtr = child;
        }else{
                procPtr temp;
                temp = parent->childProcPtr;
                while(temp->nextSiblingPtr != NULL && temp->priority <= child->priority){
                        temp = temp->nextSiblingPtr;
                }
                child->nextSiblingPtr = temp->nextSiblingPtr;
                temp->nextSiblingPtr = child;
        }


}


void makeReadyList(int procSlot){
    if (DEBUG && debugflag)
        USLOSS_Console("    Readying process in slot %d\n", procSlot);

	procPtr temp;
	ProcTable[procSlot].status = READY;
	
        // Place the ready process in the ready list in the proper position
        //   (in order of priority)
        // Case 1: ReadList is empty, add the process and update nextProc ptr
	if(ReadyList == NULL){
		ReadyList = &ProcTable[procSlot];
		ReadyList->nextProcPtr = NULL;
	}
        //  Case 2: The first element of the list has lower or equal priority
        //    to the element we are adding 
        else if ( ReadyList->priority > ProcTable[procSlot].priority) {
        	ProcTable[procSlot].nextProcPtr = ReadyList;
		ReadyList = &ProcTable[procSlot];
	} 
        // Case 3: The proper position for this element comes after the first
        //   element of the ready list.
        else {
            temp = ReadyList;
	    while(temp->nextProcPtr != NULL && temp->nextProcPtr->priority <= ProcTable[procSlot].priority){
		temp = temp->nextProcPtr;
	    }
	    ProcTable[procSlot].nextProcPtr = temp->nextProcPtr;
	    temp->nextProcPtr = &ProcTable[procSlot];
	}
}

/* -------------------------- Functions ----------------------------------- */
/* ------------------------------------------------------------------------
   Name - startup
   Purpose - Initializes process lists and clock interrupt vector.
             Start up sentinel process and the test process.
   Parameters - argc and argv passed in by USLOSS
   Returns - nothing
   Side Effects - lots, starts the whole thing
   - ---------------------------------------------------------------------- */
void startup(int argc, char *argv[])
{
    int result; /* value returned by call to fork1() */
    Current = NULL;
       
    /* initialize the process table */
    if (DEBUG && debugflag)
        USLOSS_Console("startup(): initializing process table, ProcTable[]\n");
    initializeProcessTable();

    // Initialize the Ready list, etc.
    if (DEBUG && debugflag)
        USLOSS_Console("startup(): initializing the Ready list\n");
    ReadyList = NULL;

    // Initialize the clock interrupt handler
    USLOSS_IntVec[USLOSS_CLOCK_INT] = clock_handler;

    // startup a sentinel process
    if (DEBUG && debugflag)
        USLOSS_Console("startup(): calling fork1() for sentinel\n");
    result = fork1("sentinel", sentinel, NULL, USLOSS_MIN_STACK,
                    SENTINELPRIORITY);
    if (result < 0) {
        if (DEBUG && debugflag) {
            USLOSS_Console("startup(): fork1 of sentinel returned error, ");
            USLOSS_Console("halting...\n");
        }
        USLOSS_Halt(1);
    }
  
    // start the test process
    if (DEBUG && debugflag)
        USLOSS_Console("startup(): calling fork1() for start1\n");
    result = fork1("start1", start1, NULL, 2 * USLOSS_MIN_STACK, 1);
    if (result < 0) {
        USLOSS_Console("startup(): fork1 for start1 returned an error, ");
        USLOSS_Console("halting...\n");
        USLOSS_Halt(1);
    }

    USLOSS_Console("startup(): Should not see this message! ");
    USLOSS_Console("Returned from fork1 call that created start1\n");

    return;
} /* startup */

/* ------------------------------------------------------------------------
   Name - finish
   Purpose - Required by USLOSS
   Parameters - none
   Returns - nothing
   Side Effects - none
   ----------------------------------------------------------------------- */
void finish(int argc, char *argv[])
{
    if (DEBUG && debugflag)
        USLOSS_Console("in finish...\n");
} /* finish */

/* ------------------------------------------------------------------------
   Name - fork1
   Purpose - Gets a new process from the process table and initializes
             information of the process.  Updates information in the
             parent process to reflect this child process creation.
   Parameters - the process procedure address, the size of the stack and
                the priority to be assigned to the child process.
   Returns - the process id of the created child or -1 if no child could
             be created or if priority is not between max and min priority.
   Side Effects - ReadyList is changed, ProcTable is changed, Current
                  process information changed
   ------------------------------------------------------------------------ */
int fork1(char *name, int (*startFunc)(char *), char *arg,
          int stacksize, int priority)
{
    int procSlot = 0;
    char *newStack;     // Pointer to the newly allocated stack

    if (DEBUG && debugflag)
        USLOSS_Console("fork1(): creating process %s\n", name);

	

    // test if in kernel mode; halt if in user mode
    if(!(USLOSS_PsrGet() & 0b00001)){
      if(Current!=NULL)
        USLOSS_Console("fork1(): called while in user mode, by process %d. Halting...\n", Current->pid);
      USLOSS_Halt(0);
    }

    // Return if stack size is too small
    if(stacksize < USLOSS_MIN_STACK){
	return -2;
    }

    // Return -1 if priority is out of range
    if ( priority > 6 || priority < 1 ) {
        return -1;
    } 

    // return -1 if startFunc is null
    if ( startFunc == NULL ) {
        return -1;
    }

    // Return -1 if name is null
    if ( name == NULL) {
        return -1;
    }
    
 // Check for full process table
    if ( numProcesses >= 50 ) {
        if (DEBUG && debugflag) {
            USLOSS_Console("  fork1(): process table is full! Could not ");
            USLOSS_Console("  create process %s, returning -1...\n", name);
        }

        return -1;
    }
    // Find the next pid and corresponding available process table block
    while(ProcTable[ (nextPid % 50) ].pid != -1) {
        nextPid++;
    }
    procSlot = (nextPid % 50);

    // Initilaize the stack for this process
    newStack = (char*)malloc(stacksize * sizeof(char));

    // Initilaize the stack for this process
    // fill-in entry in process table */
    if ( strlen(name) >= (MAXNAME - 1) ) {
        USLOSS_Console("fork1(): Process name is too long.  Halting...\n");
        USLOSS_Halt(1);
    }
    strcpy(ProcTable[procSlot].name, name);
    ProcTable[procSlot].startFunc = startFunc;
    if ( arg == NULL )
        ProcTable[procSlot].startArg[0] = '\0';
    else if ( strlen(arg) >= (MAXARG - 1) ) {
        USLOSS_Console("fork1(): argument too long.  Halting...\n");
        USLOSS_Halt(1);
    }
    else
        strcpy(ProcTable[procSlot].startArg, arg);
    
    // Give the process the next available PID
    ProcTable[procSlot].pid = nextPid;
    nextPid++;
    numProcesses++;

    // Record parent pid, -1 if Current = NULL
    if (Current == NULL) {
        ProcTable[procSlot].parentPid = -2;
    } else {
        ProcTable[procSlot].parentPid = Current->pid;
    }

    // Give the stack size to the process table
    ProcTable[procSlot].stackSize = stacksize;

    // Point the process's stack to the newly allocated stack
    ProcTable[procSlot].stack = newStack;

    ProcTable[procSlot].priority = priority;

    // Initialize the quitChildList
    ProcTable[procSlot].quitChildList = NULL;

    makeReadyList(procSlot);    

    // 
    // Initialize context for this process, but use launch function pointer for
    // the initial value of the process's program counter (PC)

    USLOSS_ContextInit(&(ProcTable[procSlot].state),
                       ProcTable[procSlot].stack,
                       ProcTable[procSlot].stackSize,
                       NULL,
                       launch);

    // Add newly forked process to parent's childProcPtr list, if it has
    //   a parent (parentPid != 0)
    if (ProcTable[procSlot].parentPid != -2) {
 		connectChild(procSlot);
    }

    // for future phase(s)
    p1_fork(ProcTable[procSlot].pid);
  
    // Return if the sentinel process was created
    if ( ProcTable[procSlot].pid == SENTINELPID ) {
        return ProcTable[procSlot].pid;
    }

    dispatcher();
    return ProcTable[procSlot].pid;

} /* fork1 */

/* ------------------------------------------------------------------------
   Name - launch
   Purpose - Dummy function to enable interrupts and launch a given process
             upon startup.
   Parameters - none
   Returns - nothing
   Side Effects - enable interrupts
   ------------------------------------------------------------------------ */
void launch()
{
    int result;
    
    if (DEBUG && debugflag)
        USLOSS_Console("launch(): started\n");

    USLOSS_PsrSet(USLOSS_PsrGet() | 0b000010);
    
    // Call the function passed to fork1, and capture its return value
    result = Current->startFunc(Current->startArg);

    if (DEBUG && debugflag)
        USLOSS_Console("Process %d returned to launch\n", Current->pid);

    quit(result);

} /* launch */


/* ------------------------------------------------------------------------
   Name - join
   Purpose - Synchronize with forked child and get return status.
             3 cases:
               Case 1: Process has no children
               Case 2: Child(ren) quit() before the join() occurred
               Case 3: No unjoined child has quit()
                 Block until a child quits(), update the child's
                 status and return its pid
   Parameters - a pointer to an int where the termination code of the 
                quitting process is to be stored.
   Returns - the process id of the quitting child joined on.
             -1 if the process was zapped in the join
             -2 if the process has no children
   Side Effects - If no child process has quit before join is called, the 
                  parent is removed from the ready list and blocked.
   ------------------------------------------------------------------------ */
int join(int *status)
{
    procPtr quitChild = NULL; // Pointer to the child that quit
    int childPid = -99;  // Holds the pid of the child that quit

    // Test for kernel mode; halt if in user mode
    if(!(USLOSS_PsrGet() & 0b00001)){
      if(Current!=NULL)
        USLOSS_Console("join(): called while in user mode, by process %d. Halting...\n", Current->pid);
      USLOSS_Halt(0);
    }

    // TODO: Disable interrupts

    // Check which of the 3 cases for join

	// Case 1: if Process has no child return -2
	if(Current->childProcPtr == NULL){
		return -2;
	}

	quitChild = checkChildQuit();

	// Case 2: No unjoined child has quit()
	// block until a child calls quit
	if(quitChild == NULL){
            if (DEBUG && debugflag) {
                USLOSS_Console("   join: no process quit, blocking...\n");
            }

            joinBlockMe();


            if (DEBUG && debugflag) {
                USLOSS_Console("   join: process quit, continuing join...\n");
            }

            // Process has been un-joinblocked, at this point, so a child
            //   must have quit. Find the child that quit
            quitChild = checkChildQuit();
	}
	
	// cases 2 and 3: Found a child that quit, extract quitStatus and pid
        //   from child and finish cleaning up process table for quit child

        // Get the pid and quitStatus from the child.
        if (DEBUG && debugflag) {
            USLOSS_Console("   join: getting quit child status\n");
            USLOSS_Console("   quit status of child: ");
            USLOSS_Console("%d\n", quitChild->quitStatus);
        }

	*status = quitChild->quitStatus;
        childPid = quitChild->pid;


        if (DEBUG && debugflag) {
            USLOSS_Console("   join: cleaning up child\n");
        }

        // Finish cleaning up process table
        removeChild(quitChild->pid); // Remove the child from child process list
        strcpy(quitChild->name, "");
        quitChild->pid = -1;
        quitChild->quitStatus = -99;
        quitChild->parentPid = -1;
        quitChild->priority = -1;
        quitChild->startTime = -1;
        quitChild->totalTime = -1;
        quitChild->status = EMPTY;

	numProcesses--;

        // Return the child's pid, or -1 if this process was zapped on the join
        if (isZapped() == 1) {
            return -1;
        }
	return childPid;

} /* join */


/* ------------------------------------------------------------------------
   Name - quit
   Purpose - Stops the child process and notifies the parent of the death by
             putting child quit info on the parents child completion code
             list.
   Parameters - the code to return to the grieving parent
   Returns - nothing
   Side Effects - changes the parent of pid child completion status list.
   ------------------------------------------------------------------------ */
void quit(int status)
{
    if (DEBUG && debugflag) {
        USLOSS_Console(" quit(): started, Current process: ");
        USLOSS_Console("%s\n", Current->name);
    }

    // Used to check the list of child processes for QUIT status
    procPtr childListPtr = Current->childProcPtr;
    procPtr parentPtr = NULL;  // Holds a pointer to the parent process
    procPtr quitListPtr = NULL;  // Used to traverse parent's quit procs
    procPtr temp = NULL;  // Holds value when switching pointers

    // Test for kernel mode; halt if in user mode
    if(!(USLOSS_PsrGet() & 0b00001)){
      if(Current!=NULL)
        USLOSS_Console("quit(): called while in user mode, by process %d. Halting...\n", Current->pid);
      USLOSS_Halt(0);
    }

    // TODO: Disable interrupts

    // Check to see if Current has active children (those who have not quit)
    while (childListPtr != NULL) {
        if (childListPtr->status != QUIT) {
            USLOSS_Console("quit(): process 2, '%s', has ", Current->name);
            USLOSS_Console("active children. Halting...\n");
            if (DEBUG && debugflag) {
                USLOSS_Console("  Child process name: ");
                USLOSS_Console("%s\n", childListPtr->name);
                USLOSS_Console("  Child process status: ");
                USLOSS_Console("%d\n", childListPtr->status);
            }
            USLOSS_Halt(1);
        }
        childListPtr = childListPtr->nextSiblingPtr;
    }

    // Change the Current status to QUIT, and the quitStatus to the
    //   specified value
    Current->status = QUIT;
    Current->quitStatus = status;

    // Remove the this process from the ready list
    removeInReadyList(Current->pid);

    // Finish cleaning up PCBs for children who have quit but not joined
    childListPtr = Current->childProcPtr;
    while (Current->childProcPtr != NULL) {
        strcpy(childListPtr->name, "");
        Current->childProcPtr->pid = -1;
        Current->childProcPtr->quitStatus = -99;
        Current->childProcPtr->parentPid = -1;
        Current->childProcPtr->priority = -1;
        Current->childProcPtr->startTime = -1;
        Current->childProcPtr->totalTime = -1;
        Current->childProcPtr->status = EMPTY;

        temp = Current->childProcPtr;
        Current->childProcPtr = Current->childProcPtr->nextQuitSibling;
        temp->nextSiblingPtr = NULL;
    }

    // Clean up the process table entry for this process, reset some fields
    //   to default values but not: nextSiblingPtr, pid, status,
    //   and quitStatus. Pointers are set to NULL, int values are set to -99
    if (DEBUG && debugflag) {
        USLOSS_Console("  cleaning up some of the PCB...\n");
    }
    Current->nextProcPtr = NULL;
    Current->childProcPtr = NULL;
    Current->startFunc = NULL;
    Current->stackSize = -1;

    // Find the parent
    for (int i = 0; i < MAXPROC; i++) {
        if (ProcTable[i].pid == Current->parentPid) {
            parentPtr = &(ProcTable[i]);
        }
    }

      // If the process has no parent, finish cleaning up and call dispatcher
    if (parentPtr == NULL) {
        if (DEBUG && debugflag) {
            USLOSS_Console("  Process had no parent, fully cleaning up PCB\n");
        }
      
        strcpy(Current->name, "");
        Current->nextSiblingPtr = NULL;
        Current->pid = -1;
        Current->quitStatus = -99;
        Current->parentPid = -1;
        Current->priority = -1;
        Current->startTime = -1;
        Current->status = EMPTY;

	numProcesses--;

        dispatcher();
        return;
    }

    // Add this process to its parent's quitChildList
    quitListPtr = parentPtr->quitChildList;
      // Check for an empty parent quit process list
    if (quitListPtr == NULL) {
        parentPtr->quitChildList = Current;
    }
      // If not empty, find the last process in the quit list
    else {
          // Find the end of the quit child list
        while (quitListPtr->nextQuitSibling != NULL) {
            quitListPtr = quitListPtr->nextQuitSibling;
        }
          // Add this process to the end of the list
        quitListPtr->nextQuitSibling = Current;
    }
      // Update the Current's process's nextQuitSibling to NULL
    Current->nextQuitSibling = NULL;

    // If the parent process has already called join(), the child needs
    //   to unblock the parent
      // Check to see if the parent is join blocked, unblock it if it is
    if (parentPtr->status == JOINBLOCKED) {
        unjoinblockProc(parentPtr->pid);
    }

    // Unblock processes that zap()'d this process
    while (Current->zapList != NULL) {
        if (DEBUG && debugflag) {
            USLOSS_Console("   quit(): removing zap blocks...\n");
        }

        // Ready the process
        for (int i = 0; i < MAXPROC; i++) {
            // makeReadyList takes a table position, not PID or pointer
            if (ProcTable[i].pid == Current->zapList->pid) {
                makeReadyList(i);  
            }
        }

        // Remove the process from the zapProc list
        temp = Current->zapList;
        Current->zapList = Current->zapList->nextZapProc;
        temp->nextZapProc = NULL;

        if (DEBUG && debugflag) {
            USLOSS_Console("   quit(): zap block removed from process ");
            USLOSS_Console("%s\n", temp->name);
        }

    }

    // Call p1_quit function (for later phases)
    p1_quit(Current->pid);

    // Call the dispatcher to decide which process runs next
    dispatcher();
} /* quit */


/* ------------------------------------------------------------------------
   Name - dispatcher
   Purpose - dispatches ready processes.  The process with the highest
             priority (the first on the ready list) is scheduled to
             run.  The old process is swapped out and the new process
             swapped in.
   Parameters - none
   Returns - nothing
   Side Effects - the context of the machine is changed
   ----------------------------------------------------------------------- */
void dispatcher(void)
{
    procPtr listPtr;              // Used to walk through ready list
    procPtr oldProc;              // Temporarily holds old "Current" ptr
    int     currentPriority = 1;  // The priority we're looking for
    int     executionTime;        // Amount of time the old process ran

    if (DEBUG && debugflag) {
        USLOSS_Console(" Calling dispatcher, current process: ");
        if (Current == NULL) {
            USLOSS_Console("NULL\n");
        } else {
            USLOSS_Console("%s\n", Current->name);
        }
    }
    
    // TODO: Disable interrupts

    // Find the first process on the ready list with the highest priority
    while (currentPriority <= 6) {
        // Start at the beginning of the ready list
        listPtr = ReadyList;

        // Search for a process with priority = currentPriority
        while (listPtr != NULL) {
            if (listPtr->priority == currentPriority) {
               // Found a process at this priority, switch contexts to
                //   this process
                oldProc = Current;
                Current = listPtr;
               
                // Special Case: 
                // If there was no Current process running, Current == NULL
                if (oldProc == NULL) {
		    Current->status = RUNNING;
                    p1_switch(-1, -Current->pid);

                    // Set the start time for the Current process
                    Current->startTime = readTime();
                    if (DEBUG && debugflag) {
                        USLOSS_Console("   dispatcher: switching to process: ");
                        USLOSS_Console("%s\n", Current->name);
                    }

                    USLOSS_ContextSwitch(NULL, &(Current->state));
                    return;
                }
                // If the new process to run is the same as the old process,
                //   just return, the process is already running!
		else if (oldProc->pid == Current->pid) {
		    return;
		}
                // Otherwise, switch the context to the new process
                else {
                    // Change statuses to reflect context switch
		    Current->status = RUNNING;

                    p1_switch(oldProc->pid, Current->pid);

                    // Enter new start time for new Current and calculate total
                    //   for old process
                      //  Set start time for new process
                    Current->startTime = readTime();
                      // Calculate total running time for old process
                    executionTime = readTime() - oldProc->startTime;
                    if (oldProc->totalTime == -1) {
                        // If this is the first time process ran:
                        oldProc->totalTime = executionTime;
                    } else {
                        // If the process has a running total:
                        oldProc->totalTime =
                            oldProc->totalTime + executionTime;
                    }
                    

                    if (DEBUG && debugflag) {
                        USLOSS_Console("   dispatcher: switching to process: ");
                        USLOSS_Console("%s\n", Current->name);
                        USLOSS_Console("   old process was: ");
                        USLOSS_Console("%s\n", oldProc->name);
                    }


                    USLOSS_ContextSwitch(&(oldProc->state), &(Current->state));
                    return;
                }
            }
            listPtr = listPtr->nextProcPtr;
        }
        
        // No process found at this priority, look for the next highest 
        // priority
        currentPriority++;
    }
    
    USLOSS_Console("Something went horribly wrong in dispatcher!");
    USLOSS_Console(" Could not find a process in the ready list.");
    USLOSS_Halt(1);
} /* dispatcher */


/* ------------------------------------------------------------------------
   Name - sentinel
   Purpose - The purpose of the sentinel routine is two-fold.  One
             responsibility is to keep the system going when all other
             processes are blocked.  The other is to detect and report
             simple deadlock states.
   Parameters - none
   Returns - nothing
   Side Effects -  if system is in deadlock, print appropriate error
                   and halt.
   ----------------------------------------------------------------------- */

int sentinel (char *dummy)
{
    if (DEBUG && debugflag)
        USLOSS_Console("sentinel(): called\n");
    while (1)
    {
        checkDeadlock();
        USLOSS_WaitInt();
    }
} /* sentinel */


/* check to determine if deadlock has occurred... */
static void checkDeadlock()
{
    // Check to see if there are any processes besides the sentinel running.
    //    If not, Halt.
    for (int i = 0; i < MAXPROC; i++) {
        if (ProcTable[i].pid != -1 && ProcTable[i].priority != 6) {
            USLOSS_Console("checkDeadlock(): numProc = %d. ", numProcesses);
            USLOSS_Console("Only Sentinel should be left. Halting...\n");

            if (DEBUG && debugflag) {
                dumpProcesses();
            }

            USLOSS_Halt(1);
            return;
        }
    }

    // No running non-sentinel process found, deadlock occurred, Halt
    USLOSS_Console("All processes completed.\n");
    USLOSS_Halt(0);
} /* checkDeadlock */


/*
 * Disables the interrupts.
 */
void disableInterrupts()
{
    // turn the interrupts OFF iff we are in kernel mode
    // if not in kernel mode, print an error message and
    // halt USLOSS

} /* disableInterrupts */

/* ------------------------------------------------------------------------
   Name - blockMe
   Purpose - Blocks the calling process. This is done by removing the
             process from the ready list and changing its status to
             blocked. The dispatcher is then called to determine which
             process should run since the Current process was just
             blocked. 
             The calling process is always assumed to be the Current
             process.
   Parameters - The value used to indicate the status of the process in
                the dumpProcesses command, MUST be greater than 10. Or
                USLOSS will Halt with an error.
   Returns -  -1:  if process was zapped while blocked.
               0:  otherwise
   Side Effects - Process status changed and readyList modified.
   ----------------------------------------------------------------------- */
int blockMe(int newStatus){
    // Test for kernel mode; halt if in user mode
    if (USLOSS_PsrGet() != 1) {
        USLOSS_Console("Error: blockMe cannot be called in user mode\n");
        USLOSS_Halt(1);
    }

    // TODO: Disable interrupts

    // Error and Halt if the newStatus is not greater than 10
    if (newStatus <= 10) {
        USLOSS_Console("ERROR in blockMe(): newStatus was %d, ", newStatus);
        USLOSS_Console("must be greater than 10!");
        USLOSS_Halt(1);
    }

    // Remove the Current process from the ready list and set its status to
    //   blocked
    removeInReadyList(Current->pid);
    changeStatus(Current->pid, newStatus);

    // Call the dispatcher
    dispatcher();
    //Return 1 if the process was zapped while blocked
    if (isZapped() == 1) {
        return -1;
    }
    // Return 0 if the process was blocked successfully
    return 0;
}

/* ------------------------------------------------------------------------
   Name - unblockProc
   Purpose - This operation unblocks process pid that had previously
             blocked itself by calling blockMe. The status of that process
             is changed to READY, and it is put on the Ready List.
   Parameters - The pid of the process to unblock.
   Returns -  -2: if the indicated process was not blocked, does not exist,
                  is the current process, or is blocked on a status less
                  than or equal to 10. Thus, a process that is zap-blocked
                  or join-blocked cannot be unblocked with this function
                  call. 
              -1: if the calling process was zapped
               0: otherwise
   Side Effects - Changes the readyList, calls the dispatcher.
   ----------------------------------------------------------------------- */
int unblockProc(int pid) {
    if (DEBUG && debugflag) {
        USLOSS_Console(" unblockProc(): unblocking process %d\n", pid);
    }

    procPtr procToUnblock = NULL;
    int procSlot = -1;

    // Find the process to unblock
    for (int i = 0; i < MAXPROC; i++) {
        if (ProcTable[i].pid == pid) {
            procToUnblock = &(ProcTable[i]);
            procSlot = i;
            break;
        }
    }

    // If process w/ pid doesn't exist, return -2
    if (procToUnblock == NULL) {
        return -2;
    }
    // Check if the process was actaully blocked, if not, return -2
    if (procToUnblock->status <= 10) {
        return -2;
    }
    // Check if the specified process is actually the Current process
    if (pid == Current->pid) {
        return -2;
    }

    // Change process status to ready and put it on the ready list
    makeReadyList(procSlot);
    
    dispatcher();
    //Return 1 if the process was zapped while blocked
    if (isZapped() == 1) {
        return -1;
    }
    // Return 0 if the process was blocked successfully
    return 0;
}


int removeInReadyList(int pid){
        if (DEBUG && debugflag) {
            USLOSS_Console("  removeInReadyList: pid: %d\n", pid);
        }

	if(ReadyList == NULL){
		USLOSS_Console("ReadyList is empty...Halt..\n");
		USLOSS_Halt(1);
	}

	procPtr temp = ReadyList;
	if(ReadyList->pid == pid){
		ReadyList = ReadyList->nextProcPtr;
	}else{
	// Find process pid in ReadyList
		while( temp != NULL){
			// break if we find (temp is prc that we need to remove	
			if(temp->pid == pid){
				break;
			}
			temp = temp->nextProcPtr;
		}
		// Error case: If there is no pid in the readylist
		if(temp == NULL){
			USLOSS_Console("There is no %d in the ready list", pid);
			USLOSS_Halt(1);
		}
	
		procPtr previousTemp = ReadyList;
		// Find prcess that privious of temp 
		while(previousTemp->nextProcPtr->pid != temp->pid){
			previousTemp = previousTemp->nextProcPtr;
		}
	
		// Remove process pid in ReadyList
		previousTemp->nextProcPtr = temp->nextProcPtr;	
		temp->nextProcPtr = NULL;
	}	
	return 1;	
	
}


/* ------------------------------------------------------------------------
   Name - joinBlockMe
   Purpose - Join blocks the calling process. This is done by removing the
             process from the ready list and changing its status to
             JOINBLOCKED. The dispatcher is then called to determine which
             process should run since the Current process was just
             blocked. 
             The calling process is always assumed to be the Current
             process.
             This is a static helper method called during a join. A such,
             kernel mode check is not performed and interrupts are not
             disabled, this is assumed to have been done in join().
   Parameters - none.
   Returns -  -1:  if process was already JOINBLOCKED
               0:  otherwise
   Side Effects - Process status changed and readyList modified.
   ----------------------------------------------------------------------- */
static int joinBlockMe() {
    // If the process is not already JOINBLOCKED, remove it from the ready
    //   list and set its status to JOINBLOCKED
    if (Current->status != JOINBLOCKED) {
        removeInReadyList(Current->pid);
        changeStatus(Current->pid, JOINBLOCKED);
       
        dispatcher();
 
        return 0;
    }

    // If the process was already JOINBLOCKED return -1;
    return -1;
}


/* ------------------------------------------------------------------------
   Name - unjoinblockProc
   Purpose - un-JOINBLOCKED's the process belonging to the passed pid.
             Changes the status of the process to READY and adds the 
             process back into the ready list.
             Halts with an error if:
               The pid is not valid.
               The process was not JOINBLOCKED.

             This is a static helper method called during quit. A such,
             kernel mode check is not performed and interrupts are not
             disabled, this is assumed to have been done in quit().
   Parameters - The pid of the process to un-JOINBLOCKED
   Returns - nothing.
   Side Effects - Process status changed and readyList modified.
   ----------------------------------------------------------------------- */
static void unjoinblockProc(int pid) {
    int tablePos = -1; // Index of the process w/ target pid

    // Find the specified process in the process table
    for (int i = 0; i < MAXPROC; i++) {
        if (ProcTable[i].pid == pid) {
            tablePos = i;
            break;
        }
    }
    // If not found, halt with an error
    if (tablePos == -1) {
        USLOSS_Console("ERROR in unjoinblockProc(): target pid (%d)", pid);
        USLOSS_Console(" not found in process table!\n");
        USLOSS_Halt(1);
    }

    // Check that the process is actually JOINBLOCKED
    if ( ProcTable[tablePos].status != JOINBLOCKED ) {
        USLOSS_Console("ERROR in unjoinblockProc(): target process ");
        USLOSS_Console("%s is not JOINBLOCKED!\n", ProcTable[tablePos].name);
        USLOSS_Halt(1);
    }

    // Change the status to READY and put the process on the ready list
    makeReadyList(tablePos);
}

/* ------------------------------------------------------------------------
   Name - removeChild
   Purpose - Remove a child process from the list of the Currently running
             process's children (the list pointed to by childProcPtr).
             This simply moves the pointer to that was pointing to the
             specified child process to the next child process, which could
             be NULL. Also sets the removed child's nextSiblingPtr to NULL.

             Halts with an error if:
               The Current process doesn't have a child with the specified
               pid.

             This is a static helper method called during quit. A such,
             kernel mode check is not performed and interrupts are not
             disabled, this is assumed to have been done in quit().
   Parameters - The pid of the child process to remove from the list of
                the Current process's child list.
   Returns - nothing.
   Side Effects - Current process's list of children is modified, removed
                  process's nextSiblingPtr modified.
   ----------------------------------------------------------------------- */
static void removeChild(int childPid) {
    if (DEBUG && debugflag) {
        USLOSS_Console("    removeChild: attempting to remove child at ");
        USLOSS_Console("pid: %d\n", childPid);
    }

    procPtr childList = Current->childProcPtr;  // Used to search child list
    procPtr temp = NULL;  // Holds old child's nextSiblingPtr

    // If the Current process has no children, halt w/ error
    if (childList == NULL) {
	USLOSS_Console("ERROR in removeChild: process %s has", Current->name);
	USLOSS_Console(" no children!\n");
	USLOSS_Halt(1);
    }

    // Is the child to remove the first child in the list?
    if (childList->pid == childPid) {
        temp = Current->childProcPtr;
        Current->childProcPtr = Current->childProcPtr->nextSiblingPtr;
        temp->nextSiblingPtr = NULL;
        return;
    }

    // Find the child before the child we're looking for
    while (childList->nextSiblingPtr != NULL &&
           childList->nextSiblingPtr->pid != childPid) {
        childList = childList->nextSiblingPtr;
    }

    // If the Current process does not have a child w/ the targte pid,
    //   halt w/ error
    if (childList == NULL) {
	USLOSS_Console("ERROR in removeChild: process %s has", Current->name);
	USLOSS_Console(" no child with pid: %d\n", childPid);
	USLOSS_Halt(1);
    }

    // Remove the child by changing the nextSiblingPtr before it
    temp = childList->nextSiblingPtr->nextSiblingPtr;
    childList->nextSiblingPtr->nextSiblingPtr = NULL;
    childList->nextSiblingPtr = temp;

}


/* ------------------------------------------------------------------------
   Name - processDump
   Purpose - Prints the contents of the process table. For each PCB in the
             table, prints:
               PID
               parentPID
               priority
               process status
               number of children
               CPU time consumed
               name

   Parameters - None.
   Returns - nothing.
   Side Effects - none.
   ----------------------------------------------------------------------- */
void dumpProcesses(void) {
    int numChildren;  // Counter for the # of children a process has
    procPtr childListPtr = NULL;  // Used to traverse child list

    USLOSS_Console("PID\t Parent\t Priority\t Status\t\t # Kids\t ");
    USLOSS_Console("CPUtime\t Name");

    if (DEBUG && debugflag) {
        USLOSS_Console("      zapped?");
    }

    USLOSS_Console("\n");
 
    
    for (int i = 0; i < MAXPROC; i++) {
      USLOSS_Console(" %-5d  ", ProcTable[i].pid);
      USLOSS_Console("  %-3d   ", ProcTable[i].parentPid);
      USLOSS_Console("   %-8d     ", ProcTable[i].priority);
      // Print status as a string
      switch (ProcTable[i].status) {
          case (READY) :
              USLOSS_Console("READY           ");
              break;
          case (BLOCKED) :
              USLOSS_Console("BLOCKED         ");
              break;
	  case (QUIT) :
	      USLOSS_Console("QUIT            ");
              break;
	  case (RUNNING) :
	      USLOSS_Console("RUNNING         ");
	      break;
          case (JOINBLOCKED) :
              USLOSS_Console("JOIN_BLOCK      ");
              break;
          case (ZAP_BLOCKED) :
              USLOSS_Console("ZAP_BLOCK       ");
              break;
          case (EMPTY) :
              USLOSS_Console("EMPTY           ");
              break;
          default :
              USLOSS_Console("%-16d", ProcTable[i].status); 
      }
      // # of children of any status
      numChildren = 0;
      childListPtr = ProcTable[i].childProcPtr;
      while (childListPtr != NULL) {
            numChildren++;
            childListPtr = childListPtr->nextSiblingPtr;
      }
      USLOSS_Console("  %-2d    ", numChildren);
      USLOSS_Console("   %-5d", ProcTable[i].totalTime);
      USLOSS_Console("%-10s", ProcTable[i].name);

      if (DEBUG && debugflag) {
         if (ProcTable[i].isZapped == 0) {
             USLOSS_Console("NO"); 
         } else {
             USLOSS_Console("YES");
         }
      }

      USLOSS_Console("\n");
    }

}

/* ------------------------------------------------------------------------
   Name - initializeProcessTable
   Purpose - Initializes PCBs in the process table to default values.
             For int fields this is -1, for string fields this is an empty
             string, for pointers this is NULL.

   Parameters - None.
   Returns - nothing.
   Side Effects - none.
   ----------------------------------------------------------------------- */
static void initializeProcessTable(void) {
    for (int i = 0; i < MAXPROC; i++) {
        ProcTable[i].nextProcPtr = NULL;
        ProcTable[i].childProcPtr = NULL;
        ProcTable[i].nextSiblingPtr = NULL;
        ProcTable[i].quitChildList = NULL;
        ProcTable[i].nextQuitSibling = NULL;
        ProcTable[i].zapList = NULL;
        strcpy(ProcTable[i].name, "");
        ProcTable[i].pid = -1;
        ProcTable[i].parentPid = -1;
        ProcTable[i].priority = -1;
        ProcTable[i].startFunc = NULL;
        ProcTable[i].stackSize = -1;
        ProcTable[i].status = EMPTY;
        ProcTable[i].quitStatus = -99;
        ProcTable[i].startTime = -1;
        ProcTable[i].totalTime = -1;
        ProcTable[i].isZapped = 0;
    }
}

/* ------------------------------------------------------------------------
   Name - zap
   Purpose - Marks process w/ pid as zapped, then ZAP_BLOCKED's the
             process if successful. The ZAP_BLOCKED process stays blocked
             until the zapped process quits and unblocks the calling
             process, at which point zap returns.
             Return values:
               -1:  the calling process itself was zapped while in zap
                0:  the zapped process has called quit

             If a process attempts to zap itself or a non-existant
             process USLOSS halts with an error.

   Parameters - Process ID of the process to zap.
   Returns -  -1:  the calling process itself was zapped while in zap
               0:  the zapped process has called quit
   Side Effects - Changes process @ pid's isZapped status, the calling
                  process's status and the ready list.
   ----------------------------------------------------------------------- */
int zap(int pid) {
    if (DEBUG && debugflag) {
        USLOSS_Console(" zap(): attempting to zap process %d\n", pid);
    }

    procPtr processToZap = NULL;  // Points to the target process
    procPtr zapListPtr   = NULL;  // Used to traverse target's zapList

    // Find the process to zap
    for (int i = 0; i < MAXPROC; i++) {
        if (ProcTable[i].pid == pid) {
            processToZap = &(ProcTable[i]);
            break;
        }
    }

    // If no process found, Halt w/ error
    if (processToZap == NULL) {
        USLOSS_Console("zap(): process being zapped does not exist.  ");
        USLOSS_Console("Halting...\n");
        USLOSS_Halt(1);
    }

    // Halt w/ error if the process is trying to zap itself
    if (Current->pid == pid) {
        USLOSS_Console("zap(): process %d tried to zap itself.  ", pid);
        USLOSS_Console("Halting...\n");
        USLOSS_Halt(1);
    }

    // If the process has already quit, just return, no need to wait
    if (processToZap->status == QUIT) {
        if (Current->isZapped == 1) {
            return -1;
        }
        return 0;
    }
   
    // Set the flag io the target process 
    processToZap->isZapped = 1;

    // Add this process to the end of the target process's zapList
    zapListPtr = processToZap->zapList;
    if (zapListPtr == NULL) {
        processToZap->zapList = Current;
    } else {
        while (zapListPtr->nextZapProc != NULL) {
            zapListPtr = zapListPtr->nextZapProc;
        }

        zapListPtr->nextZapProc = Current;
    }

    Current->nextZapProc = NULL;

    // ZAP_BLOCK this process, then call the dispatcher
    zapBlockMe();
    dispatcher();

    // Process unblocked, the zapped process must have quit.
    // Return -1 if this process was zapped in the meantime, 0 otherwise
    if (Current->isZapped == 1) {
        return -1;
    }
    return 0;
} /* zap */


/* ------------------------------------------------------------------------
   Name - isZapped
   Purpose - Returns 0 if the calling process has not been zapped, 1 if it
             has.

   Parameters - None.
   Returns -  0:  the calling process has not been zapped
              1:  the calling process has been zapped
   Side Effects - none.
   ----------------------------------------------------------------------- */
int isZapped(void) {
    return Current->isZapped;
} /* isZapped */

/* ------------------------------------------------------------------------
   Name - zapBlockMe
   Purpose - Changes Current process's status to ZAP_BLOCKED and removes it
             from the ready list.

   Parameters - None.
   Returns - Nothing. 
   Side Effects - Changes Current process status and the ready list.
   ----------------------------------------------------------------------- */
static void zapBlockMe(void) {
    if (DEBUG && debugflag) {
        USLOSS_Console("   zapBlockMe(): zap blocking process: ");
        USLOSS_Console("%d\n", Current->pid);
    }

    Current->status = ZAP_BLOCKED;
    removeInReadyList(Current->pid);
} /* zapBlockMe */
