  /*
	* name: Francis Kim and Andrew Carlisle
	* project name: phase3.c
	* Date: 10/22/2017 9:00pm
	* purpose: you are to design and implement a support level that creates user mode processes and then provides system services to the user processes. 
	* All of these services will be requested by user programs through the syscall interface. 
	* Many of the services that the support level provides are almost completely implemented by the kernel primitives
*/

#include <usloss.h>
#include <phase1.h>
#include <phase2.h>
#include <phase3.h>
#include <usyscall.h>
#include <stdlib.h>
#include <sems.h>
#include <stdio.h>
#include <string.h>
#include <libuser.h>

extern int start3 (char *);


// semstable stores all the sems in the system
SemsS SemsTable[MAXSEMS];

// pass message through variable to send it
static char message[50];

// exit status
int exit1 = 1;

// the current mailbox
int mailBox;

// process table stores all the processes in the system.
ProcS ProcessTable[MAXPROC];



/*
	void targetRemove(ProcS* target, int pid):
	help method for removing the target process, it is in processRemove
	SideEffect: none
*/

void targetRemove(ProcS* target, int pid){

	ProcS* p;
	ProcS* c;
	int pidLessThanZero=1;

	if(pid < 0){
		pidLessThanZero = -1;
	}

	if(pidLessThanZero < 0){
		return;
	}else{ 

		if(pid > 0){
			MboxRelease(target->mailboxID);	// release the mail box
			target->mailboxID=-1;	// change mailbox ID to -1 which means remove

			// remove all child
			if(target->PPP != NULL && target->PPP->currentPid >= 0){

				p = target->PPP;	//  p points to parent process of target

				c = p->CPP;	// c points to child process of p
				target->PPP = NULL;	// set child is empty

				// if child pid is equal to current target pid
				if(c->currentPid == pid){
					p->CPP = p->CPP->NSP;	
				}else{
					// loop until pid of child next sibling ptr is equal to target pid
					for(int i=0; i < MAXPROC; i++){
						if(c->NSP == NULL && c->NSP->currentPid == pid){
							c = c->NSP;
							break;
						}
					}

					c->NSP=c->NSP->NSP;
				}
			}
		}
	}
}

/*
	void getPID(systemArgs args):
	get the processID from systemArgs.
*/

void getPID(systemArgs args){

	systemArgs* args1 = (systemArgs*) (long) args.number; // get argument

	args1->arg1 = (void*) (long) getpid();	// get pid
	return;

} 


/*
	void processRemove(ProcS* target)
	Remove the target process from process table
	SideEffect: none
*/

void processRemove(ProcS* target){

	int pid;

	pid = target->currentPid;
	// if pid is less than 0
	if(pid<0){
		pid=-1;
	}

	if(pid == -1){
		return;	// return nothing
	}else{
		// remove target process and set pid to -1
		targetRemove(target, pid);
		target->currentPid = -1;
	}

}

/*
	ProcS* getCurrentProc()
	get current process
*/

ProcS* getCurrentProc(){
	return &(ProcessTable[getpid() % MAXPROC]);
}

/*
	void terminateReal(int stat)
	wait for a child process to terminate, and help terminate method.

*/


void terminateReal(int stat){

	// get current process
	ProcS* currentProcess= getCurrentProc();
	int pid = stat;	

	// if pid is less than 0
	if(pid < 0){
		pid = -1;
	}

	if(pid == -1){
		return;	// return nothing
	}else{

		// loop until child process is NULL
		for(int a=0; a < MAXPROC; a++){
			if(currentProcess->CPP == NULL){
				break;
			}else{
				zap(currentProcess->CPP->currentPid);
			}
		}
		// remove current process
		processRemove(currentProcess);
	}
	return quit(stat);	// return quit status
} // terminateReal


/*
	void terminate(systemArgs args)
	terminates the invoking process and all of its children, and synchronizes with its parent's wait system call.
	Processes are terminated by zap'ing them
	SideEffect: none

*/
void terminate(systemArgs args){

	int stat;
	// get argument
	systemArgs* args1 = (systemArgs*) (long) args.number;

	stat = (int) (long) args1->arg1;
	
	//terminateReal( stat )
	terminateReal( stat );

} // terminate

/*
	setUserMode()
	set to user mode from kernel mode
*/
void setUserMode(){
	USLOSS_PsrSet(USLOSS_PsrGet() & 0b1111110);
}

/*
	long getUserMode()
	return user mode
*/

long userMode(){
	return USLOSS_PsrGet() & 0b1111110;
}

/*
	void spawnLaunch()
	change the process as the user mode. And start the process
*/

void spawnLaunch(){
	// get current pid
	int currentPid = getpid();

	// set one = 1
	int one = 1;

	// child has higher priority than the parent priority
	if(mailBox>=0){
		MboxReceive(mailBox, &(message[0]), one); // receive message
	}

	// When zapped, exit
	if(isZapped()){
		terminateReal(one);
	}

	// change to User Mode
	setUserMode();

	// get the content of current pid and save its value
	int correctpid = currentPid % MAXPROC;

	int result = ProcessTable[correctpid].startFunc(ProcessTable[correctpid].startArg);

	// if it is kerner mode, then quit, if it is in user mode, then terminate.
	systemArgs arg;
	if(userMode()){
		arg.number = SYS_TERMINATE;
		USLOSS_Syscall(&arg);
	}else{
		quit(result);
	}

} // spawnLaunch

/*
	int spawnReal(char *name, int (*function)(char *), char *arg, int stacksize, int priority)
	It will create the process by using a call to fork1 to create a process executing the code in spawnLaunch().
	SideEffect: none
*/
int spawnReal(char *name, int (*function)(char *), char *arg, int stacksize, int priority){

	int resultpid=0; 
	int currentPid=0;
	ProcS* c;	// child
	
	// create mail box which has 10 slot and each has size 1
	mailBox = MboxCreate(10,1);

	// fork1 recieved spawnReal argument
	resultpid = fork1(name, (int (*)(char *))spawnLaunch, arg, stacksize, priority);

	// get current pid % MAXPROC
	currentPid = resultpid%MAXPROC;

	// make new process and insert into process table

	// save pid
	ProcessTable[currentPid].currentPid=resultpid;
	// save function
	ProcessTable[currentPid].startFunc=function;

	// if argument is null then initialize startArg to '\0', if not copy startArg into new process table
	if ( arg == NULL ){
		ProcessTable[currentPid].CPP = NULL;
		ProcessTable[currentPid].startArg[0] = '\0';
		ProcessTable[currentPid].NSP = NULL;
		ProcessTable[currentPid].mailboxID = mailBox;
		ProcessTable[currentPid].PPP = NULL;
	}else{
		ProcessTable[currentPid].CPP = NULL;
		strcpy(ProcessTable[currentPid].startArg, arg);
		ProcessTable[currentPid].NSP = NULL;
		ProcessTable[currentPid].mailboxID = mailBox;
		ProcessTable[currentPid].PPP = NULL;
	}

	int correctPid = getpid() % MAXPROC;
	// if parent is active, add to be parent's child
	if(ProcessTable[correctPid].currentPid == getpid()){

		if(exit1 == 1){
			ProcessTable[currentPid].PPP = &(ProcessTable[correctPid]);
		}else{
			exit1 = 0;
		}

		if(ProcessTable[correctPid].CPP!=NULL){
			c = ProcessTable[correctPid].CPP;
			for(int a=0; a < MAXPROC; a++){
				if(c->NSP == NULL){
					break;
				}else{
					c=c->NSP;
				}
			}
			c->NSP=&(ProcessTable[currentPid]);
		}else{
			ProcessTable[correctPid].CPP=&(ProcessTable[currentPid]);
		}
	}

	if(exit1==1){
		// set up mailbox to -1
		mailBox=-1;
	}
	// send message to current pid mail box
	MboxSend(ProcessTable[currentPid].mailboxID, &(message[0]), 1);
	return resultpid;
} // spawnReal

/*
	systemArgs spawn(systemArgs sysArg)
	Create a user-level process. Use fork1 to create the process, then change it to user-mode.
	If the spawned function returns, it should have the same effect as calling terminate.
*/
systemArgs spawn(systemArgs arg){

	char* name;
	void* function;
	char* argu;
	int stackSize;
	int priority;

	int pidFromFork1 = 0;

	// save arg.number into *sysArg
	systemArgs *sysArg = (systemArgs*) (long) arg.number;

	// initialize the each parameter that receieved from spawnReal
	name = sysArg->arg5;
	function = sysArg->arg1;
	argu = sysArg->arg2;
	stackSize = (int) (long) sysArg->arg3;
	priority = (int) (long) sysArg->arg4;

	// spawnReal return pid of fork1
	pidFromFork1 = spawnReal( name, function, argu, stackSize, priority);

	// save pid to function	
	if(pidFromFork1 > 0){
		// arg1: save pid of newly created process
		sysArg->arg1 = (void*) (long) pidFromFork1;
	}else{
		// arg1: if process not created, then save -1
		sysArg->arg1 = (void*) (long) -1;
	}

	if(pidFromFork1 <= 0){
		// arg4: save -1 if it is illegal value
		sysArg->arg4 = (void*) (long) -1;
	}
	else{
		// arg4: save 0 otherwise
		sysArg->arg4 = (void*) (long) 0;
	}

	return *sysArg;

} // spawn

/*
	int waitReal(int *status)
	it is waiting for child process to terminate
*/
int waitReal(int *status){

	int pid=0;
	int currentPid;

	pid = join(status);

	currentPid = pid%MAXPROC;

	// remove process if join successfully
	if(pid>=0){
		if(ProcessTable[currentPid].currentPid == pid){
			processRemove(&(ProcessTable[pid]));
		}
	}

	return pid;
} // waitReal

/*
	systemArgs wait(systemArgs args)
	Wait for a child process to terminate
*/

systemArgs wait(systemArgs args){
	
	// get pid
	int waitpid = waitReal((int*) &( args.arg2));

	// arg1: process id of terminating child
	args.arg1 = (void*) (long) waitpid;

	// if it is zapped, then terminate
	if(isZapped()){
		terminateReal(1);
	}
	// set to user mode
	userMode(); 
	return args;
} // wait
 

/*
	void semCreate(systemArgs args)
	Create child process of current process in user mode
*/
void semCreate(systemArgs args){
	int argOne, result, index=0;

	// get argument

	systemArgs* args1=(systemArgs*) (long)args.number;
	argOne= (int) (long)args1->arg1;

	for(int a=0; a < MAXSEMS; a++){
		if(SemsTable[a].mailboxID < 0){
			break;
		}else{
			index++;
		}
	}

	// save index = -1 when size is MAXSEMS or arg1 is less than 0
	if(index==MAXSEMS){
		index = -1;
	}

	if(argOne < 0){
		index = -1;
	}

	//create new mailbox
        if(index != -1){
		SemsTable[index].semsCounter=argOne;
	}

	result = index;

	if(index != -1){
		SemsTable[index].mailboxID=MboxCreate(0,0);
		SemsTable[index].zap = 0;
	}
	

	if(result>=0){
		// set up output
		args1->arg1 = (void* ) (long) result;
		// 0 otherwise
	 	args1->arg4 =(void* )(long) 0;
	}
	else{
		// set up output
		args1->arg1 = (void* ) (long) result;
		// -1 if initial value is negative or no semaphores are available
		args1->arg4 = (void* )(long) -1;
		
	}

	// if is Zapped, terminate
	if(isZapped()){
		terminateReal(1);
	}
	// set up to User Mode
	userMode();

	return;
} // semCreate

/*
	semP
	Performs a P operation on a semaphore
	input 
	arg1: semaphore handle
	output
	arg4: -1 if semaphore handle is invalid, 0 otherwise.
*/
void semP(systemArgs args){
	int argfour;
	SemsS* sbox;

	systemArgs* number = (systemArgs*) (long) args.number;
	// get args.number
	systemArgs* args1 = number;

	// save sems box into sbox
	sbox= &(SemsTable[(int) (long)args1->arg1]);

	// if Id is available
	if(sbox->mailboxID>=0){
		int semsCounter = sbox->semsCounter-1;

		sbox->semsCounter = semsCounter;

		if(sbox->semsCounter<0){	// if sems counter < 0

			// send to mailbox for block
			MboxSend(sbox->mailboxID, (void*)&message, 0);

			// if is Zapped, terminate
			if(sbox->zap){
				terminateReal(1);
			}

			argfour = 0;
		}else{
			argfour = 0;
		}
		
	}else{
		argfour = -1;
	}

	void* save = (void*) (long) argfour;
	args1->arg4 = save;

	// if is Zapped, terminate
	if(isZapped()){
		terminateReal(1);
	}

	// set up to User Mode
	userMode();
} 	// semP

/*
	name: semV
	performs a V operation on a semaphore
	input arg1: semaphore handle
	output arg4: -1 if semaphore handle is invalid, 0 otherwise
*/
void semV(systemArgs args){

	 

	int argfour;

	SemsS* sbox;

	systemArgs* number = (systemArgs*) (long) args.number;

	// get args.number
	systemArgs* args1 = number;

	// save sems box into sbox
	sbox = &(SemsTable[(int) (long)args1->arg1]);
 
	if(sbox->mailboxID>=0){	// if Id is vailable

		int semsCounter = sbox->semsCounter+1;
		sbox->semsCounter = semsCounter;

		// if sems Counter is less than 0 
		if(sbox->semsCounter <= 0){
			// receive from mailbox and unblock it
			MboxReceive(sbox->mailboxID, (void*) &message, 0);
			argfour = 0;	// arg4 = 0;
		}else{
			argfour = 0;	// arg4 = 0;
		}
		
	}else{	// if Id is unvailable
		argfour = -1;
	}

	void* save = (void*) (long) argfour;
	args1->arg4 = save;

	// if is Zapped, terminate
	if(isZapped()){
		terminateReal(1);
	}

	// set up to User Mode
	userMode();

} // semV

/*
	name: semFree
	Frees a semaphore
	input
	arg1: semaphore handle
	output
	arg4: -1 if semaphore handle is invalid, 1 if there were processes blocked
	on the semaphore, 0 otherwise
*/
void semFree(systemArgs args){
	int argfour;
	SemsS* sbox;
	// get argument
	systemArgs* number = (systemArgs*) (long) args.number;
	systemArgs* args1 = number;

	// find the semsbox

	sbox= &(SemsTable[(int) (long)args1->arg1]);

	// if Id is invailable
	if(sbox->mailboxID>=0){
		argfour = 0;
		if(sbox->semsCounter<0){
			argfour=1;
			sbox->zap=1;
			MboxRelease(sbox->mailboxID);
			sbox->semsCounter=0;
			sbox->mailboxID=-1;
		}else{
			sbox->zap=1;
			MboxRelease(sbox->mailboxID);
			sbox->semsCounter=0;
			sbox->mailboxID=-1;
		}
		
	// if Id is vailable
	}else{
		argfour = -1;
	}
	args1->arg4 =(void*) (long) argfour;

	// if it is Zapped, terminate
	if(isZapped()){
		terminateReal(1);
	}
	// set up to User Mode
	userMode();
}

/*
	name: getTimeofDay
	get the time of the day 
*/
void getTimeofDay(systemArgs args){
	// get argument
	systemArgs* args1 = (systemArgs*) (long) args.number;
	int stat=0;
	
	USLOSS_DeviceInput(USLOSS_CLOCK_INT, 0, &(stat));
	args1->arg1 = (void*) (long) stat;
	
	return;
} //getTimeofDay

/*
	* name: cpuTime
	 get the CPU time from the system
*/
void cpuTime(systemArgs args){
	// get argument
	systemArgs* args1=(systemArgs*) (long)args.number;

	args1->arg1=(void*) (long)readtime();
	return;
} //cpuTime


/*
	int start2(char *arg)
	it is start function from the start3. it will initialize all the information of process table and semephore table
	and it will initialize all the systemCallVec value.
	
*/
int start2(char *arg){
	int pid; 
	int status; 
	int index;

	/*
	 * Check kernel mode here.
	 */
	if (!(USLOSS_PsrGet() & 0b00000001)) { 
		USLOSS_Console("ERROR: It is not in kernel mode, it is in user mode\n"); 
		USLOSS_Halt(1);  
	}

	/*
	 * Data structure initialization as needed...
	 */

	mailBox=-1;

	// initialize the process table
	
	for(index=0;index<MAXPROC;index++){
		ProcessTable[index].currentPid = -1;
		ProcessTable[index].mailboxID=-1;
	}

	// initialize the sems table
	for(index=0;index<MAXSEMS;index++){
		SemsTable[index].semsCounter=0;
		SemsTable[index].mailboxID=-1;
	}

	// initialize the syscall vector
	systemCallVec[SYS_SPAWN] = (void*) spawn;
	systemCallVec[SYS_TERMINATE] = (void*) terminate;
	systemCallVec[SYS_SEMFREE] = (void*) semFree;
	systemCallVec[SYS_SEMP] = (void*) semP;
	systemCallVec[SYS_GETTIMEOFDAY] = (void*) getTimeofDay;
	systemCallVec[SYS_GETPID] = (void*) getPID;
	systemCallVec[SYS_CPUTIME] = (void*) cpuTime;
	systemCallVec[SYS_SEMCREATE] = (void*) semCreate;
	systemCallVec[SYS_SEMV] = (void*) semV;
	systemCallVec[SYS_WAIT] = (void*) wait;
       /*
        * Create first user-level process and wait for it to finish.
        * These are lower-case because they are not system calls;
        * system calls cannot be invoked from kernel mode.
        * Assumes kernel-mode versions of the system calls
        * with lower-case names.  I.e., Spawn is the user-mode function
        * called by the test cases; spawn is the kernel-mode function that
        * is called by the syscallHandler; spawnReal is the function that
        * contains the implementation and is called by spawn.
        *
        * Spawn() is in libuser.c.  It invokes USLOSS_Syscall()
        * The system call handler calls a function named spawn() -- note lower
        * case -- that extracts the arguments from the sysargs pointer, and
        * checks them for possible errors.  This function then calls spawnReal().
        *
        * Here, we only call spawnReal(), since we are already in kernel mode.
        *
        * spawnReal() will create the process by using a call to fork1 to
        * create a process executing the code in spawnLaunch().  spawnReal()
        * and spawnLaunch() then coordinate the completion of the phase 3
        * process table entries needed for the new process.  spawnReal() will
        * return to the original caller of Spawn, while spawnLaunch() will
        * begin executing the function passed to Spawn. spawnLaunch() will
        * need to switch to user-mode before allowing user code to execute.
        * spawnReal() will return to spawn(), which will put the return
        * values back into the sysargs pointer, switch to user-mode, and
        * return to the user code that called Spawn.
        */

	spawnReal("start3", start3, NULL, USLOSS_MIN_STACK, 3);

       /* Call the waitReal version of your wait code here.
        * You call waitReal (rather than Wait) because start2 is running
        * in kernel (not user) mode.
        */

	pid = waitReal(&status);
	return pid;
} /* start2 */

