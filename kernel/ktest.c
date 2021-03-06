/*
 * To test if new kernel features work normally, and if old features still 
 * work normally with new features added.
 * added by xw, 18/4/27
 */
#include "type.h"
#include "const.h"
#include "protect.h"
#include "string.h"
#include "proc.h"
#include "global.h"
#include "proto.h"
#include "fs.h"

/*======================================================================*
                         Interrupt Handling Test
added by xw, 18/4/27
 *======================================================================*/
	/*
void TestA()
{
	int i, j;
	while (1)
	{
		disp_str("A ");

		i = 100;
		while(--i){
			j = 1000;
			while(--j){}
		}
	}
}

void TestB()
{
	int i, j;
	while (1)
	{
		disp_str("B ");

		i = 100;
		while(--i){
			j = 1000;
			while(--j){}
		}
	}
}

void TestC()
{
	int i, j;
	while (1)
	{
		disp_str("C ");

		i = 100;
		while(--i){
			j = 1000;
			while(--j){}
		}
	}
}

void initial()
 {
	int i, j;
	while (1)
	{
		disp_str("I ");

		i = 100;
		while(--i){
			j = 1000;
			while(--j){}
		}
	}
 }
//	*/

/*======================================================================*
                         Exception Handling Test
added by xw, 18/12/19
 *======================================================================*/
	/*
void TestA()
{
	int i, j;
	while (1)
	{
		disp_str("A ");

		i = 100;
		while(--i){
			j = 1000;
			while(--j){}
		}
		
		//generates an Undefined opcode(#UD) exception, without error code
		//UD2 is provided by Intel to explicitly generate an invalid opcode exception.
//		asm volatile ("ud2");
		
		//generates a General Protection(#GP) exception, with error code
		//the privilege level of a procedure must be 0 to execute the HLT instruction
//		asm volatile ("hlt");
		
		//generates a Divide Error(#DE) exception, without error code
		//calculate a / b
		int a, b;
		a = 10, b = 0;
		asm volatile ("mov %0, %%eax\n\t"
					  "div %1\n\t"
					  : 
					  : "r"(a), "r"(b)
					  : "eax");		
		
	}
}

void TestB()
{
	int i, j;
	while (1)
	{
		disp_str("B ");

		i = 100;
		while(--i){
			j = 1000;
			while(--j){}
		}
	}
}

void TestC()
{
	int i, j;
	while (1)
	{
		disp_str("C ");

		i = 100;
		while(--i){
			j = 1000;
			while(--j){}
		}
	}
}

void initial()
 {
	int i, j;
	while (1)
	{
		disp_str("I ");

		i = 100;
		while(--i){
			j = 1000;
			while(--j){}
		}
	}
 }
//	*/

/*======================================================================*
                        Ordinary System Call Test
added by xw, 18/4/27
 *======================================================================*/
	/*
void TestA()
{
	int i, j;
	while (1)
	{
		disp_str("A ");
		milli_delay(100);
	}
}

void TestB()
{
	int i, j;
	while (1)
	{
		disp_str("B ");
		milli_delay(100);
	}
}

void TestC()
{
	int i, j;
	while (1)
	{
		disp_str("C ");
		milli_delay(100);
	}
}

void initial()
 {
	int i, j;
	while (1)
	{
		disp_str("I ");
		milli_delay(100);
	}
 }
//	*/
	 

/*======================================================================*
                          Kernel Preemption Test
added by xw, 18/4/27
 *======================================================================*/
	/*
void TestA()
{
	int i, j;
	while (1)
	{
		disp_str("A ");
		print_E();
		milli_delay(100);
	}
}

void TestB()
{
	int i, j;
	while (1)
	{
		disp_str("B ");
		print_F();
		milli_delay(100);
	}
}

void TestC()
{
	int i, j;
	while (1)
	{
		disp_str("C ");
		milli_delay(100);
	}
}

void initial()
 {
	int i, j;
	while (1)
	{
		disp_str("I ");
		milli_delay(100);
	}
 }
//	*/

/*======================================================================*
                          Syscall Yield Test
added by xw, 18/4/27
 *======================================================================*/
	/*
void TestA()
{
	int i;
	while (1) 
	{
		disp_str("A( ");
		yield();
		disp_str(") ");
		milli_delay(100);
	} 
}

void TestB()
{
	int i, j;
	while (1)
	{
		disp_str("B ");
		milli_delay(100);
	}
}

void TestC()
{
	int i, j;
	while (1)
	{
		disp_str("C ");
		milli_delay(100);
	}
}

void initial()
 {
	int i, j;
	while (1)
	{
		disp_str("I ");
		milli_delay(100);
	}
 }
//	*/

/*======================================================================*
                          Syscall Sleep Test
added by xw, 18/4/27
 *======================================================================*/
	/*
void TestA()
{
	int i;
	while (1) 
	{
		disp_str("A( ");
		disp_str("[");
		disp_int(ticks);
		disp_str("] ");
		sleep(5);
		disp_str("[");
		disp_int(ticks);
		disp_str("] ");
		disp_str(") ");
		milli_delay(100);
	} 
}

void TestB()
{
	int i, j;
	while (1)
	{
		disp_str("B ");
		milli_delay(100);
	}
}

void TestC()
{
	int i, j;
	while (1)
	{
		disp_str("C ");
		milli_delay(100);
	}
}

void initial()
 {
	int i, j;
	while (1)
	{
		disp_str("I ");
		milli_delay(100);
	}
 }
//	*/

/*======================================================================*
                          User Process Test
added by xw, 18/4/27
 *======================================================================*/
/* You should also enable the feature you want to test in init.c */
//	/*
void TestA()
{
	int i, j;
	while (1)
	{
		disp_str("A ");
		milli_delay(100);
	}
}

void TestB()
{
	int i, j;
	while (1)
	{
		disp_str("B ");
		milli_delay(100);
	}
}

void TestC()
{
	int i, j;
	while (1)
	{
		disp_str("C ");
		milli_delay(100);
	}
}

void initial()
 {
	exec("init/init.bin");
	while(1)
	{
		
	}
 }
//	*/

/*======================================================================*
                          File System Test
added by xw, 18/5/26
 *======================================================================*/
	/*
void TestA()
{	
	//while (1) {}
	
	//manipulate /blah
	//if /blah exists, open
	//if /blah doesn't exist, open+write+close
	int fd;
	int i, n;
	char filename[MAX_FILENAME_LEN+1] = "blah";
	const char bufw[] = "abcde";
	const int rd_bytes = 4;
	char bufr[5];

	disp_str("(TestA)");
	fd = open(filename, O_CREAT | O_RDWR);	
	
	if(fd != -1) {
		disp_str("File created: ");
		disp_str(filename);
		disp_str(" (fd ");
		disp_int(fd);
		disp_str(")\n");	
		
		n = write(fd, bufw, strlen(bufw));
		if(n != strlen(bufw)) {
			disp_str("Write error\n");
		}
		
		close(fd);
	}
	
	while (1) {
	}
}


void TestB()
{	
	//while (1) {}
	
	//manipulate /blah, open+lseek+read+close
	int fd, n;
	const int rd_bytes = 4;
	char bufr[5];
	char filename[MAX_FILENAME_LEN+1] = "blah";

	disp_str("(TestB)");
	fd = open(filename, O_RDWR);
	disp_str("       ");
	disp_str("File opened. fd: ");
	disp_int(fd);
	disp_str("\n");

	disp_str("(TestB)");
	int lseek_status = lseek(fd, 1, SEEK_SET);
	disp_str("Return value of lseek is: ");
	disp_int(lseek_status);
	disp_str("  \n");

	disp_str("(TestB)");
	n = read(fd, bufr, rd_bytes);
	if(n != rd_bytes) {
		disp_str("Read error\n");
	}
	bufr[n] = 0;
	disp_str("Bytes read: ");
	disp_str(bufr);
	disp_str("\n");

	close(fd);

	while(1){

	}
}

void TestC()
{
	//while (1) {}
	
	//manipulate /blah, open+lseek+read+close
	int fd, n;
	const int rd_bytes = 3;
	char bufr[4];
	char filename[MAX_FILENAME_LEN+1] = "blah";
	
	disp_str("(TestC)");
	fd = open(filename, O_RDWR);
	disp_str("       ");
	disp_str("File opened. fd: ");
	disp_int(fd);
	disp_str("\n");

	disp_str("(TestC)");
	int lseek_status = lseek(fd, 1, SEEK_SET);
	disp_str("Return value of lseek is: ");
	disp_int(lseek_status);
	disp_str("  \n");

	disp_str("(TestC)");
	n = read(fd, bufr, rd_bytes);
	if(n != rd_bytes) {
		disp_str("Read error\n");
	}
	bufr[n] = 0;
	disp_str("bytes read: ");
	disp_str(bufr);
	disp_str("\n");

	close(fd);
	
	while(1){
	}
}

void initial()
{
	//while (1) {}
	
	int i, fd;
	char* filenames[] = {"/foo", "/bar", "/baz"};
	char* rfilenames[] = {"/bar", "/foo", "/baz", "/dev_tty0"};
	
	//open+close
	for (i = 0; i < sizeof(filenames) / sizeof(filenames[0]); i++) {
		disp_str("(Initial)");
		fd = open(filenames[i], O_CREAT | O_RDWR);
		if(fd != -1) {
			disp_str("File created: ");
			disp_str(filenames[i]);
			disp_str(" (fd ");
			disp_int(fd);
			disp_str(")\n");

			close(fd);
		}
	}

	//unlink
	for (i = 0; i < sizeof(rfilenames) / sizeof(rfilenames[0]); i++) {
		disp_str("(Initial)");
		if (unlink(rfilenames[i]) == 0) {
			disp_str("File removed: ");
			disp_str(rfilenames[i]);
			disp_str("\n");
		}
		else {
			disp_str("         ");
			disp_str("Failed to remove file: ");
			disp_str(rfilenames[i]);
			disp_str("\n");
		}
	}
	
	while (1){
	}
}
//	*/

/*======================================================================*
                          File System Test 2
to test interrupt_wait_sched() used in hd.c
interrupt_wait_sched() is not used currently, because it can only support
one process to access hd disk, as you can see below.
added by xw, 18/8/16
 *======================================================================*/
	/*
void TestA()
{
	int i, j;
	while (1)
	{
		disp_str("A ");
		milli_delay(100);
	}
}

void TestB()
{	
	//while (1) {}
	
	int fd, n;
	char bufr[5];
	char bufw[5] = "mmnn";
	char filename[MAX_FILENAME_LEN+1] = "blah";

	disp_str("(TestB)");
	fd = open(filename, O_RDWR);
	disp_str("       ");
	disp_str("File opened. fd: ");
	disp_int(fd);
	disp_str("\n");

	disp_str("(TestB)");
	lseek(fd, 1, SEEK_SET);
	n = write(fd, bufw, 4);
	if(n != 4) {
		disp_str("Write error\n");
	}
	bufr[4] = 0;
	disp_str("Bytes write: ");
	disp_str(bufw);
	disp_str("\n");
	
	disp_str("(TestB)");
	lseek(fd, 1, SEEK_SET);
	n = read(fd, bufr, 4);
	if(n != 4) {
		disp_str("Read error\n");
	}
	bufr[4] = 0;
	disp_str("Bytes read: ");
	disp_str(bufr);
	disp_str("\n");

	close(fd);

	while(1){

	}
}

void TestC()
{
	int i, j;
	while (1)
	{
		disp_str("C ");
		milli_delay(100);
	}
}

void initial()
 {
	int i, j;
	while (1)
	{
		disp_str("I ");
		milli_delay(100);
	}
 }
//	*/
