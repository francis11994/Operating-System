 
#include "usloss.h"
#include <phase5.h>
#define DEBUG 0
extern int debugflag;
void *vmRegion = NULL;

/*
	initializePageTable
	Initialize page table, it is called from initializeProcTable method
*/
void initializePageTable(Process *id){
		id->pageTable=malloc(sizeof(PTE)*vmStats.pages);
                for(int i = 0;i<vmStats.pages;i++){
                        id->pageTable[i].state=-1;
                    id->pageTable[i].frame=-1;
                        id->pageTable[i].diskBlock=-1;
                }

}
/* 
	initializeProcTable(int pid)
	Initialize process table, and it is called from p1_fork method
*/
void initializeProcTable(int pid){
                Process* id = &ProcTable[pid%MAXPROC];
                id->pid = pid;
                id->mBox1=MboxCreate(1,0);
                id->mBox2=MboxCreate(1,0);
                id->mBox3 = MboxCreate(MAXPAGERS,0);
                id->mBox4 = MboxCreate(MAXPAGERS,0);
                MboxSend(id->mBox3 , "", 0);
                MboxSend(id->mBox4, "", 0);
        // Initialize page Table
		initializePageTable(id);

}

void p1_fork(int pid)
{
//	initializeProcTable(pid);
	if(vmRegion!=NULL){
		initializeProcTable(pid);
        }

    if (DEBUG && debugflag)
        USLOSS_Console("p1_fork() called: pid = %d\n", pid);
} /* p1_fork */

void
p1_switch(int old, int new)
{
    // increase switch if vmRegion is not null
    if(vmRegion!=NULL){
	vmStats.switches +=1;
    }
    if (DEBUG && debugflag)
        USLOSS_Console("p1_switch() called: old = %d, new = %d\n", old, new);
} /* p1_switch */

void
p1_quit(int pid)
{
    // Free page table when vmRegion is not null, and when p1_quit is called
    if(vmRegion!=NULL){
	Process *id = &ProcTable[pid%MAXPROC];
	free(id->pageTable);
    }
    if (DEBUG && debugflag)
        USLOSS_Console("p1_quit() called: pid = %d\n", pid);
} /* p1_quit */

