/*
     *  Name: Francis Kim and Andrew Carlisle
     *  File:  libuser.c
     *
     *  Description:  This file contains the interface declarations
     *                to the OS kernel support package.
     *
 */

#include <phase1.h>
#include <phase2.h>
#include <phase3.h>
#include <libuser.h>
#include <usyscall.h>
#include <usloss.h>

#define CHECKMODE {    \
    if (USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE) { \
        USLOSS_Console("Trying to invoke syscall from kernel\n"); \
        USLOSS_Halt(1);  \
    }  \
}

/*
     *  Routine:  Sleep
     *
     *  Description: Sleep the process
     *
     *  Arguments:
     *              arg1: input, number of seconds to delay the process
     *              arg4: output, -1 if illegal values are given as input;      
     *                                    0 otherwise
 */
int Sleep(int second){
    systemArgs sysArg;

    CHECKMODE;

    sysArg.number = SYS_SLEEP;
    sysArg.arg1 = (void*) (long) second;

    USLOSS_Syscall(&sysArg);

    return (int) (long)sysArg.arg4;
} // Sleep

/*
    *   Routine: DiskRead
    *
    *              arg1: output status
    *              arg4: output, -1 if illegal values are given as input;      
    *                                    0 otherwise
    *
*/
int DiskRead(void* buff, int unit, int track, int first, int sectors, int* status){
    systemArgs sysArg;

    CHECKMODE;

    sysArg.number = SYS_DISKREAD;
    sysArg.arg1 = buff;
    sysArg.arg2 = (void*) (long) sectors;
    sysArg.arg3 = (void*) (long) track;
    sysArg.arg4 = (void*) (long) first;
    sysArg.arg5 = (void*) (long) unit;
    USLOSS_Syscall(&sysArg);

    *status = (int) (long) sysArg.arg1;
    return (int) (long) sysArg.arg4;
}   // DiskRead

/*
    *   Routine: DiskWrite
    *
    *              arg1: output status
    *              arg4: output, -1 if illegal values are given as input;      
    *                                    0 otherwise
*/
int DiskWrite(void* buff, int unit, int track, int first, int sectors, int* status){
    systemArgs sysArg;

    CHECKMODE;

    sysArg.number = SYS_DISKWRITE;
    sysArg.arg1 = buff;
    sysArg.arg2 = (void*) (long) sectors;
    sysArg.arg3 = (void*) (long) track;
    sysArg.arg4 = (void*) (long) first;
    sysArg.arg5 = (void*) (long) unit;

    USLOSS_Syscall(&sysArg);

    *status = (int) (long) sysArg.arg1;

    return (int) (long) sysArg.arg4;
}   //  DiskWrite

/*
    *   Routine: DiskSize
    *
    *              arg1: output status
    *              arg4: output, -1 if illegal values are given as input;      
    *                                    0 otherwise
    *
*/
int DiskSize(int unit, int* sectorSize, int* trackSize, int* diskSize ){
    systemArgs sysArg;

    CHECKMODE;

    sysArg.number = SYS_DISKSIZE;
    sysArg.arg1 = (void*) (long)unit;
    sysArg.arg2 = (void*) sectorSize;
    sysArg.arg3 = (void*) trackSize;
    sysArg.arg4 = (void*) diskSize;

    USLOSS_Syscall(&sysArg);

    return (int) (long) sysArg.arg4;
}   //  DiskSize

/*
    *   Routine: TermRead
    *
    *              arg1: output status
    *              arg4: output, -1 if illegal values are given as input;      
    *                                    0 otherwise
    *
*/
int TermRead(char* buff, int size, int unit, int* len ){
    systemArgs sysArg;

    CHECKMODE;

    sysArg.number = SYS_TERMREAD;
    sysArg.arg1 = buff;
    sysArg.arg2 = (void*) (long) size;
    sysArg.arg3 = (void*) (long) unit;

    USLOSS_Syscall(&sysArg);

    *len = (int) (long) sysArg.arg2;
    return (int) (long) sysArg.arg4;
}   //  TermRead

/*
    *   Routine: TermWrite
    *
    *              arg1: output status
    *              arg4: output, -1 if illegal values are given as input;      
    *                                    0 otherwise
    *
*/
int TermWrite(char* buff, int size, int unit, int* len ){
    systemArgs sysArg;

    CHECKMODE;

    sysArg.number = SYS_TERMWRITE;
    sysArg.arg1 = buff;
    sysArg.arg2 = (void*) (long) size;
    sysArg.arg3 = (void*) (long) unit;

    USLOSS_Syscall(&sysArg);

    *len = (int) (long) sysArg.arg2;
    return (int) (long) sysArg.arg4;
}   //  TermWrite


/* end libuser.c */
