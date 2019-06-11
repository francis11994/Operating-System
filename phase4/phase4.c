/*	
	*	name: Francis Kim and Andrew Carlisle
	*	file: phase4.c
	*	purpose: the simulation of the clock driver, disk driver, and term driver.
	*	They will communicate with the hardware to get the information or do
	*	the operation. On the other hand, it will communicate with the operating
	*	system to know what should the hardware do.
	*
*/
#include <phase1.h>
#include <phase2.h>
#include <phase3.h>
#include <phase4.h>
#include <providedPrototypes.h>
#include <libuser.h>
#include <stdio.h>
#include <stdlib.h> /* needed for atoi() */
#include <usyscall.h>

static int ClockDriver(char *);
static int DiskDriver(char *);
static int TermDriver(char *);
static int TermWriter(char* );
void sleepReal(int second);
static int TermReader(char* );
static void sleep(systemArgs *sysArg);

int trackSz[2], semRunning, diskMBox[2], termMBox[4][5],a=-2,b=-2, termWriting[4],m[4], termSz[4], termNum[4], termMb[4];
procStruct procTable[MAXPROC];
procStruct* sleep_queue=NULL, *disk_queue[2];
char* termBuff[4];
int* i=0;

#define CHECK_KERNEL_MODE {    \
if (!(USLOSS_PsrGet() & 0b00000001)) { \
USLOSS_Console("ERROR: phase3.c: User Mode is active.\n"); \
USLOSS_Halt(1);  \
}  \
}


/*
    *   popDiskQueue is the help function fo rthe disk driver to get
    *   the element that it needs for writing or reading
    *   it will return the process struct that in the first Queue
 */
procStruct* popDiskQueue(int unit, int track){
    procStruct* tmp; 
    tmp=disk_queue[unit];
    if(tmp->track>=track)
        return NULL;
    while(tmp->nextDiskProc[unit]!=NULL&&tmp->target==tmp->nextDiskProc[unit]->target){
        if(tmp->nextDiskProc[unit]->track >= track){
            return tmp;
        }
        tmp=tmp->nextDiskProc[unit];
    }
    return NULL;
}   //  popDiskQueue

/*
    *   sleeepRead is the actual function to do
    *   the sleep function. the input is the second
    *   the operating system wants to sleep,
    *
 */
void sleepReal(int second){

    // calculate the end time
    int tm=0, pid=0, mailbox=0;
    procStruct* tmp;
    gettimeofdayReal(&tm);
    tm+=second*1000000;

    // create new process and get mailbox
    getPID_real(&pid);
    if(procTable[pid%MAXPROC].pID != pid)
        procTable[pid%MAXPROC].pID=pid;
    if( procTable[pid%MAXPROC].mailboxID <0)
        procTable[pid%MAXPROC].mailboxID = MboxCreate(1,80);
    procTable[pid%MAXPROC].wakeUptime=tm;
    mailbox=procTable[pid%MAXPROC].mailboxID;

    // add to queue
    if(sleep_queue==NULL)
        sleep_queue=&procTable[pid%MAXPROC];
    else if(sleep_queue->wakeUptime >tm){
        tmp=sleep_queue;
        sleep_queue = &procTable[pid%MAXPROC];
        sleep_queue->nextSleepProc=tmp;
    }
    else{
        tmp=sleep_queue;
        while(tmp->nextSleepProc!=NULL && tmp->nextSleepProc->wakeUptime <= tm)
            tmp=tmp->nextSleepProc;
        procTable[pid%MAXPROC].nextSleepProc= tmp->nextSleepProc;
        tmp->nextSleepProc = &procTable[pid%MAXPROC];
    }

    // wait for clock device
    MboxReceive(mailbox,"",1);
}   //  sleepReal

/*
    *   diskReadReal is the actual function to do the disk read from
    *   the disk driver. Kernel mode is necessary
 */
int diskReadReal(int unit, int track, int first, int sectors, void *buffer, int *status){
    int pid, mailbox;
    procStruct* proc, *tmp,*point;

    //check error case
    if(unit>1 || unit <0 || track>=trackSz[unit] || track <0 || first>15|| sectors <0 || buffer==NULL ){
        *status=-1;
        return -1;
    }

    // create process
    getPID_real(&pid);
    proc = &procTable[pid%MAXPROC];
    if(proc->pID != pid)
        proc->pID=pid;
    if( proc->mailboxID <0)
        proc->mailboxID = MboxCreate(1,80);
    mailbox=proc->mailboxID;
    proc->diskBuffer = buffer;
    proc->sectors = sectors;
    proc->track = track;
    proc->first = first;
    proc->unit = unit;
    proc->target = DRIVER_READ;

    //add to queue
    if(disk_queue[unit]==NULL){
        proc -> nextDiskProc[unit] = NULL;
        disk_queue[unit] = proc;
    }
    else{
        tmp=disk_queue[unit];
        point=disk_queue[unit];
        while(tmp->nextDiskProc[unit]!=NULL){
            if(tmp->target== DRIVER_WRITE)
                point=tmp;
            tmp=tmp->nextDiskProc[unit];
        }
        if(point->target==DRIVER_WRITE && point->nextDiskProc[unit]==NULL){
            proc-> nextDiskProc[unit] = NULL;
            point->nextDiskProc[unit] = proc;
        }
        else if(point->target==DRIVER_WRITE){
            if(point->nextDiskProc[unit]->track > track){
                proc->nextDiskProc[unit] = point->nextDiskProc[unit];
                point->nextDiskProc[unit] = proc;
            }
            else{
                tmp=point->nextDiskProc[unit];
                while(tmp->nextDiskProc[unit] !=NULL&&tmp->nextDiskProc[unit]->track <= proc->track)
                    tmp=tmp->nextDiskProc[unit];
                proc->nextDiskProc[unit] = tmp->nextDiskProc[unit];
                tmp->nextDiskProc[unit] = proc;
            }
        }
        else{
            if(point->track > track){
                proc->nextDiskProc[unit] = disk_queue[unit];
                disk_queue[unit] = proc;
            }
            else{
                tmp=point;
                while(tmp->nextDiskProc[unit] !=NULL&&tmp->nextDiskProc[unit]->track <= proc->track)
                    tmp=tmp->nextDiskProc[unit];
                proc->nextDiskProc[unit] = tmp->nextDiskProc[unit];
                tmp->nextDiskProc[unit] = proc;
            }
        }

    }

    // wake up disk driver
    MboxSend(diskMBox[unit],"",0);

    // block by the private process
    MboxReceive(mailbox," ",1);
    *status=0;
    return 0;
}   //  diskReadReal

/*
    *   diskWriteReal is the actual function do the disk write from
    * the disk driver. The kernel mode is necessary
 */
int diskWriteReal(int unit, int track, int first, int sectors, void *buffer, int *status){
    int pid, mailbox;
    procStruct* proc, *tmp,*point;

    //check error case
    if(unit>1 || unit <0 || track>=trackSz[unit] || track <0 || first>15|| sectors <0 || buffer==NULL ){
        *status=-1;
        return -1;
    }

    // create process
    getPID_real(&pid);
    proc = &procTable[pid%MAXPROC];
    if(proc->pID != pid)
        proc->pID=pid;
    if( proc->mailboxID <0)
        proc->mailboxID = MboxCreate(1,80);
    mailbox=proc->mailboxID;
    proc->diskBuffer = buffer;
    proc->sectors = sectors;
    proc->track = track;
    proc->first = first;
    proc->unit = unit;
    proc->target = DRIVER_WRITE;

    //add to queue
    if(disk_queue[unit]==NULL){
        proc -> nextDiskProc[unit] = NULL;
        disk_queue[unit] = proc;
    }
    else{
        tmp=disk_queue[unit];
        point=disk_queue[unit];
        while(tmp->nextDiskProc[unit]!=NULL){
            if(tmp->target== DRIVER_READ)
                point=tmp;
            tmp=tmp->nextDiskProc[unit];
        }
        if(point->target==DRIVER_READ && point->nextDiskProc[unit]==NULL){
            proc-> nextDiskProc[unit] = NULL;
            point->nextDiskProc[unit] = proc;
        }
        else if(point->target==DRIVER_READ){
            if(point->nextDiskProc[unit]->track > track){
                proc->nextDiskProc[unit] = point->nextDiskProc[unit];
                point->nextDiskProc[unit] = proc;
            }
            else{
                tmp=point->nextDiskProc[unit];
                while(tmp->nextDiskProc[unit] !=NULL&&tmp->nextDiskProc[unit]->track <= proc->track)
                    tmp=tmp->nextDiskProc[unit];
                proc->nextDiskProc[unit] = tmp->nextDiskProc[unit];
                tmp->nextDiskProc[unit] = proc;
            }
        }
        else{
            if(point->track > track){
                proc->nextDiskProc[unit] = disk_queue[unit];
                disk_queue[unit] = proc;
            }
            else{
                tmp=point;
                while(tmp->nextDiskProc[unit] !=NULL&&tmp->nextDiskProc[unit]->track <= proc->track)
                    tmp=tmp->nextDiskProc[unit];
                proc->nextDiskProc[unit] = tmp->nextDiskProc[unit];
                tmp->nextDiskProc[unit] = proc;
            }
        }

    }

    // wake up disk driver
    MboxSend(diskMBox[unit],"",0);

    // block by the private process
    MboxReceive(mailbox," ",1);
    *status=0;
    return 0;
}   // diskWriteReal

/*
    * diskSizeReal is the actual function to get the size of the disk driver,
    * the diskSize is the number of sectors, number of tracks, and the capacity
    * in one track. The kernel mode is necessary.
 */
int diskSizeReal(int unit, int *sector, int *track, int *disk){
    if(unit>=USLOSS_DISK_UNITS || unit<0)
        return -1;
    sleepReal(1);
    *sector=512;
    *track=16;
    *disk=trackSz[unit];
    return 0;
}   //  diskSizeReal

/*
    *   termReadReal is the actual function to do the read in the terminal
    *   the kernel mode is necessary.
 */
int termReadReal(int unit, int size, char *buff, int *num){
    int pid, mailbox;
    // error check
    if(unit<0|| unit>3 || size<0 || buff==NULL)
        return -1;

    // create process
    getPID_real(&pid);
    if(procTable[pid%MAXPROC].pID != pid)
        procTable[pid%MAXPROC].pID=pid;
    if( procTable[pid%MAXPROC].mailboxID <0)
        procTable[pid%MAXPROC].mailboxID = MboxCreate(1,80);
    mailbox=procTable[pid%MAXPROC].mailboxID;
    procTable[pid%MAXPROC].size=size;
    procTable[pid%MAXPROC].diskBuffer=buff;

    // wake up termRead
    
    MboxSend(termMBox[unit][1], (void*) &pid , 4);
    MboxReceive(mailbox,(void*) num, 32 );

    return 0;
}   //  termReadReal

/*
    *   termWriteReal is the actual function to do the write in the terminal,
    *   the kernel mode is necessary.
 */
int termWriteReal(int unit, int size, char *buff, int *num){
    int pid, mailbox;
    // error check
    if(unit<0|| unit>3 || size<0 || buff==NULL)
        return -1;

    // create process
    getPID_real(&pid);
    if(procTable[pid%MAXPROC].pID != pid)
        procTable[pid%MAXPROC].pID=pid;
    if( procTable[pid%MAXPROC].mailboxID <0)
        procTable[pid%MAXPROC].mailboxID = MboxCreate(1,80);
    mailbox=procTable[pid%MAXPROC].mailboxID;
    procTable[pid%MAXPROC].size=size;
    procTable[pid%MAXPROC].diskBuffer=buff;

    // wake up termRead
    MboxSend(termMBox[unit][2], (void*) &pid , 4);
    MboxReceive(mailbox,(void*) num, 80 );
    return 0;
}   // termWriteReal

/*
    *   sleep is the help function to do the sleepReal, which
    *   is the real function to do sleep
 */
static void sleep(systemArgs *sysArg){
    int second;
    
    // get argument
    second=(int) (long) (sysArg->arg1);
    if(second<0){
        sysArg->arg4 = (void*) (long) -1;
    }
    else{
        sysArg->arg4 = (void*) (long) 0;
        sleepReal(second);
    }

    // if is Zapped, terminate
    if(isZapped())
        terminateReal(1);

    // change into User Mode
    USLOSS_PsrSet(USLOSS_PsrGet() &0b1111110);
}   //  sleep

/*
    *   diskRead is the help function to pass through the 
    *   argument to the diskReadReal, where will do the 
    *   actual disk reading
 */
static void diskRead(systemArgs *sysArg){
    int unit, track, first, sectors,result;
    void *buffer;
    //get argument
    buffer = sysArg->arg1;
    sectors = (int) (long)sysArg->arg2;
    track = (int) (long)sysArg->arg3;
    first = (int) (long)sysArg->arg4;
    unit = (int) (long)sysArg->arg5;

    //run real function
    result = diskReadReal(unit, track, first, sectors, buffer, (int*) &(sysArg->arg1));
    sysArg->arg4 = (void*) (long)result;

    // if is Zapped, terminate
    if(isZapped())
        terminateReal(1);

    // change into User Mode
    USLOSS_PsrSet(USLOSS_PsrGet() &0b1111110);
}   //  diskRead

/*
 *  diskWrite is the help function to read the argument
 *  and transfer the argument to the diskWriteReal to do
 *  the disk writing
 */
static void diskWrite(systemArgs *sysArg){
    int unit, track, first, sectors,result;
    void *buffer;
    //get argument
    buffer = sysArg->arg1;
    sectors = (int) (long)sysArg->arg2;
    track = (int) (long)sysArg->arg3;
    first = (int) (long)sysArg->arg4;
    unit = (int) (long)sysArg->arg5;

    //run real function
    result = diskWriteReal(unit, track, first, sectors, buffer, (int *) &(sysArg->arg1));
    sysArg->arg4 = (void*) (long) result;
   
    // if is Zapped, terminate
    if(isZapped())
        terminateReal(1);

    // change into User Mode
    USLOSS_PsrSet(USLOSS_PsrGet() &0b1111110);
}   //  diskWrite

/*
 *  diskSize is the help function to read the argument
 *  and transfer the argument to the diskSizeReal to get
 *  the size of the disk
 */
static void diskSize(systemArgs *sysArg){
    int unit, result;

    //get argument
    unit = (int) (long) sysArg->arg1;

    result = diskSizeReal(unit, sysArg->arg2, sysArg->arg3, sysArg->arg4);
    sysArg->arg4=(void*) (long)result;

    // if is Zapped, terminate
    if(isZapped())
        terminateReal(1);

    // change into User Mode
    USLOSS_PsrSet(USLOSS_PsrGet() &0b1111110);
}   // diskSize

/*
 *  termRead is the help function to read the argument
 *  and transfer the argument to the termReadReal to do
 *  the terminal reading
 */
static void termRead(systemArgs *sysArg){
    char *buff;
    int size, unit, result;

    //get argument
    buff = sysArg->arg1;
    size = (int) (long)sysArg->arg2;
    unit = (int) (long)sysArg->arg3;

    // run real function
    result = termReadReal(unit, size, buff, (int*) &(sysArg->arg2));
    sysArg->arg4 = (void*) (long) result;

    // if is Zapped, terminate
    if(isZapped())
        terminateReal(1);

    // change into User Mode
    USLOSS_PsrSet(USLOSS_PsrGet() &0b1111110);
}   // termRead

/*
 *  termWrite is the help function to read the argument
 *  and transfer the argument to the termWriteReal to do
 *  the terminal writing
 */
static void termWrite(systemArgs *sysArg){
    char *buff;
    int size, unit, result;

    //get argument
    buff = sysArg->arg1;
    size = (int) (long)sysArg->arg2;
    unit = (int) (long)sysArg->arg3;

    // run real function
    result = termWriteReal(unit, size, buff, (int*) &(sysArg->arg2));
    sysArg->arg4 = (void*) (long) result;

    // if is Zapped, terminate
    if(isZapped())
        terminateReal(1);

    // change into User Mode
    USLOSS_PsrSet(USLOSS_PsrGet() &0b1111110);
}   // termWrite

/*
 *  start3 is the start function for the phase4.c it will initialize the
 *  clock driver, disk driver, and terminal driver. Then it will spawn
 *  the start4 function. The start3 is in kernel mode.
 */
void start3(void){
    char        termbuf[10];
    int     clockPID, diskPID[USLOSS_DISK_UNITS], termPID[4][3];
    int     status;
     CHECK_KERNEL_MODE

     //initialize the process table
    for(int i=0;i<MAXPROC; i++){
        procTable[i].pID=-1;
        procTable[i].mailboxID=-1;
        procTable[i].wakeUptime=-1;
    }

    // initialize the syscall vector
    systemCallVec[SYS_SLEEP]= (void*)sleep;
    systemCallVec[SYS_DISKREAD]=(void*)diskRead;
    systemCallVec[SYS_DISKWRITE]=(void*)diskWrite;
    systemCallVec[SYS_DISKSIZE]= (void*)diskSize;
    systemCallVec[SYS_TERMREAD]=(void*)termRead;
    systemCallVec[SYS_TERMWRITE]=(void*)termWrite;

    // Create clock device driver 
    semRunning = semcreateReal(0);
    clockPID = fork1("Clock driver", ClockDriver, NULL, USLOSS_MIN_STACK, 2);
    if (clockPID < 0) {
    USLOSS_Console("start3(): Can't create clock driver\n");
    USLOSS_Halt(1);
    }
    sempReal(semRunning);

    // Create the disk device drivers here. 
    for (int i = 0; i < USLOSS_DISK_UNITS; i++) {
        sprintf(termbuf, "%d", i);
        diskMBox[i] = MboxCreate(MAXSLOTS, 80);
        diskPID[i] = fork1("Disk driver", DiskDriver, termbuf, USLOSS_MIN_STACK, 2);
        if (diskPID[i] < 0) {
            USLOSS_Console("start3(): Can't create disk driver %d\n", i);
            USLOSS_Halt(1);
        }
        sempReal(semRunning);
    }
    // Create terminal device drivers.
    for (int i = 0; i < USLOSS_TERM_UNITS; i++) {
        m[i]=MboxCreate(10,10);
        MboxSend(m[i],"",0);
        sprintf(termbuf, "%d", i);

        termPID[i][0] = fork1("Term Driver", TermDriver, termbuf, USLOSS_MIN_STACK, 2);
        sempReal(semRunning);
        if (termPID[i][0]< 0) {
            USLOSS_Console("start3(): Can't create term driver %d\n", i);
            USLOSS_Halt(1);
        }
        termMBox[i][0] = MboxCreate(0,0);

        termPID[i][1] = fork1("Term Reader", TermReader, termbuf, USLOSS_MIN_STACK, 2);
        sempReal(semRunning);
        if (termPID[i][1] < 0) {
            USLOSS_Console("start3(): Can't create term driver %d\n", i);
            USLOSS_Halt(1);
        }
        termMBox[i][1] = MboxCreate(MAXSLOTS,100);
        termMBox[i][3] = MboxCreate(MAXSLOTS,100);

        termPID[i][2] = fork1("Term Writer", TermWriter, termbuf, USLOSS_MIN_STACK, 2);
        sempReal(semRunning);
        if (termPID[i][2] < 0) {
            USLOSS_Console("start3(): Can't create term driver %d\n", i);
            USLOSS_Halt(1);
        }
        termMBox[i][2] = MboxCreate(MAXSLOTS,100);
        termMBox[i][4] = MboxCreate(MAXSLOTS,100);
    }

    /*
     * Create first user-level process and wait for it to finish.
     * These are lower-case because they are not system calls;
     * system calls cannot be invoked from kernel mode.
     * I'm assuming kernel-mode versions of the system calls
     * with lower-case first letters.
     */
    fork1("start4", start4, NULL, 4 * USLOSS_MIN_STACK, 3);
    waitReal(&status);

    // Zap the device drivers
    zap(clockPID);  // clock driver
    for(int i=0; i<USLOSS_DISK_UNITS; i++){
        MboxSend(diskMBox[i], "", 0);
        zap(diskPID[i]);
    }

    for(int i=0;i<USLOSS_TERM_UNITS;i++){
        MboxSend(termMBox[i][0], "", 0);
        zap(termPID[i][0]);
        MboxSend(termMBox[i][1], "", 0);
        zap(termPID[i][1]);
        MboxSend(termMBox[i][2], "", 0);
        zap(termPID[i][2]);
    }
    quit(0);
}   //  start3

/*
 *  ClockDriver is the process of the clock driver and will answer the time
 *  and do the sleep for the process.
 */
static int ClockDriver(char *arg){
    int result, status, current, mailbox;
    procStruct* tmp;

    //enable interrupts.
    USLOSS_PsrSet(USLOSS_PsrGet() | 0x2);

    semvReal(semRunning);

    // Infinite loop until we are zap'd
    while(! isZapped()) {
        result = waitDevice(USLOSS_CLOCK_DEV, 0, &status);
        if (result != 0) {
            return 0;
        }
        /*
         * Compute the current time and wake up any processes
         * whose time has come.
         */
        gettimeofdayReal(&current);
        while(sleep_queue!=NULL && current >= sleep_queue->wakeUptime){
            mailbox=sleep_queue->mailboxID;
            sleep_queue->wakeUptime=0;
            tmp=sleep_queue;
            sleep_queue=sleep_queue->nextSleepProc;
            tmp->nextSleepProc=NULL;
            MboxSend(mailbox,"",0);
        }
    }
    return 0;
} // ClockDriver

/*
    *   DiskDriver is the process of the disk driver,
    *   it will do the writing and reading for the 
    *   disk driver.
 */
static int DiskDriver(char *arg){
    int unit, status, result, track,first;
    void* buff;
    procStruct* proc, *tmp;
    track=-1;
    
    USLOSS_DeviceRequest request;
    unit = atoi( (char *) arg);   

    //enable interrupts.
    USLOSS_PsrSet(USLOSS_PsrGet() | 0x2);


    // prepare for diskSize
    disk_queue[unit]=NULL;
    request.opr = USLOSS_DISK_TRACKS;
    request.reg1=&(trackSz[unit]);
    USLOSS_DeviceOutput(USLOSS_DISK_DEV, unit, &request);
    result = waitDevice(USLOSS_DISK_DEV, unit, &status);
    if (result != 0) {
        return 0;
    }
    semvReal(semRunning);

   // Infinite loop until we are zap'd
    while(! isZapped()) {
        // blocked by self mailbox
        MboxReceive(diskMBox[unit], "", 1);
        if(isZapped()){
            break;
        }
        //  get the request
        proc = popDiskQueue(unit, track);
        if(proc==NULL){
            proc=disk_queue[unit];
            disk_queue[unit]= proc->nextDiskProc[unit];
            proc->nextDiskProc[unit] = NULL;
        }
        else{
            tmp=proc;
            proc=tmp->nextDiskProc[unit];
            tmp->nextDiskProc[unit] = proc->nextDiskProc[unit];
            proc->nextDiskProc[unit]=NULL;
        }
        //  do the read or write
        buff=proc->diskBuffer;
        first=proc->first;
        track=proc->track;
        request.opr=USLOSS_DISK_SEEK;
        request.reg1=(void*) (long) track;
        USLOSS_DeviceOutput(USLOSS_DISK_DEV, unit, &request);
        result = waitDevice(USLOSS_DISK_DEV, unit, &status);
            if (result != 0) {
                return 0;
            }
        if(proc->target==DRIVER_READ)
            request.opr=USLOSS_DISK_READ;
        else request.opr = USLOSS_DISK_WRITE;
        request.reg1= (void*) (long) first;
        request.reg2 = buff;
        for(int i=0;i < proc->sectors;i++){
            USLOSS_DeviceOutput(USLOSS_DISK_DEV, unit, &request);
            result = waitDevice(USLOSS_DISK_DEV, unit, &status);
            if (result != 0) {
                return 0;
            }
            first++;
            buff+=512;
            request.reg2= (void*) (long) buff;
            if(first<16){
                request.reg1= (void*) (long) first;
            }
            else{
                first=0;
                track=(track+1)%trackSz[unit];        
                request.opr=USLOSS_DISK_SEEK;
                request.reg1=(void*) (long) track;
                USLOSS_DeviceOutput(USLOSS_DISK_DEV, unit, &request);
                result = waitDevice(USLOSS_DISK_DEV, unit, &status);
                if (result != 0) {
                    return 0;
                }
                if(proc->target==DRIVER_READ)
                    request.opr=USLOSS_DISK_READ;
                else request.opr = USLOSS_DISK_WRITE;
                request.reg1= (void*) (long) first;
                request.reg2 = buff;
            }
        }

        // wake up real function
        MboxSend(proc->mailboxID,"", 0);

    }
    return 0;
} // DiskDriver

/*
    *   termDriver is the process of terminal driver.
    *   it will do the reading and writing for the terminal driver.
    *   the process is running until the start4 quits.
 */
static int TermDriver(char* arg){
    int unit, status, write, read;
    char chr;
    unit = atoi( (char *) arg);   // Unit is passed as arg.

    //enable interrupts.
    USLOSS_PsrSet(USLOSS_PsrGet() | 0x2);

    semvReal(semRunning);
    while(!isZapped()){
        MboxReceive(termMBox[unit][0], "",0);
        if(isZapped())
            return 0;
        waitDevice(USLOSS_TERM_DEV, unit, &status);
        write=MboxCondReceive(termWriting[unit], "",0);
        read= status & 0b11;
        chr=(char) (status>>8);
        if(read==1){
            MboxSend(termMBox[unit][3], (void*) &chr, 1);
          }
        if(write == 0){
            MboxSend(termMBox[unit][4], "", 0);
        }
        MboxSend(m[unit],"",0);

    }
    return 0;
} // TermDriver

/*
    *   TermWriter is the process to do the terminal writing, it will
    *   wake up the termdriver, when it get the writing request
 */
static int TermWriter(char* arg){
    int unit, num, status;
    char chr;
    unit = atoi( (char *) arg);   // Unit is passed as arg.
    termWriting[unit]= MboxCreate(10,100);
    //enable interrupts.
    USLOSS_PsrSet(USLOSS_PsrGet() | 0x2);
    termNum[unit]=-1;
    semvReal(semRunning);

   // Infinite loop until we are zap'd
    while(! isZapped()) {
        int pid;
       MboxReceive(termMBox[unit][2], (void*) &pid, 80);
        if(isZapped())
            return 0;
       termSz[unit]=procTable[pid%MAXPROC].size;
       termBuff[unit]=(char*) procTable[pid%MAXPROC].diskBuffer;
       termMb[unit]= procTable[pid%MAXPROC].mailboxID;
        termNum[unit]=0;
       // if counter > size, wake up termDriver
       while(termNum[unit]<termSz[unit] && termNum[unit]<MAXLINE){
            MboxReceive(m[unit],"",0);
            MboxSend(termMBox[unit][0], "", 0);
            MboxSend(termWriting[unit], "", 0);

            //here
            if(MboxCondReceive(termMBox[unit][4],"", 40)==0){
                    termNum[unit]++;
                    if(   *(termBuff[unit]+termNum[unit]-1)=='\n' || termNum[unit]>=termSz[unit]){
                        USLOSS_DeviceOutput(USLOSS_TERM_DEV,unit, (void*)(long)0x2);
                        MboxReceive(termMBox[unit][4],"", 40);
                        break;
                    }
            }

            chr=*(termBuff[unit]+termNum[unit]);
            if(termNum[unit]==MAXLINE-1|| chr==0)
                chr='\n';
            status=(((int) chr) <<8) + 0x5;
           USLOSS_DeviceOutput(USLOSS_TERM_DEV,unit, (void*)(long)status);


           //conditional receive
           MboxReceive(termMBox[unit][4],"", 40);
            termNum[unit]++;
            if(chr=='\n')
                break;
        }
       // done
        if(termNum[unit]>=0){
            num=termNum[unit];
            termNum[unit]=-1;
           MboxSend(termMb[unit], (void*) &num, 4  );
       }
   }
   return 0;
}   //  TermWriter

/*
    * TermReader is the driver for reading the output from the terminal
    * there are 4 terminal in the hardware
*/
static int TermReader(char* arg){
    int unit, size, pid,line=0,num=0, index=0, counter=0, mbox, sign, status;
    char* buff, chr, data[11][20], read[20],chr2;
    unit = atoi( (char *) arg);   // Unit is passed as arg.
    mbox=MboxCreate(100,100);
    //enable interrupts.
    USLOSS_PsrSet(USLOSS_PsrGet() | 0x2);

    semvReal(semRunning);
    // Infinite loop until we are zap'd
    while(! isZapped()) {
        pid=0;
        MboxReceive(termMBox[unit][1], (void*) &pid, 80);
        if(isZapped())
            return 0;
        size=procTable[pid%MAXPROC].size;
        buff=(char*) procTable[pid%MAXPROC].diskBuffer;

        int i=0;
        if(counter>0){
            MboxReceive(mbox, (void*) &read,20);
            counter--;
            i=0;
            while( *(read+i)!='\n' && size>i){
                *(buff+i) = *(read+i);
                i++;
            }
            if(i<size){
                *(buff+i) = '\n';
                i++;
            }
            num=i;
        }

        else{
            sign=1;
            if(index>0){
                i=0;
                while( data[line][i]!='\n' && index>i) {
                    *(buff+i) = *(read+i);
                    i++;
                    if(i==size){
                        num=i;
                        sign=0;
                        break;
                    }
                }
                index=0;
            }

            // conditional receive
            while(sign && MboxCondReceive(termMBox[unit][3], (void*) &chr, 1)>=0){
               if(i<MAXLINE-1 && chr!='\n'){
                   *(buff+i) = chr;
                   i++;
                   if(size==i){
                       num=i;
                       sign=0;
                   }
               }
               else{
                   *(buff+i) = '\n';
                   i++;
                   sign=0;
                   num=i;
               }
            }
            while(sign){
                MboxReceive(m[unit],"",0);
                MboxSend(termMBox[unit][0], "",0);

                while(sign && MboxCondReceive(termMBox[unit][3], (void*) &chr, 1)>=0){
                    if(i<MAXLINE-1 && chr!='\n'){
                        *(buff+i) = chr;
                        i++;
                        if(size==i){
                            num=i;
                            sign=0;
                        }
                    }
                    else{
                        *(buff+i) = '\n';
                        i++;
                        sign=0;
                        num=i;
                    }
                }
                status=0x2;

                // here
                if(termNum[unit]>=0 ){
                    chr2=*(termBuff[unit]+termNum[unit]);
                    if(termNum[unit] == MAXLINE-1 || chr2==0 )
                        chr2='\n';
                    status=0x7+(chr2<<8);
                    MboxSend(termWriting[unit], "", 0);
                }
                b=status;

                USLOSS_DeviceOutput(USLOSS_TERM_DEV, unit, (void*) (long)status);

                if(sign){
                    MboxReceive(termMBox[unit][3], (void*) &chr, 1);
                    if(i<MAXLINE-1 && chr!='\n'){
                        *(buff+i) = chr;
                        i++;
                        if(size==i){
                            num=i;
                            sign=0;
                        }
                    }
                    else{
                        *(buff+i) = '\n';
                        i++;
                        sign=0;
                        num=i;
                    }
                }
            }
        }
        // conditional receive
        MboxSend(procTable[pid%MAXPROC].mailboxID, (void*) &num, 4);

    }
    return 0;
}   //  TermReader
