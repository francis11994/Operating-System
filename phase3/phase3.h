/*
 * These are the definitions for phase 3 of the project
 */

#ifndef _PHASE3_H
#define _PHASE3_H

#define MAXSEMS         200

#endif 

typedef struct procStruct {

  struct procStruct*         CPP; // Child Process Pointer
  char            startArg[MAXARG];  // argument that passed from the process 
  struct procStruct*         PPP; // Parent Process Pointer
  int             currentPid;               // process id 
  int             priority;
  struct procStruct*         NSP; // Next Sibling Pointer
  int (* startFunc) (char *);   // beginning of the process function 
  char           *stack;	// Stack
  int zapStatus;		// Represent for zap status
  unsigned int    stackSize;	// Size of stack
  int 	mailboxID;		// Mail box ID

} ProcS;
