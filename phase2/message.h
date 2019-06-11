
#define DEBUG2 1
#define EMPTY 0
#define RUNNING 1
#define WAIT 2
typedef struct mailSlot *slotPtr;
typedef struct mailSlot mailSlot;
typedef struct mailbox   mailbox;
typedef struct mboxProc *mboxProcPtr;

struct block{
	int pid;
	int mboxID;
	int status;
	int mem_size;
	int message[MAX_MESSAGE];
};

struct mailSlot {
    int	mboxID;
    int status;
    int mem_size;
    char message[MAX_MESSAGE];
   
};	
struct mailbox {
    int       mboxID;
    int	      pid;
    int       status;
    int       slots;
    int	      originalSlots;
    int       slot_size;
    int       release;
    struct block blockSendList[50];
    struct block blockRecieveList[50];
    // other items as needed...
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
