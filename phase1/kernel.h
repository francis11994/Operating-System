/* Patrick's DEBUG printing constant... */
#define DEBUG 1

// Constants for process statuses
#define READY 1
#define BLOCKED 2
#define QUIT 3
#define JOINBLOCKED 4
#define EMPTY 5
#define RUNNING 6
#define ZAP_BLOCKED 7


typedef struct procStruct procStruct;

typedef struct procStruct * procPtr;

struct procStruct {
   procPtr         nextProcPtr;
   procPtr         childProcPtr;
   procPtr         nextSiblingPtr;
   procPtr         quitChildList;
   procPtr         nextQuitSibling;
   procPtr         zapList;        // List of processes that zapped this one
   procPtr         nextZapProc;    // Link zapping processes together
   char            name[MAXNAME];     /* process's name */
   char            startArg[MAXARG];  /* args passed to process */
   USLOSS_Context  state;             /* current context for process */
   short           pid;               /* process id */
   short           parentPid;         /* parent process pid */
   int             priority;
   int (* startFunc) (char *);   /* function where process begins -- launch */
   char           *stack;
   unsigned int    stackSize;
   int             status;        /* READY, BLOCKED, QUIT, etc. */
   int             quitStatus;   /* Holds the status of a child when quit */
   int             startTime;    // Time the process started "RUNNING"
   int             totalTime;    // Total time the process has run so far
   int             isZapped;     // Tracks a process's zapped status
};

struct psrBits {
    unsigned int curMode:1;
    unsigned int curIntEnable:1;
    unsigned int prevMode:1;
    unsigned int prevIntEnable:1;
    unsigned int unused:28;
};

union psrValues {
   struct psrBits bits;
   unsigned int integerPart;
};

/* Some useful constants.  Add more as needed... */
#define NO_CURRENT_PROCESS NULL
#define MINPRIORITY 5
#define MAXPRIORITY 1
#define SENTINELPID 1
#define SENTINELPRIORITY (MINPRIORITY + 1)