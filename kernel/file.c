/********************************************
*    file.c 	//add by visual 2016.5.17
*Ŀǰ��������ļ���д
***********************************************/

#include "type.h"
#include "const.h"
#include "protect.h"
#include "proto.h"
#include "string.h"
#include "proc.h"
#include "global.h"


#define	BaseOfEchoFilePhyAddr	(K_PHY2LIN(0x7e00))   //Ŀǰ����һ���ļ�
static u32 position=0;

/*****************************************************
*				open 		//add by visual 2016.5.17
*Ŀǰû��ʲô�õ�open
******************************************************/
PUBLIC u32 open(char* path,char* mode)
{
	position = 0;
	return 0;
}


/******************************************************
*				read		//add by visual 2016.5.17
********************************************************/
PUBLIC u32 read(u32 fd,void* buffer,u32 size)
{
	u32 addr_lin = BaseOfEchoFilePhyAddr + position;
	position += size;
	memcpy(buffer,(void*)addr_lin,size);
	return 0;
}

/******************************************************
*				seek //add by visual 2016.5.17
*******************************************************/
PUBLIC u32 seek(u32 pos)
{
	position = pos;
	return 0;
}

