#ifndef _KERNEL_H
#define _KERNEL_H
#include <stdlib.h>
#include <assert.h>
#define Max_Segment 10
//#define Max_Process 10
//#define Max_Queuesize 20//时间队列最大空间
#define Max_Memblock 8
#define Max_Diskblock 128

#define ERROR 65535

//段表项，在Process中，由段表项构成的数组即为段表
//段号即为段表下标

typedef struct WORD
{
	bool status;//0为空闲，1为busy
	int data;
} WORD;

extern WORD mem[Max_Memblock];
extern WORD disk[Max_Diskblock];

typedef struct segTableItem
{
	unsigned int length; // 段长
	unsigned int baseAddr;		 //段基址
	int accessMode;		 //存取方式，1表示只执行，2表示只读，3表示允许读/写
	int  A;		         //访问字段，若使用FIFO，记录本段进入时间；若使用LRU，记录本段多长时间未访问
	bool M;              //修改位，0表示该段在内存中未被修改，1表示被修改
	bool P;			     //存在位，是否在内存中，0表示不在，1表示在
	unsigned int diskAddr;      //外存始址
} segTableItem;

typedef struct memSegmentList
{
	int PID;
	int SID;
	segTableItem* segItem;
	memSegmentList* next;

} memSegmentList;

extern memSegmentList* segMemList;//FIFO、LRU用到，把调入到内存的段连接起来

typedef struct Process
{
	int ID;     //进程号
	char name[20]; //进程名
	segTableItem segtable[Max_Segment]; //段表
	int seg_num;   //进程包含的段数
	Process* next;
} Process;
extern Process* process;

typedef struct FreeChain
{
	unsigned int start;
	unsigned int length;
	FreeChain* next;
} FreeChain;

extern FreeChain* freeMem;// 管内存空闲链的
extern FreeChain* freeDisk;//管外存空闲链的


//初始化
extern void initMemory();//初始化内存
extern void allocateDiskBlock(unsigned int start, unsigned int length);//文件初始化分配磁盘块
extern void initFreeChain(FreeChain** freechain, unsigned int size);//初始化空闲链
extern void initSegMemList(memSegmentList** segMemList);//初始化调入内存的段的链
extern void initProcess(Process** process);
extern void releaseFreeChain(FreeChain** freechain);

//存储空间管理
extern void insertSegment(memSegmentList* segMemList, int PID, int SID, segTableItem* segItem);
extern void compactChain(memSegmentList* segMemList, WORD* mem);
extern int replace();
extern int dispatchSegment(FreeChain* freeMem, Process* process, unsigned index, memSegmentList* segMemList,bool mode);
extern unsigned int allocateFreeBlock(FreeChain* freechain, unsigned int len, WORD* memory); //分配空闲空间
extern void Recycle(FreeChain* freechain, WORD* mem, WORD* disk, unsigned int baseAddr, unsigned int diskAddr, unsigned int offset, bool M);//回收内存
extern void mergeFreeChain(FreeChain* freechain, unsigned int start, unsigned int offset);

//进程部分
extern Process* createPcb(char name[20], int segcount);//创建进程控制块
extern char* strcpy(char* dst, const char* src);//复制字符串
extern void releaseProcess(Process* process);

//用户接口
extern void readFile(char* filePath);
extern void writeFile(char* filePath, int PID);
extern int createProcess(char name[20], int segcount, unsigned int len[]);//创建进程，输入进程名、进程段数、各段长度
extern Process* searchProcess(int PID);
extern void executeProcess(int PID, int SID, int w);
extern void deleteProcess(int PID);

#endif
