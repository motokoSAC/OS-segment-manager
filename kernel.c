#include "kernel.h"
#include <stdio.h>

WORD mem[Max_Memblock];
WORD disk[Max_Diskblock];
memSegmentList* segMemList;
Process* process;
FreeChain* freeMem;
FreeChain* freeDisk;

//初始化存储单元
void initMemory()
{
	//可以初始化一些os内核的内存区域
	unsigned int i;
	for (i = 0; i < Max_Memblock; i++)
		mem[i].status = 0;

	for (i = 0; i < Max_Diskblock; i++)
		disk[i].status = 0;
}
///////////////////////////////////////////////////////////
//初始化空闲链 可合并
void releaseFreeChain(FreeChain** freechain)
{
	if (*freechain == NULL)
		return;

	FreeChain* p, * phead = (*freechain)->next;
	while (phead != NULL) {
		p = phead;
		phead = phead->next;
		free(p);
	}
	(*freechain)->next = NULL;
}

void initFreeChain(FreeChain** freechain, unsigned int size)
{
	FreeChain* block = (FreeChain*)malloc(sizeof(FreeChain));
	releaseFreeChain(freechain);
	*freechain = (FreeChain*)malloc(sizeof(FreeChain));
	block->start = 0;
	block->length = size;
	block->next = NULL;
	(*freechain)->next = block;
}

void initSegMemList(memSegmentList** segMemList)
{
	*segMemList = (memSegmentList*)malloc(sizeof(memSegmentList));
	(*segMemList)->next = NULL;
}

//新调段进入内存后，将该段信息添加到内存段链表
void insertSegment(memSegmentList* segMemList, int PID, int SID, segTableItem* segItem)
{
	memSegmentList* seg = (memSegmentList*)malloc(sizeof(memSegmentList));
	seg->PID = PID;
	seg->SID = SID;
	seg->segItem = segItem;
	seg->next = NULL;
	//插到表尾
	while (segMemList->next != NULL)
		segMemList = segMemList->next;
	segMemList->next = seg;

}

void initProcess(Process** process)
{
	*process = (Process*)malloc(sizeof(Process));
	(*process)->next = NULL;
}

Process* createPcb(char name[20], int segcount)
{
	Process* pcb = (Process*)malloc(sizeof(Process)), * p = process;
	//如果是第一个进程
	if (process->next == NULL) {
		pcb->ID = 0;
		strcpy(pcb->name, name);
		pcb->seg_num = segcount;
		for (int i = 0; i < segcount; i++) {
			pcb->segtable[i].baseAddr = ERROR;
			pcb->segtable[i].diskAddr = ERROR;
			pcb->segtable[i].P = false;
		}
		pcb->next = NULL;
		process->next = pcb;
	} else {
		//如果不是第一个进程
		while (p->next != NULL)
			p = p->next;
		pcb->ID = p->ID + 1;
		pcb->seg_num = segcount;
		strcpy(pcb->name, name);
		for (int i = 0; i < segcount; i++) {
			pcb->segtable[i].baseAddr = ERROR;
			pcb->segtable[i].diskAddr = ERROR;
			pcb->segtable[i].P = false;
		}
		pcb->next = NULL;
		p->next = pcb;
	}
	return pcb;
}

unsigned int allocateFreeBlock(FreeChain* freechain, unsigned int len, WORD* memory)
{
	unsigned int i;
	unsigned int addr;
	while (freechain->next != NULL) {
		if (freechain->next->length == len) {
			FreeChain* temp = freechain->next;
			//将此freeSpace移出空闲块
			addr = freechain->next->start;
			for (i = freechain->next->start; i < freechain->next->start + len; i++)
				memory[i].status = 1;
			freechain->next = temp->next;
			free(temp);
			return addr;
		} else if (freechain->next->length > len) {
			//修改此空闲块的首地址和长度
			addr = freechain->next->start;
			for (i = freechain->next->start; i < freechain->next->start + len; i++)
				memory[i].status = 1;
			freechain->next->start = freechain->next->start + len;
			freechain->next->length = freechain->next->length - len;
			return addr;
		}
		freechain = freechain->next;
	}
	return ERROR;
}

//紧凑算法,将内存中的占用块和空闲块重新排列，修改空闲链表内容、
void compactChain(memSegmentList* segMemList, WORD* mem)
{
	unsigned int sumLength = 0;//计算占用块总长度;
	unsigned int i;
	//unsigned int minBaseAddr = ERROR;//直接赋值为最大值
	memSegmentList* ptr = segMemList->next;//指向头结点后的第一个结点
	while (ptr != NULL) {
		ptr->segItem->baseAddr = sumLength;//修改内存段链表，把当前的段的内存起始地址置为sumLength，初始为0，默认从内存起始处开始放
		process[ptr->PID].segtable[ptr->SID].baseAddr = sumLength;//同时需要修改进程段表的内存起始地址
		sumLength = sumLength + ptr->segItem->length;//sumLength+=当前段的长度
		ptr = ptr->next;
	}

	releaseFreeChain(&freeMem);//操作空闲链表，将其删到除头结点外只剩一个结点
	FreeChain* freeBlock = (FreeChain*)malloc(sizeof(FreeChain));
	freeBlock->start = sumLength;
	freeBlock->length = Max_Memblock - sumLength;
	freeBlock->next = NULL;
	freeMem->next = freeBlock;
	//内存单元操作
	for (i = 0; i < sumLength; i++)
		mem[i].status = true;
	for (; i < Max_Memblock; i++)
		mem[i].status = false;

}

int replace()
{
	int len = segMemList->next->segItem->length;
	//回收内存
	memSegmentList* temp = segMemList->next;
	segMemList->next = temp->next;
	Recycle(freeMem, mem, disk, temp->segItem->baseAddr, temp->segItem->diskAddr, temp->segItem->length, temp->segItem->M);
	Process* p = process->next;
	while (p != NULL) {
		if (p->ID == temp->PID)
			break;
		p = p->next;
	}
	p->segtable[temp->SID].baseAddr = ERROR;
	p->segtable[temp->SID].P = false;
	free(temp);
	return len;
}

memSegmentList* deleteSegment(memSegmentList* segMemList, FreeChain* freechain, int PID, int SID)
{
	if (segMemList->next == NULL)
		return NULL;

	memSegmentList* temp, * pre = segMemList;
	while (pre != NULL && pre->next != NULL) {
		if (pre->next->PID == PID && pre->next->SID == SID) {
			temp = pre->next;
			pre->next = temp->next;
			free(temp);
			return segMemList;
		}
		pre = pre->next;
	}
}

int dispatchSegment(FreeChain* freeMem, Process* process, unsigned index, memSegmentList* segMemList,bool mode) //用户输入的第一段对应第0段
{
	unsigned int freeAreaLen = 0;
	FreeChain* ptr = freeMem->next;
	//首次适应换入段
	if (index > process->seg_num - 1) {
		//InterruptType = 1;//越界中断
		return 1;
	}
	if (process->segtable[index].baseAddr != ERROR) {//process->segtable[index].P = true
		//InterruptType = 0;//已在内存中
		if (!mode) {
			;
		} else {
			memSegmentList* pre = segMemList;
			memSegmentList* cur;
			memSegmentList* temp = NULL;
			while (pre->next != NULL) {
				cur = pre->next;
				if (cur->PID == process->ID && cur->SID == index) {
					if (segMemList->next == cur) {
						segMemList->next = cur->next;
						temp = cur;
						temp->next = NULL;
					} 
					else if(cur->next==NULL) {
						;
					} else {
						temp = cur;
						pre->next = cur->next;
						temp->next = NULL;
					}

				}
				pre = pre->next;
			}
			pre->next = temp;
		}
		return 0;
	}
	unsigned int baseAddr = allocateFreeBlock(freeMem, process->segtable[index].length, mem);
	if (baseAddr != ERROR) {
		//InterruptType = 2;//缺段中断
		process->segtable[index].baseAddr = baseAddr;
		//process->segtable[index].P = true
		insertSegment(segMemList, process->ID, index, &process->segtable[index]);
		return 2;
	} else {
		while (ptr != NULL) {
			freeAreaLen += ptr->length;
			ptr = ptr->next;
		}

		if (process->segtable[index].length <= freeAreaLen) { //空闲区合并后可容纳下
			//调用紧凑函数
			compactChain(segMemList, mem);
			//InterruptType = 2;//缺段中断
			//process->segtable[index].P = true
			baseAddr = allocateFreeBlock(freeMem, process->segtable[index].length, mem);
			if (baseAddr != ERROR) {
				process->segtable[index].baseAddr = baseAddr;
				insertSegment(segMemList, process->ID, index, &process->segtable[index]);
				return 2;
			}
		} else { //空闲区合并后不可容纳下，需要置换(限制用户创建进程时，段长不可超过内存，所以无需担心内存放不下)
			while (freeAreaLen < process->segtable[index].length)
				freeAreaLen = freeAreaLen + replace();
			//调用紧凑函数
			compactChain(segMemList, mem);
			//InterruptType = 3;//内存满，使用置换算法
			baseAddr = allocateFreeBlock(freeMem, process->segtable[index].length, mem);
			if (baseAddr != ERROR) {
				process->segtable[index].baseAddr = baseAddr;
				//process->segtable[index].P = true
				insertSegment(segMemList, process->ID, index, &process->segtable[index]);
				return 3;
			}
		}
	}
	return 4;
}

//回收内存单元，可以把baseAddr+offset区间内全部内存单元回收，并合并所有覆盖到的空闲块和相邻的空闲块
void Recycle(FreeChain* freechain, WORD* mem, WORD* disk, unsigned int baseAddr, unsigned int diskAddr, unsigned int offset, bool M)
{
	int i, j;
	//重复回收会出错
	if (M) {//内存中数据已被修改	
		for (i = baseAddr, j = diskAddr; i < baseAddr + offset; i++, j++) {//回写disk
			mem[i].status = false;
			disk[j].data = mem[i].data;
		}
		M = false;
	} else {
		for (i = baseAddr; i < baseAddr + offset; i++)
			mem[i].status = false;
	}

	mergeFreeChain(freechain, baseAddr, offset);
	//printf("当前释放的盘块起始地址为%d:\n", ptr->start);
	return;
}

void mergeFreeChain(FreeChain* freechain, unsigned int start, unsigned int offset)
{
	FreeChain* ptr = freechain->next;
	FreeChain* newBlock = (FreeChain*)malloc(sizeof(FreeChain));
	if (ptr == NULL) {
		newBlock->start = start;
		newBlock->length = offset;
		newBlock->next = ptr;
		freechain->next = newBlock;
	} else {
		if (start < ptr->start) {//baseAddr小于空闲链第一个结点的起始地址，此时baseAddr排在空闲链表最前面
			if (start + offset >= ptr->start) {//无需创建新结点，直接扩大该空闲块
				ptr->length = ptr->start + ptr->length - start;
				ptr->start = start;
			} else {
				newBlock->start = start;
				newBlock->length = offset;
				newBlock->next = ptr;
				freechain->next = newBlock;
			}
		}
		else//baseAddr大于等于第一个空闲链第一个结点的起始地址
		{
			while (ptr->start < start && ptr->next != NULL) {//遍历链表，找到第一个起始地址大于等于baseAddr的结点
			
				if (ptr->next->start >= start) 
					break;
				ptr = ptr->next;//这里最后总会执行一次，有可能会是最后一个结点
			}

			if (ptr->next != NULL) {//不是最后一个，现在baseAddr处在合适的两个结点的start之间（）
				if (start <= (ptr->start + ptr->length) && (start + offset) < ptr->next->start) {
					//与左边块相邻，直接扩大左边块
					if (start + offset > (ptr->start + ptr->length)) {//当前释放的块长度超过左邻块的右边界,但此时不会与下一个结点start相邻或覆盖
					
						ptr->length = start + offset - ptr->start;
					}
					else {
						ptr->length = ptr->start + ptr->length - start;
					}
				}
				else if (start > (ptr->start + ptr->length) && (start + offset) >= ptr->next->start) {
					//与右边块相邻,扩大右边块
					if (start + offset > (ptr->next->start + ptr->next->length)) {//当前释放的块长度超过右邻块的右边界

						//删去当前块长度能覆盖到的结点
						FreeChain* qtr = ptr->next;
						while (qtr->next != NULL && qtr->start + qtr->length < start + offset) {
							FreeChain* temp = qtr->next;
							qtr->next = temp->next;
							free(temp);
						}

						ptr->next->length = start + qtr->length;
					}
					else {
						ptr->next->length = ptr->next->start + ptr->next->length - start;
					}
					ptr->next->start = start;
				}
				else if (start <= (ptr->start + ptr->length) && (start + offset) >= ptr->next->start) {
					//与左右两个块同时相邻
					if (start + offset > (ptr->next->start + ptr->next->length)) {//当前释放的块长度超过右邻块的右边界
						FreeChain* qtr = ptr->next;
						while (qtr->next != NULL && qtr->start + qtr->length < start + offset)
						{
							FreeChain* temp = qtr->next;
							qtr->next = temp->next;
							free(temp);
						}
						ptr->length = ptr->start + qtr->length;
					}
					else {
						FreeChain* temp = ptr->next;
						ptr->length = ptr->next->start + ptr->next->length - ptr->start;
						ptr->next = temp->next;
						free(temp);
					}
				}
				else {
					newBlock->start = start;
					newBlock->length = offset;
					newBlock->next = ptr->next;
					ptr->next = newBlock;
				}

			}
			else { //此时baseAddr大于任何一个结点的起始地址，插到表尾
				if (start <= ptr->start + ptr->length) {//此时挨在一起
					ptr->length = start + offset - ptr->start;
				}
				else {
					newBlock->start = start;
					newBlock->length = offset;
					newBlock->next = ptr->next;
					ptr->next = newBlock;
				}

			}
		}
	}
}

void releaseProcess(Process* pcb)
{
	unsigned int i, j;
	memSegmentList* cur, * temp, * pre = segMemList;
	Process* cu, * pr = process;
	for (i = 0; i < pcb->seg_num; i++) { //外存释放
		for (j = pcb->segtable[i].diskAddr; j < pcb->segtable[i].diskAddr + pcb->segtable[i].length; j++)
			disk[j].status = false;
		mergeFreeChain(freeDisk, pcb->segtable[i].diskAddr, pcb->segtable[i].length);
	}
	while (pre->next != NULL) //内存释放
	{
		cur = pre->next;
		if (cur->PID == pcb->ID) {
			for (j = pcb->segtable[cur->SID].baseAddr; j < pcb->segtable[cur->SID].baseAddr + pcb->segtable[cur->SID].length; j++)
				mem[j].status = false;
			mergeFreeChain(freeMem, pcb->segtable[cur->SID].baseAddr, pcb->segtable[cur->SID].length);
			if (cur == segMemList->next) {
				segMemList->next = cur->next;
				free(cur);
			}
			else {
				pre->next = cur->next;
				free(cur);
			}
		}
		else
			pre = pre->next;
	}
	//从进程队列中删除
	while (pr->next != NULL) //内存释放
	{
		cu = pr->next;
		if (cu->ID == pcb->ID) {
			if (cu == process->next) {
				process->next = cu->next;
				free(cu);
			}
			else {
				pr->next = cu->next;
				free(cu);
			}
			break;
		}
		pr = pr->next;
	}

}

char* strcpy(char* dst, const char* src)   //[1]
{
	assert(dst != NULL && src != NULL);    //[2]
	char* ret = dst;  //[3]
	while ((*dst++ = *src++) != '\0'); //[4]
	return ret;
}

void allocateDiskBlock(unsigned int start, unsigned int length)
{
	FreeChain* p = freeDisk->next, * pre = freeDisk;
	//定位内存所在空闲块
	while (p != NULL) {
		if (p->start == start && p->start + p->length == start + length) { //空闲块全部占据
			if (p == freeDisk->next) {
				freeDisk->next = p->next;
				free(p);
			}
			else {
				pre->next = p->next;
				free(p);
			}
			break;
		} else if (p->start == start && p->start + p->length > start + length) { //从头占据但未满
			p->start = p->start + length;
			p->length = p->length - length;
			break;
		} else if (p->start < start && p->start + p->length == start + length) { //占据到尾部但未满
			p->length = p->length - length;
			break;
		} else if (p->start < start && p->start + p->length > start + length) { //占据中间部分
			FreeChain* temp = (FreeChain*)malloc(sizeof(FreeChain));
			temp->length = p->length - length - start + p->start;
			temp->start = start + length;
			temp->next = p->next;
			p->length = start - p->start;
			p->next = temp;
			break;
		}
		pre = pre->next;
		p = p->next;
	}
}


