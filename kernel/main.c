
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
                            main.c
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
                                                    Forrest Yu, 2005
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

#include "type.h"
#include "const.h"
#include "protect.h"
#include "proto.h"
#include "string.h"
#include "proc.h"
#include "global.h"

int safe ;

/*======================================================================*
                            kernel_main
 *======================================================================*/
PUBLIC int kernel_main()
{
	disp_str("-----\"kernel_main\" begins-----\n");
	init();//内存管理模块的初始化  add by liang 

	clear_kernel_pagepte_low();		//add by visual 2016.5.13
	
	TASK*		p_task		= task_table;
	PROCESS*	p_proc		= proc_table;
	u16		selector_ldt	= SELECTOR_LDT_FIRST;	
	char* p_regs;	//point to registers in the new kernel stack, added by xw, 17/12/11
	/*************************************************************************
	*进程初始化部分 	edit by visual 2016.5.4 
	***************************************************************************/
	int pid;
	u32 AddrLin,pte_addr_phy_temp,addr_phy_temp,err_temp;//edit by visual 2016.5.9
	
	for( pid=0 ; pid<NR_TASKS ; pid++ )
	{//1>对前NR_TASKS个PCB初始化,且状态为READY(生成的进程)
		/*************基本信息*********************************/
		strcpy(p_proc->task.p_name, p_task->name);		//名称
		p_proc->task.pid = pid;							//pid
		p_proc->task.stat = READY;  						//状态
		
		/**************LDT*********************************/
		p_proc->task.ldt_sel = selector_ldt;
		memcpy(&p_proc->task.ldts[0], &gdt[SELECTOR_KERNEL_CS >> 3],sizeof(DESCRIPTOR));
		p_proc->task.ldts[0].attr1 = DA_C | PRIVILEGE_TASK << 5;
		memcpy(&p_proc->task.ldts[1], &gdt[SELECTOR_KERNEL_DS >> 3],sizeof(DESCRIPTOR));
		p_proc->task.ldts[1].attr1 = DA_DRW | PRIVILEGE_TASK << 5;
		
		/**************寄存器初值**********************************/
		p_proc->task.regs.cs	= ((8 * 0) & SA_RPL_MASK & SA_TI_MASK)| SA_TIL | RPL_TASK;
		p_proc->task.regs.ds	= ((8 * 1) & SA_RPL_MASK & SA_TI_MASK)| SA_TIL | RPL_TASK;
		p_proc->task.regs.es	= ((8 * 1) & SA_RPL_MASK & SA_TI_MASK)| SA_TIL | RPL_TASK;
		p_proc->task.regs.fs	= ((8 * 1) & SA_RPL_MASK & SA_TI_MASK)| SA_TIL | RPL_TASK;
		p_proc->task.regs.ss	= ((8 * 1) & SA_RPL_MASK & SA_TI_MASK)| SA_TIL | RPL_TASK;
		p_proc->task.regs.gs	= (SELECTOR_KERNEL_GS & SA_RPL_MASK)| RPL_TASK;
		p_proc->task.regs.eflags = 0x1202; /* IF=1, IOPL=1 */
		//p_proc->task.cr3 在页表初始化中处理
		
		
		/**************线性地址布局初始化**********************************///	add by visual 2016.5.4
		/**************task的代码数据大小及位置暂时是不会用到的，所以没有初始化************************************/
		p_proc->task.memmap.heap_lin_base = HeapLinBase;
		p_proc->task.memmap.heap_lin_limit = HeapLinBase;				//堆的界限将会一直动态变化
		p_proc->task.memmap.stack_child_limit = StackLinLimitMAX;		//add by visual 2016.5.27
		p_proc->task.memmap.stack_lin_base = StackLinBase;
		p_proc->task.memmap.stack_lin_limit = StackLinBase - 0x4000;		//栈的界限将会一直动态变化，目前赋值为16k，这个值会根据esp的位置进行调整，目前初始化为16K大小
		p_proc->task.memmap.kernel_lin_base = KernelLinBase;
		p_proc->task.memmap.kernel_lin_limit = KernelLinBase + KernelSize;	//内核大小初始化为8M		//add  by visual 2016.5.10
		
		/***************初始化PID进程页表*****************************/
		if( 0 != init_page_pte(pid) )
		{
			disp_color_str("kernel_main Error:init_page_pte",0x74);
			return -1;
		}
		//	pde_addr_phy_temp = get_pde_phy_addr(pid);//获取该进程页目录物理地址			//delete by visual 2016.5.19
		
		/****************代码数据*****************************/
		p_proc->task.regs.eip= (u32)p_task->initial_eip;//进程入口线性地址		edit by visual 2016.5.4
		
		/****************栈（此时堆、栈已经区分，以后实验会重新规划堆的位置）*****************************/
		p_proc->task.regs.esp=(u32)StackLinBase;			//栈地址最高处	
		for( AddrLin=StackLinBase ; AddrLin>p_proc->task.memmap.stack_lin_limit ; AddrLin-=num_4K )
		{//栈
			//addr_phy_temp = (u32)test_kmalloc_4k();//为栈申请一个物理页,Task的栈是在内核里面	//delete by visual 2016.5.19
			//if( addr_phy_temp<0 || (addr_phy_temp&0x3FF)!=0  )
			//{
			//	disp_color_str("kernel_main Error:addr_phy_temp",0x74);
			//	return -1;
			//}
			err_temp = lin_mapping_phy(	AddrLin,//线性地址						//add by visual 2016.5.9
										MAX_UNSIGNED_INT,//物理地址					//edit by visual 2016.5.19
										pid,//进程pid							//edit by visual 2016.5.19
										PG_P  | PG_USU | PG_RWW,//页目录的属性位
										PG_P  | PG_USU | PG_RWW);//页表的属性位
			if( err_temp!=0 )
			{
				disp_color_str("kernel_main Error:lin_mapping_phy",0x74);
				return -1;
			}
								
		}
		
		/***************copy registers data****************************/
		//copy registers data to the bottom of the new kernel stack
		//added by xw, 17/12/11
		p_regs = (char*)(p_proc + 1);
		p_regs -= P_STACKTOP;
		memcpy(p_regs, (char*)p_proc, 18 * 4);
		
		p_proc->task.esp_save_int = p_regs; //initialize esp_save_int, added by xw, 17/12/11
		
		/***************变量调整****************************/
		p_proc++;
		p_task++;
		selector_ldt += 1 << 3;
	}
	for( 	; pid<NR_K_PCBS ; pid++ )
	{//2>对中NR_TASKS~NR_K_PCBS的PCB表初始化,状态为IDLE,没有初始化esp(并没有生成,所以没有代码入口,只是留位置)
		/*************基本信息*********************************/
		strcpy(p_proc->task.p_name, "Task");				//名称
		p_proc->task.pid = pid;							//pid
		p_proc->task.stat = IDLE;  						//状态
		
		/**************LDT*********************************/
		p_proc->task.ldt_sel = selector_ldt;
		memcpy(&p_proc->task.ldts[0], &gdt[SELECTOR_KERNEL_CS >> 3],sizeof(DESCRIPTOR));
		p_proc->task.ldts[0].attr1 = DA_C | PRIVILEGE_TASK << 5;
		memcpy(&p_proc->task.ldts[1], &gdt[SELECTOR_KERNEL_DS >> 3],sizeof(DESCRIPTOR));
		p_proc->task.ldts[1].attr1 = DA_DRW | PRIVILEGE_TASK << 5;
		
		/**************寄存器初值**********************************/
		p_proc->task.regs.cs	= ((8 * 0) & SA_RPL_MASK & SA_TI_MASK)| SA_TIL | RPL_TASK;
		p_proc->task.regs.ds	= ((8 * 1) & SA_RPL_MASK & SA_TI_MASK)| SA_TIL | RPL_TASK;
		p_proc->task.regs.es	= ((8 * 1) & SA_RPL_MASK & SA_TI_MASK)| SA_TIL | RPL_TASK;
		p_proc->task.regs.fs	= ((8 * 1) & SA_RPL_MASK & SA_TI_MASK)| SA_TIL | RPL_TASK;
		p_proc->task.regs.ss	= ((8 * 1) & SA_RPL_MASK & SA_TI_MASK)| SA_TIL | RPL_TASK;
		p_proc->task.regs.gs	= (SELECTOR_KERNEL_GS & SA_RPL_MASK)| RPL_TASK;
		p_proc->task.regs.eflags = 0x1202; /* IF=1, IOPL=1 */		
		
		/****************页表、代码数据、堆栈*****************************/
		//无
		
		/***************copy registers data****************************/
		//copy registers data to the bottom of the new kernel stack
		//added by xw, 17/12/11
		p_regs = (char*)(p_proc + 1);
		p_regs -= P_STACKTOP;
		memcpy(p_regs, (char*)p_proc, 18 * 4);
		
		p_proc->task.esp_save_int = p_regs; //initialize esp_save_int, added by xw, 17/12/11

		/***************变量调整****************************/
		p_proc++;
		selector_ldt += 1 << 3;
	}
	for( ; pid<NR_K_PCBS+1 ; pid++ )
	{//initial 进程的初始化				//add by visual 2016.5.17
		/*************基本信息*********************************/
		strcpy(p_proc->task.p_name,"initial");			//名称
		p_proc->task.pid = pid;							//pid
		p_proc->task.stat = READY;  						//状态
		
		/**************LDT*********************************/
		p_proc->task.ldt_sel = selector_ldt;
		memcpy(&p_proc->task.ldts[0], &gdt[SELECTOR_KERNEL_CS >> 3],sizeof(DESCRIPTOR));
		p_proc->task.ldts[0].attr1 = DA_C | PRIVILEGE_TASK << 5;
		memcpy(&p_proc->task.ldts[1], &gdt[SELECTOR_KERNEL_DS >> 3],sizeof(DESCRIPTOR));
		p_proc->task.ldts[1].attr1 = DA_DRW | PRIVILEGE_TASK << 5;
		
		/**************寄存器初值**********************************/
		p_proc->task.regs.cs	= ((8 * 0) & SA_RPL_MASK & SA_TI_MASK)| SA_TIL | RPL_TASK;
		p_proc->task.regs.ds	= ((8 * 1) & SA_RPL_MASK & SA_TI_MASK)| SA_TIL | RPL_TASK;
		p_proc->task.regs.es	= ((8 * 1) & SA_RPL_MASK & SA_TI_MASK)| SA_TIL | RPL_TASK;
		p_proc->task.regs.fs	= ((8 * 1) & SA_RPL_MASK & SA_TI_MASK)| SA_TIL | RPL_TASK;
		p_proc->task.regs.ss	= ((8 * 1) & SA_RPL_MASK & SA_TI_MASK)| SA_TIL | RPL_TASK;
		p_proc->task.regs.gs	= (SELECTOR_KERNEL_GS & SA_RPL_MASK)| RPL_TASK;
		p_proc->task.regs.eflags = 0x1202; /* IF=1, IOPL=1 */
		//p_proc->task.cr3 在页表初始化中处理
		
		
		/**************线性地址布局初始化**********************************/	//edit by visual 2016.5.25
		p_proc->task.memmap.text_lin_base = 0;	//initial这些段的数据并不清楚，在变身init的时候才在exec中赋新值
		p_proc->task.memmap.text_lin_limit = 0;	//initial这些段的数据并不清楚，在变身init的时候才在exec中赋新值
		p_proc->task.memmap.data_lin_base = 0;	//initial这些段的数据并不清楚，在变身init的时候才在exec中赋新值
		p_proc->task.memmap.data_lin_limit= 0;	//initial这些段的数据并不清楚，在变身init的时候才在exec中赋新值
		p_proc->task.memmap.vpage_lin_base = VpageLinBase;					//保留内存基址
		p_proc->task.memmap.vpage_lin_limit = VpageLinBase;					//保留内存界限
		p_proc->task.memmap.heap_lin_base = HeapLinBase;						//堆基址
		p_proc->task.memmap.heap_lin_limit = HeapLinBase;						//堆界限	
		p_proc->task.memmap.stack_lin_base = StackLinBase;						//栈基址
		p_proc->task.memmap.stack_lin_limit = StackLinBase - 0x4000;					//栈界限（使用时注意栈的生长方向）
		p_proc->task.memmap.arg_lin_base = ArgLinBase;						//参数内存基址
		p_proc->task.memmap.arg_lin_limit = ArgLinBase;						//参数内存界限
		p_proc->task.memmap.kernel_lin_base = KernelLinBase;					//内核基址
		p_proc->task.memmap.kernel_lin_limit = KernelLinBase + KernelSize;		//内核大小初始化为8M
		
		/*************************进程树信息初始化***************************************/
		p_proc->task.info.type = TYPE_PROCESS;			//当前是进程还是线程
		p_proc->task.info.real_ppid = -1;  	//亲父进程，创建它的那个进程
		p_proc->task.info.ppid = -1;			//当前父进程	
		p_proc->task.info.child_p_num = 0;	//子进程数量
		//p_proc->task.info.child_process[NR_CHILD_MAX];//子进程列表
		p_proc->task.info.child_t_num = 0;		//子线程数量
		//p_proc->task.info.child_thread[NR_CHILD_MAX];//子线程列表	
		p_proc->task.info.text_hold = 1;			//是否拥有代码
		p_proc->task.info.data_hold = 1;			//是否拥有数据
		
		
		/***************初始化PID进程页表*****************************/
		if( 0 != init_page_pte(pid) )
		{
			disp_color_str("kernel_main Error:init_page_pte",0x74);
			return -1;
		}
		//pde_addr_phy_temp = get_pde_phy_addr(pid);//获取该进程页目录物理地址	//edit by visual 2016.5.19
		
		/****************代码数据*****************************/
		p_proc->task.regs.eip= (u32)initial;//进程入口线性地址		edit by visual 2016.5.17
		
		/****************栈（此时堆、栈已经区分，以后实验会重新规划堆的位置）*****************************/
		p_proc->task.regs.esp=(u32)StackLinBase;			//栈地址最高处	
		for( AddrLin=StackLinBase ; AddrLin>p_proc->task.memmap.stack_lin_limit ; AddrLin-=num_4K )
		{//栈
			//addr_phy_temp = (u32)test_kmalloc_4k();//为栈申请一个物理页,Task的栈是在内核里面 //delete by visual 2016.5.19
			//if( addr_phy_temp<0 || (addr_phy_temp&0x3FF)!=0  )
			//{
			//	disp_color_str("kernel_main Error:addr_phy_temp",0x74);
			//	return -1;
			//}
			err_temp = lin_mapping_phy(	AddrLin,//线性地址
										MAX_UNSIGNED_INT,//物理地址		//edit by visual 2016.5.19
										pid,//进程pid	//edit by visual 2016.5.19
										PG_P  | PG_USU | PG_RWW,//页目录的属性位
										PG_P  | PG_USU | PG_RWW);//页表的属性位
			if( err_temp!=0 )
			{
				disp_color_str("kernel_main Error:lin_mapping_phy",0x74);
				return -1;
			}
								
		}
		
		/***************copy registers data****************************/
		//copy registers data to the bottom of the new kernel stack
		//added by xw, 17/12/11
		p_regs = (char*)(p_proc + 1);
		p_regs -= P_STACKTOP;
		memcpy(p_regs, (char*)p_proc, 18 * 4);
		
		p_proc->task.esp_save_int = p_regs; //initialize esp_save_int, added by xw, 17/12/11
		
		/***************变量调整****************************/
		p_proc++;
		selector_ldt += 1 << 3;
	}
	for( 	; pid<NR_PCBS ; pid++ )
	{//3>对后NR_K_PCBS~NR_PCBS的PCB表部分初始化,(名称,pid,stat,LDT选择子),状态为IDLE.
		/*************基本信息*********************************/
		strcpy(p_proc->task.p_name, "USER");		//名称
		p_proc->task.pid = pid;							//pid
		p_proc->task.stat = IDLE;  						//状态
		
		/**************LDT*********************************/
		p_proc->task.ldt_sel = selector_ldt;
		memcpy(&p_proc->task.ldts[0], &gdt[SELECTOR_KERNEL_CS >> 3],sizeof(DESCRIPTOR));
		p_proc->task.ldts[0].attr1 = DA_C | PRIVILEGE_USER << 5;
		memcpy(&p_proc->task.ldts[1], &gdt[SELECTOR_KERNEL_DS >> 3],sizeof(DESCRIPTOR));
		p_proc->task.ldts[1].attr1 = DA_DRW | PRIVILEGE_USER << 5;
		
		/**************寄存器初值**********************************/
		p_proc->task.regs.cs	= ((8 * 0) & SA_RPL_MASK & SA_TI_MASK)| SA_TIL | RPL_USER;
		p_proc->task.regs.ds	= ((8 * 1) & SA_RPL_MASK & SA_TI_MASK)| SA_TIL | RPL_USER;
		p_proc->task.regs.es	= ((8 * 1) & SA_RPL_MASK & SA_TI_MASK)| SA_TIL | RPL_USER;
		p_proc->task.regs.fs	= ((8 * 1) & SA_RPL_MASK & SA_TI_MASK)| SA_TIL | RPL_USER;
		p_proc->task.regs.ss	= ((8 * 1) & SA_RPL_MASK & SA_TI_MASK)| SA_TIL | RPL_USER;
		p_proc->task.regs.gs	= (SELECTOR_KERNEL_GS & SA_RPL_MASK)| RPL_USER;
		p_proc->task.regs.eflags = 0x0202; /* IF=1, 倒数第二位恒为1 */
		
		/****************页表、代码数据、堆栈*****************************/
		//无
		
		/***************copy registers data****************************/
		//copy registers data to the bottom of the new kernel stack
		//added by xw, 17/12/11
		p_regs = (char*)(p_proc + 1);
		p_regs -= P_STACKTOP;
		memcpy(p_regs, (char*)p_proc, 18 * 4);
		
		p_proc->task.esp_save_int = p_regs; //initialize esp_save_int, added by xw, 17/12/11
		
		/***************变量调整****************************/
		p_proc++;
		selector_ldt += 1 << 3;
	}	
	
	proc_table[0].task.ticks = proc_table[0].task.priority = 1;		//edit by visual 2016.4.5
	proc_table[1].task.ticks = proc_table[1].task.priority = 1;		//edit by visual 2016.4.5
	proc_table[2].task.ticks = proc_table[2].task.priority = 1;		//edit by visual 2016.4.5
	proc_table[NR_K_PCBS].task.ticks = proc_table[NR_K_PCBS].task.priority = 1;		//add by visual 2016.4.5

	/************************************************************************
	*计时器部分初始化
	*************************************************************************/
    /* 初始化 8253 PIT */
    out_byte(TIMER_MODE, RATE_GENERATOR);
    out_byte(TIMER0, (u8) (TIMER_FREQ/HZ) );
    out_byte(TIMER0, (u8) ((TIMER_FREQ/HZ) >> 8));
	ticks = 0;

	/***********************************************************************
	*中断部分初始化
	************************************************************************/
    put_irq_handler(CLOCK_IRQ, clock_handler); /* 设定时钟中断处理程序 */
    enable_irq(CLOCK_IRQ);                     /* 让8259A可以接收时钟中断 */
	k_reenter = 0;

	/*************************************************************************
	*第一个进程开始启动执行
	**************************************************************************/
	p_proc_current	= proc_table;
	//restart();
	restart_int();	//modified by xw, 17/12/11
	while(1){}
}


/*======================================================================*
                               initial   //add by visual 2016.5.17	
 *======================================================================*/
 void initial()
 {
	/*int i=0x4000;
	while (1) 
	{
		disp_str("I");
		disp_int(i++);
		disp_str(" ");
		milli_delay(1);
	}*/
	exec("init/init.bin");
 }
 
/*======================================================================*
                               TestA
 *======================================================================*/
void TestA()				//edit by visual 2016.4.11
{
	int i = 0x1000;
/*
	if( 0==fork() )
	{//子进程
		while (1) 
		{
			disp_int(get_pid());
			disp_int(i++);
			disp_str(" ");
			milli_delay(1);
		}
	
	}
	else
	{//父进程
		while (1) 
		{
			disp_str("A");
			disp_int(i++);
			disp_str(" ");
			milli_delay(1);
		} 
	}
*/
	//exec("init/init.bin");
	while (1) 
	{
		disp_str("A");
		disp_int(i++);
		disp_str(" ");
		milli_delay(100);
	} 
}


/*======================================================================*
                               TestB
 *======================================================================*/
void pthread_Test()//add by visual 2016.4.11
{
	while(1)
	{
		disp_str(proc_table[get_pid()].task.p_name);
		(*(int*)0x401FFC)--;
		disp_int(*(int*)0x401FFC);
		disp_str(" ");
		milli_delay(10);
	}
}

void TestB()		//edit by visual 2016.4.11
{
	int i=0x2000;
	//(*(int*)0x401FFC) = 0x2000;
	//pthread((void*)pthread_Test);
/*	while(1){
		disp_str("B");
		(*(int*)0x401FFC)++;
		disp_int(*(int*)0x401FFC);		
		disp_str(" ");
		milli_delay(10);
	}*/
	while (1) 
	{
		disp_str("B");
		disp_int(i++);
		disp_str(" ");
		milli_delay(100);
	}
}


/*======================================================================*
                               TestC
 *======================================================================*/
void TestC()		//edit by visual 2016.4.8
{
	/*int *i;
	malloc(sizeof(int));//这个只是用来干扰的
	i = (int*)malloc(sizeof(int));
	*i = 0x3000;
	while (1) {
		disp_str("Ci=");
		disp_int((int)i);
		disp_str("*i=");
		disp_int((*i)++);
		disp_str(" ");
		milli_delay(1);
		//free_4k(i);
	}*/
	int i=0x3000;
	while (1) 
	{
		disp_str("C");
		disp_int(i++);
		disp_str(" ");
		milli_delay(100);
	}
}
