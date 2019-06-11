/* ------------------------------------------------------------------------
   phase2.c

   Francis Kim and Andrew Carlisle
   University of Arizona
   Computer Science 452

   description: For   this   second   phase   of   the   operating   system,   you   will   implement   low-level   process synchronization
   and communication via messages, and interrupt handlers. This phase, combined with phase 1, provides the building blocks needed by later phases,
   which will implement more complicate d process-control functions, device drivers, and virtual memory
   ------------------------------------------------------------------------ */

#include <phase1.h>
#include <phase2.h>
#include <usloss.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "message.h"

/* ------------------------- Prototypes ----------------------------------- */
int start1 (char *);
extern int start2 (char *);


/* -------------------------- Globals ------------------------------------- */

int debugflag2 = 0;
int stat=0;
int exit11=0; // exit statement

// the mail boxes 
mailbox MailBoxTable[MAXMBOX];
// the mail slot
mailSlot MailSlot[MAXSLOTS*MAXMBOX];
int totalMailBoxSlot=0;
// also need array of mail slots, array of function ptrs to system call 
// handlers, ...


/* 
	void check_kernel_mode(char *name) --
	Check the kernel mode
*/	
void check_kernel_mode(char *name){
	if(!(USLOSS_PsrGet() & 0b00001)){
		USLOSS_Console("%s calles while in user mode with process %d. Halting..\n", name, getpid());
	}
}

// void disable Interrupts()
// disable the interrupts
void disableInterrupts(){
        	
        USLOSS_PsrSet( USLOSS_PsrGet() & 0b111101 );
}

// void enableInterrupts
// enable the interrupts
void enableInterrupts(){

	USLOSS_PsrSet( USLOSS_PsrGet() & 0b000010 );
}

// void clockHandler(int dev, void *arg)
// It is the interupt handler of the Clock device
void clockHandler2(int dev, void *arg){      
  // Find a error cases
  if(dev!=USLOSS_CLOCK_DEV){
          USLOSS_Console("clockHandler2(): not calling clockHandler2. Halting...\n");
          USLOSS_Halt(1);
  }
  timeSlice();
  if(MailBoxTable[0].blockRecieveList[0].status!=EMPTY){
	
      USLOSS_DeviceInput(USLOSS_CLOCK_DEV, 0 ,&(stat));
      MboxSend(0,&(stat),sizeof(int));
  }
   if (DEBUG2 && debugflag2)
      USLOSS_Console("clockHandler2(): called\n");
} /* clockHandler */


// diskHandler(int dev, void *arg)
// Check statement of disk handler, and check also error case
void diskHandler(int dev, void *arg){
    // check error case
    if(dev!=USLOSS_DISK_INT){
            USLOSS_Console("termHandler(): not calling termHandler. Halting...\n");
            USLOSS_Halt(1);
    }
      long index= 0 | (long)arg;
    if(MailBoxTable[1+index].blockRecieveList[0].status!=EMPTY){
        USLOSS_DeviceInput(USLOSS_TERM_INT, index ,&(stat));
        MboxSend(1+index,  &(stat),sizeof(int));
    }
        if (DEBUG2 && debugflag2)
          USLOSS_Console("diskHandler(): called\n");
} /* diskHandler */


// termHandler(int dev, void *arg)
// check statement of term handler, and check also error case
void termHandler(int dev, void *arg){
  // check error case
  if(dev!=USLOSS_TERM_INT){
          USLOSS_Console("termHandler(): not calling termHandler. Halting...\n");
          USLOSS_Halt(1);
  }

  long index= 0 | (long)arg;

  if(MailBoxTable[3+index].blockRecieveList[0].status!=EMPTY){
      USLOSS_DeviceInput(USLOSS_TERM_INT, index ,&(stat));
      MboxSend(3+index,  &(stat),sizeof(int));
  }

  if (DEBUG2 && debugflag2){
        USLOSS_Console("termHandler(): called\n");
  }
} /* termHandler */

// nullsys(systemArgs *args)
// To check the invalid syscall
void nullsys(systemArgs *args){
    USLOSS_Console("nullsys(): Invalid syscall %d. Halting...\n",((systemArgs*)args)->number);
    USLOSS_Halt(1);
} /* nullsys */


// void syscallHandler(int dev, void *arg)
// It is the interupt handler of the System Call
void syscallHandler(int dev, void *arg){

  disableInterrupts();

  int num=((systemArgs*)arg)->number;
  if(num>=MAXSYSCALLS || num<0){
    USLOSS_Console("syscallHandler(): sys number %d is wrong.  Halting...\n",num);
    USLOSS_Halt(1);
  }

  nullsys((systemArgs*)arg);

  if (DEBUG2 && debugflag2){
    USLOSS_Console("syscallHandler(): called\n");
  }
  enableInterrupts();
} /* syscallHandler */


// int waitDevice(int type, int unit, int *stats)
// this routine causes the process to block on a receive on a zero-slot mailbox associated with the device
int waitDevice(int type, int unit, int *status){

	// For usloss_clock_dev
	if(type == USLOSS_CLOCK_DEV){
		MboxReceive(0, status, sizeof(int));
	// for usloss_dick_int
	}else if(type == USLOSS_DISK_INT){
		MboxReceive(unit+1, status, sizeof(int));
	// for usloss_term_int
	}else if(type == USLOSS_TERM_INT){
		MboxReceive(unit+3, status, 8);
	}

	// return -1 if it already zap
 	if(isZapped()){
    		return -1;
	}

  return 0;
}


// check_io()
// check the first block from the recieve list and return 1. If not return 0
int check_io(){

  int i=0;

  for(i=0;i<7;i++){
    if(MailBoxTable[i].blockRecieveList[0].pid!=-1){
      return 1;
    }
  }

  return 0;
}

/*
	Find the current mailbox that matches with m_id
*/
mailbox* currentMailBox(int m_id){
	return &(MailBoxTable[m_id%MAXMBOX]);
}
/* -------------------------- Functions ----------------------------------- */

/* ------------------------------------------------------------------------
   Name - start1
   Purpose - Initializes mailboxes and interrupt vector.
             Start the phase2 test process.
   Parameters - one, default arg passed by fork1, not used here.
   Returns - one to indicate normal quit.
   Side Effects - lots since it initializes the phase2 data structures.
   ----------------------------------------------------------------------- */
int start1(char *arg)
{
    int kid_pid, status;
    if (DEBUG2 && debugflag2)
        USLOSS_Console("start1(): at beginning\n");

    check_kernel_mode("start1");
    // Disable interrupts
    disableInterrupts();

    // Create all the USLOSS_INTVEC = handler
    USLOSS_IntVec[USLOSS_SYSCALL_INT]=syscallHandler;
    USLOSS_IntVec[USLOSS_TERM_DEV]=termHandler;
    USLOSS_IntVec[USLOSS_CLOCK_DEV]=clockHandler2;
    USLOSS_IntVec[USLOSS_DISK_INT]=diskHandler;
    // Initialize the mail box table, slots, & other data structures.
    for(int i=0; i < MAXMBOX; i++){
	// Create IO boxes from 0 - 6
	if(i < 7){
		MailBoxTable[i].status = RUNNING; // status
		MailBoxTable[i].mboxID = i;	// mboxID
		MailBoxTable[i].originalSlots = 0;	// size of slot
		
		// Initialize each table of block recieve and send list
		for(int j=0; j < 100; j++){
			MailBoxTable[i].blockRecieveList[j].status = EMPTY;
			MailBoxTable[i].blockRecieveList[j].pid = -1;
			MailBoxTable[i].blockSendList[j].status = EMPTY;
			MailBoxTable[i].blockSendList[j].pid = -1;
		}
	}else{
		// create all the left of mail box table
		MailBoxTable[i].status = EMPTY;
		MailBoxTable[i].mboxID = -1;
		MailBoxTable[i].pid = -1;
		MailBoxTable[i].slots = 0;
		MailBoxTable[i].slot_size = 0;
		MailBoxTable[i].release = 0;

		// Initialize each table of block recieve and send list
		for(int j=0; j < 100; j++)
		{
			MailBoxTable[i].blockRecieveList[j].status = EMPTY;
			MailBoxTable[i].blockRecieveList[j].pid = -1;
			MailBoxTable[i].blockSendList[j].status = EMPTY;
			MailBoxTable[i].blockSendList[j].pid = -1;
		}
	}
    } 

    // Initialize mail slot
    for(int i=0; i < MAXSLOTS*MAXMBOX; i++){
	MailSlot[i].mboxID = -1;
	MailSlot[i].status = EMPTY;
	MailSlot[i].mem_size = 0;
    }
    // Initialize USLOSS_IntVec and system call handlers,
    // allocate mailboxes for interrupt handlers.  Etc... 

    enableInterrupts();

    // Create a process for start2, then block on a join until start2 quits
    if (DEBUG2 && debugflag2)
        USLOSS_Console("start1(): fork'ing start2 process\n");
    kid_pid = fork1("start2", start2, NULL, 4 * USLOSS_MIN_STACK, 1);
    if ( join(&status) != kid_pid ) {
        USLOSS_Console("start2(): join returned something other than ");
        USLOSS_Console("start2's pid\n");
    }
    
    return 0;
} /* start1 */

/* ------------------------------------------------------------------------
   Name - MboxCreate
   Purpose - gets a free mailbox from the table of mailboxes and initializes it 
   Parameters - maximum number of slots in the mailbox and the max size of a msg
                sent to the mailbox.
   Returns - -1 to indicate that no mailbox was created, or a value >= 0 as the
             mailbox id.
   Side Effects - initializes one element of the mail box array. 
   ----------------------------------------------------------------------- */
int MboxCreate(int slots, int slot_size)
{
	int m_id = 0;
	while(m_id < MAXMBOX && MailBoxTable[m_id].mboxID != -1){
		m_id++;	
	}
        m_id = m_id % MAXMBOX;
	// If mail box already received id, then error
	if(MailBoxTable[m_id].mboxID != -1){
		return -1;
	}

	// catch error case for size of slot
	if(slots < 0 || slot_size < 0 || slot_size>MAX_MESSAGE){
		return -1;
	}
	// add each mail box size
	totalMailBoxSlot = totalMailBoxSlot + slots;

	// initialize mailbox table and mailslot table
	MailBoxTable[m_id].mboxID = m_id;
	MailBoxTable[m_id].pid = getpid()%MAXPROC;
	MailBoxTable[m_id].status = RUNNING;
	MailBoxTable[m_id].slots = slots;
	MailBoxTable[m_id].slot_size = slot_size;
	MailBoxTable[m_id].originalSlots = slots;
	MailBoxTable[m_id].release = 0;

	// create mailslot for each mailbox
	int index=0;
 
	while(MailSlot[index].mboxID != -1){
		index++;
	}
	
	// Initialize the mail slot
	for(int i=index; i < index + slots; i++){
		MailSlot[i].mboxID = m_id;
		MailSlot[i].status = EMPTY;
		MailSlot[i].mem_size = slot_size;	
	}
	return MailBoxTable[m_id].mboxID;
} /* MboxCreate */


/* ------------------------------------------------------------------------
   Name - MboxSend
   Purpose - Put a message into a slot for the indicated mailbox.
             Block the sending process if no slot available.
   Parameters - mailbox id, pointer to data of msg, # of bytes in msg.
   Returns - zero if successful, -1 if invalid args.
   Side Effects - none.
   ----------------------------------------------------------------------- */
int MboxSend(int mbox_id, void *msg_ptr, int msg_size)
{
	check_kernel_mode("MboxReceive");
	mailbox *current = currentMailBox(mbox_id);
	int emptySlotExist=0;
	int slots = current->slots;
	int num_slot;

	// Error case for return -1
	if(slots > MAXSLOTS || current->status == EMPTY){
		return -1;
	}
	if(current->slot_size < msg_size || msg_size < 0){
		return -1;
	}

	// Check the current mail slot match with conditions
	for(int j=0; j < MAXSLOTS; j++){
		if(MailSlot[j].status == EMPTY && MailSlot[j].mboxID == current->mboxID && current->slots > 0){
			emptySlotExist=1;
			num_slot=j;
			break;
		}
	}
	
// If there is empty slot in mailslot
if(current->originalSlots != 0){

	// If there is empty slot when we recieve
	if(emptySlotExist==1){
		// Check the block list that waiting for input message
		int pid = -1;
		int index;
		// Check the block recieve list
		for(int j=0; j < 100; j++){
			// If there is recieve status RUNNING
			if(current->blockRecieveList[j].status == EMPTY){
				// Get pid to unblock
				pid = current->blockRecieveList[j].pid;
				// Save all the information of current mailbox to recieve list
				current->blockRecieveList[j].mboxID = current->mboxID;
				current->blockRecieveList[j].mem_size = msg_size;
				current->blockRecieveList[j].status = RUNNING;
				if(msg_ptr==NULL){
					msg_ptr="";
				}
				memcpy(&(current->blockRecieveList[j].message), msg_ptr, current->slot_size);
				index = j;	
				break;
			}
		}
		// unblock if pid is not -1
		if( pid != -1){
			unblockProc(pid);
		}else{
		// If pid is -1 then edit all of information in current mailslot
			slots--;
			current->slots = slots;
			MailSlot[num_slot].mboxID = mbox_id;
			MailSlot[num_slot].status = RUNNING;
			MailSlot[num_slot].mem_size = msg_size;
			memcpy(&(MailSlot[num_slot].message), msg_ptr, msg_size);
		}

	}
	// case1: IF there is no empty slot in mailslot, then block
	// case2: If slots in mailbox has no space, then block
	else{
		// Save all the information of current to block send list
		for(int i=0; i < 50; i++){
			if(current->blockSendList[i].pid == -1){
				current->blockSendList[i].pid = getpid()%MAXPROC;
				current->blockSendList[i].mboxID = current->mboxID;
				current->blockSendList[i].status = WAIT;
				current->blockSendList[i].mem_size = msg_size;
				memcpy(&(current->blockSendList[i].message), msg_ptr, msg_size);
				break;
			}
		}
		// block
		blockMe(12);
	}
}
else{

	// If size of slot is 0
	int index1=0;
	// find a send list which is not empty
	while(current->blockSendList[index1].status != EMPTY){
		index1++;
	}

	// Update information of send list 
	current->blockSendList[index1].status = RUNNING;
	current->blockSendList[index1].pid = getpid();
	current->blockSendList[index1].mem_size = msg_size;
	memcpy(&(current->blockSendList[index1].message), msg_ptr, current->blockSendList[index1].mem_size);

	int messagesize = current->blockSendList[0].mem_size;

	// If there is recieve list RUNNING
	if(current->blockRecieveList[0].status != EMPTY){
		// Copy message from send list to recieve list slot
		memcpy(current->blockRecieveList[0].message, &(current->blockSendList[0].message), messagesize);
		current->blockRecieveList[0].mem_size = current->blockSendList[0].mem_size;
	
		// get previous pid to unblock itself
		int previouspid = current->blockRecieveList[0].pid;
		int index2=0;

		// Free the current send lost and move all the info
		while(current->blockSendList[index2+1].status != EMPTY){
			current->blockSendList[index2] = current->blockSendList[index2+1];
			index2++;
		}
		current->blockSendList[index2].pid = -1;
		current->blockSendList[index2].status = EMPTY;

		// unblock the process
		unblockProc(previouspid);
	}else{
		// block if it is empty slot
		blockMe(12);
		int index=0;
		// Free the current send lsit slot and move all the info
		while(current->blockSendList[index+1].status != EMPTY){
			current->blockSendList[index] = current->blockSendList[index+1];
			index++;
		}
		current->blockSendList[index].status = EMPTY;
		current->blockSendList[index].pid = -1;
	}
}

	// return -3 if it is zap or release
	if(isZapped()  || current->release==1){
		return -3;
	}
	if(exit11==1){
		exit11=0;
		return -1;
	}
	return 0;
} /*  MboxSend */


/* ------------------------------------------------------------------------
   Name - MboxReceive
   Purpose - Get a msg from a slot of the indicated mailbox.
             Block the receiving process if no msg available.
   Parameters - mailbox id, pointer to put data of msg, max # of bytes that
                can be received.
   Returns - actual size of msg if successful, -1 if invalid args.
   Side Effects - none.
   ----------------------------------------------------------------------- */
int MboxReceive(int mbox_id, void *msg_ptr, int msg_size)
{
	check_kernel_mode("MboxSend");
	mailbox *current = currentMailBox(mbox_id);
	int returnValue;
	int status=2;
	// Catch error for mbox id and current stats should not be EMPTY
	if(current->mboxID!=mbox_id || current->status == EMPTY){
      		return -1;
    	}
	// Catch error for msg size
  	if(msg_size<0){
      		return -1;
  	}
	if(isZapped() || current->release==1){
		return -3;
	}
	int current_slot=0;


	// Find slot that match with mbox_ID
	for(int i=0; i < MAXSLOTS; i++){
		if(MailSlot[i].mboxID == mbox_id && MailSlot[i].status == RUNNING){
			current_slot = i;
			status=1;
			break;
		}
	}
// If size of current mailbox is not 0	
if(current->originalSlots !=0){
	// If there is message in mailslot
	if(status==1){
		// Return -1 if memory size is bigger than current mailbox slot size
		if(MailSlot[current_slot].mem_size > msg_size){
			exit11=1;	
			return -1;
		}
		// Copy currrent mail slot message and save return value with memory size
		memcpy(msg_ptr, &(MailSlot[current_slot].message), msg_size);
		returnValue = MailSlot[current_slot].mem_size;
		// free mailbox in mailboxslot after receive
		while(MailSlot[current_slot+1].status != EMPTY && MailSlot[current_slot].mboxID == MailSlot[current_slot+1].mboxID){
			MailSlot[current_slot] = MailSlot[current_slot+1];
			current_slot++;
		}
		MailSlot[current_slot].status = EMPTY;

		// If there is wait mailslot in send list, copy them into Mailslot
		if(current->blockSendList[0].pid != -1){
			int pid = current->blockSendList[0].pid;
			int slotsize = current->originalSlots;

			// Move all the information from index 0 to last of index
			MailSlot[slotsize-1].mboxID = current->blockSendList[0].mboxID;
			MailSlot[slotsize-1].status = RUNNING;
			MailSlot[slotsize-1].mem_size = current->blockSendList[0].mem_size;
			memcpy(&(MailSlot[slotsize-1].message), &(current->blockSendList[0].message), msg_size);

			// unblock the process
			unblockProc(pid);
			int index=0;
	
			// move all the information in lbock send list and free index 0
			while(current->blockSendList[index+1].pid != -1){
				current->blockSendList[index] = current->blockSendList[index+1];
				index++;
			}
			current->blockSendList[index].pid = -1;
		}else{
			// slot size is increase
			int slot = current->slots;
			slot++;
			current->slots = slot;
		}

		
	}
	// If there is no message in mailslot, block
	else{
		// save current pid to use in the future
		for(int j=0; j < 50; j++){
			if(current->blockRecieveList[j].pid == -1){
				current->blockRecieveList[j].pid = getpid()%MAXPROC;
				current->blockRecieveList[j].mboxID = current->mboxID;
				break;
			}
		}
		// block
		blockMe(12);
		// copy message of block recieve list to msg_ptr with message size
		memcpy(msg_ptr, &(current->blockRecieveList[0].message), msg_size);
		// set up return value which is size of memory
		returnValue = current->blockRecieveList[0].mem_size;
		// Free mailbox in mailslot after receive
		int index1=0;

		// move all the information in block recievelist and free index 0
		while(current->blockRecieveList[index1+1].status != EMPTY){
			current->blockRecieveList[index1] = current->blockRecieveList[index1+1];
			index1++;
		}
		current->blockRecieveList[index1].status = EMPTY;
		current->blockRecieveList[index1].pid = -1;
		
	}
}
else{

	int index=0;
	// Find block slot which is RUNNING
	while(current->blockRecieveList[index].status != EMPTY){
		index++; // increase index
	}
	// get slot size
	int slotsize1 = current->blockRecieveList[index].mem_size;

	// Edit all the information of current and add to block recieve list
	current->blockRecieveList[index].status = RUNNING;
	current->blockRecieveList[index].pid = getpid();
	memcpy(&(current->blockRecieveList[index].message),msg_ptr, slotsize1);
	current->blockRecieveList[index].mem_size = msg_size;

	// If block send list has element RUNNING
	if(current->blockSendList[0].status !=EMPTY){
		// Copy information of message to msg_ptr
		memcpy(msg_ptr, &(current->blockSendList[0].message), current->blockSendList[0].mem_size);
		// set up return value
		returnValue = current->blockSendList[0].mem_size;
			
		// get slot size of first send slot
		// get pid of first send slot
		int currentPid = current->blockSendList[0].pid;
		int index1 = 0;
		// move all the information of block recievelist and free index 0
		while( current->blockRecieveList[index1+1].status != EMPTY){
			current->blockRecieveList[index1] = current->blockRecieveList[index1+1];
			index1++;
		}
		current->blockRecieveList[index1].status = EMPTY;
		current->blockRecieveList[index1].pid = -1;
		// unblock the process
		unblockProc(currentPid);
	
	}else{
		// block
		blockMe(12);
		int size = current->blockRecieveList[0].mem_size;
		// copy message of first index in block list to msg_ptr
		memcpy(msg_ptr, &current->blockRecieveList[0].message, size);
		// set up return value
		returnValue = size;
		int index2=0;
		// Move all the information of block recieve list which is RUNNING and free index 0
		while( current->blockRecieveList[index2+1].status != EMPTY){
			current->blockRecieveList[index2] = current->blockRecieveList[index2+1];
			index2++;
		}
		current->blockRecieveList[index2].status = EMPTY;
		current->blockRecieveList[index2].pid = -1;
	}
}

	// return -3 if it is zap or realease
	if(isZapped() || current->release==1){
		return -3;
	}

	return returnValue;
} /* MboxReceive */


/*
	Name: MboxRelease
	Purpose: Releases a previously created mailbox mailbox. You will need to devise a means of handling processes that are blocked on a mailbox being released. Essentially, you will need to have a blocked process return -3 from the send or receive that caused it to block. You will need to have the process that called MBOX release unblock all the blocked process.
	parameter: mbox_id ( to find current Mailslot)
	Returns: normally it return 0, but others for error cases
	Side effects: none.
*/
int MboxRelease(int mbox_id){

	// get current mail box
	mailbox *current = currentMailBox(mbox_id);
	// Find error case for empy status and none match mbox id
	if(current->status == EMPTY || current->mboxID != mbox_id){
		return -1;
	}

	// set up current status to empty
	current->status = EMPTY;
	current->mboxID = -1;
	current->release = 1;
	// release all the process in send wait list
	int index=0;
	int pid=0;
	int unblock=0;


	// If there is block send list which is running loop until unblock all the process
	while(current->blockSendList[index].pid != -1){  
		// get pid
		pid = current->blockSendList[index].pid;
		// current status change to empty
		current->blockSendList[index].status = EMPTY;
		// current pid change to -1
		current->blockSendList[index].pid = -1;

		// unblock the process which contains pid
		unblock = unblockProc(pid);
		// if pid is not -1 then index--
		if(current->blockRecieveList[index].pid != -1){
			index--;
		}
		if(unblock==-1){
			return -3;
		}

		index++;
	}

	int index1=0;
	int pid1;
	int unblock1;

	// If there is block recieve lis which is running loop until block all the process
	while(current->blockRecieveList[index1].pid != -1){
		// get pid
		pid1 = current->blockRecieveList[index1].pid;
		// current status change to empty
		current->blockRecieveList[index1].status = EMPTY;
		// current pid change to -1
		current->blockRecieveList[index1].pid = -1;
		// unblock the process
		unblock1 = unblockProc(pid1);

		if(current->blockRecieveList[index1].pid != -1){
			index1--;
		}	
		if(unblock1==-1){
			return -3;
		}
		index1++;
	}

	return 0;
}

/*
	Name: MboxCondReceive
	Purpose: Conditionally receive a message from a mailbox. Do not block the invoking process. Rather, if there is no messagein the mail box, the value -2 is returned
	parameter: mbox id, message ptr, message size
	returns: -3 : process zap
		 -2 : no message available
		 -1 : illegal values given as arguments
		 >=0 : the size of the message received

	side effect: none
*/
int MboxCondReceive(int mbox_id, void *msg_ptr, int msg_size){

	// get current mail box
	mailbox *current = currentMailBox(mbox_id);
	// get mail box slot
	int slotSize = current->slots;

	// catch error for illegal values input
	if(current->mboxID != mbox_id || (current->status == EMPTY && !(current->release))){
			return -1;
	}
	
	// no message available
	if(slotSize >= current->originalSlots){
		return -2;
	}

	// no message abailable
	if(current->originalSlots <= 0){
			return -2;
	}
	int index=0;

	// Find current mail slot
	while(MailSlot[index].mboxID != current->mboxID){
		index++;
	}
 	
	// illegal value given as arguments
	if(msg_size < MailSlot[index].mem_size || current->release){
		return -1;
	}
	int size = MailSlot[index].mem_size;

	// copy current slot message to msg_ptr with proper size
	memcpy(msg_ptr, &(MailSlot[index].message), size);

	// Move all the mailslot and free at the first of mail slot
	while(MailSlot[index+1].status != EMPTY){
		MailSlot[index] = MailSlot[index+1];
		index++;
	}
	MailSlot[index].status = EMPTY;
	
	// slot size increase
	slotSize++;
	current->slots = slotSize;

	int index1=0;

	// find block send list which is RUNNING
	if(current->blockSendList[0].status != EMPTY){
		int pid = current->blockSendList[0].pid;
		// Move all the information in block send list and free at the index 0
		while(current->blockSendList[index1+1].status != EMPTY){
			current->blockSendList[index1] = current->blockSendList[index1+1];
			index1++;
		}
		current->blockSendList[index1].status = EMPTY;
		// unblock the process
		unblockProc(pid);
	}
	
	// return size
	return size;
}

// totalSlots()
// parameter: none
// get total size of slot
int totalSlots(){
  int result=0;
  // check all the size of slot in mail slot which is for RUNNING
  for(int index=0; index<MAXMBOX*2; index++){
	if(MailSlot[index].status != EMPTY){
		result = result + 1;	
	}
  }
  
  return result+1;
}
/*
	Name: int MboxCondSend
	purpose: Conditionally send a message to a mailbox. Do not block the invoking process. Rather, if there is no empty slot in the mailbox in which to place the message, the value -2 is returned
	parameter: mbox id, message ptr, and message size
	return: -3: process was zapd
		-2: mailbox full, message not sent; or no slots avilable in the system
		-1: illegal values given as arguments
		 0: message sent successfully
	side effects: none

*/
int MboxCondSend(int mbox_id, void *msg_ptr, int msg_size){

	mailbox *current = currentMailBox(mbox_id);
	int currentSlot = current->slots;
	
	// If process is zapped
	if(isZapped()){
		return -3;
	}
	// if given message size is less than 0
	if(msg_size < 0 || current->release==1){
		return -1;
	}
	// If there is no slots in mailbox
	if(totalSlots() > MAXSLOTS){
		return -2;
	}	
	// mailbox is full
	if(currentSlot <= 0){
		return -2;
	}
	int index=0;
	// find current mail slot
	while(MailSlot[index].mboxID != current->mboxID){
		index++;
	}
	
	// find current mail slot which is RUNNING
	while(MailSlot[index].status != EMPTY){
		index++;
	}

	
	// change all the information in current mail slot
	slotPtr newSlot = &(MailSlot[index]); 
	newSlot->status = RUNNING; // stats
	newSlot->mboxID = current->mboxID; // mbox id
	newSlot->mem_size = msg_size; // message size
	memcpy(&(newSlot->message), msg_ptr, msg_size); // copy message from msg_ptr
	currentSlot--; // decrease size of slot
	current->slots = currentSlot;

	// Find block slot in list which is RUNNING
	if(current->blockRecieveList[0].status != EMPTY){

		int pid=current->blockRecieveList[0].pid;
		int index=0;
		// move all the information for RUNNING and free index 0
		while(current->blockRecieveList[index+1].status !=EMPTY){
			current->blockRecieveList[index] = current->blockRecieveList[index+1];
			index++;
		}
		current->blockRecieveList[index].status=EMPTY;
		unblockProc(pid);
	}
	


	return 0; // message sent succesfully
}



