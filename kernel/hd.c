/// zcr copy whole file from Orange's and the file was modified.

/*************************************************************************//**
 *****************************************************************************
 * @file   hd.c
 * @brief  Hard disk (winchester) driver.
 * The `device nr' in this file means minor device nr.
 * @author Forrest Y. Yu
 * @date   2005~2008
 *****************************************************************************
 *****************************************************************************/

#include "type.h"
#include "const.h"
#include "protect.h"
#include "string.h"
#include "proc.h"
#include "global.h"
#include "proto.h"
#include "fs_const.h"
#include "hd.h"
#include "fs.h"
#include "fs_misc.h"


PUBLIC  void	init_hd();
PUBLIC  void	hd_open(int device);
PUBLIC  void	hd_close(int device);
PUBLIC  void	hd_rdwt(MESSAGE * p);
PUBLIC  void	hd_ioctl(MESSAGE * p);
PUBLIC  void	hd_cmd_out(struct hd_cmd* cmd);
PRIVATE void	get_part_table(int drive, int sect_nr, struct part_ent * entry);
PRIVATE void	partition(int device, int style);
PUBLIC  void	print_hdinfo(struct hd_info * hdi);
PUBLIC  int		waitfor(int mask, int val, int timeout);
PUBLIC  void	interrupt_wait();
PUBLIC 	void	hd_identify(int drive);
PUBLIC  void	print_identify_info(u16* hdinfo);
PUBLIC	void 	hd_handler(int irq);
PUBLIC	void	inform_int();

PRIVATE volatile int hd_int_waiting_flag;
PRIVATE	u8 hd_status;
PRIVATE	u8 hdbuf[SECTOR_SIZE * 2];
PRIVATE	struct hd_info hd_info[1];

#define	DRV_OF_DEV(dev) (dev <= MAX_PRIM ? \
			 dev / NR_PRIM_PER_DRIVE : \
			 (dev - MINOR_hd1a) / NR_SUB_PER_DRIVE)

/*****************************************************************************
 *                                init_hd
 *****************************************************************************/
/**
 * <Ring 1> Check hard drive, set IRQ handler, enable IRQ and initialize data
 *          structures.
 *****************************************************************************/
PUBLIC void init_hd()
{
	int i;

	put_irq_handler(AT_WINI_IRQ, hd_handler);
	enable_irq(CASCADE_IRQ);
	enable_irq(AT_WINI_IRQ);

	for (i = 0; i < (sizeof(hd_info) / sizeof(hd_info[0])); i++)
		memset(&hd_info[i], 0, sizeof(hd_info[0]));
	hd_info[0].open_cnt = 0;
}

/*****************************************************************************
 *                                hd_open
 *****************************************************************************/
/**
 * <Ring 1> This routine handles DEV_OPEN message. It identify the drive
 * of the given device and read the partition table of the drive if it
 * has not been read.
 * 
 * @param device The device to be opened.
 *****************************************************************************/
PUBLIC void hd_open(int device)
{
	disp_str("Read hd information...  ");
	
	/* Get the number of drives from the BIOS data area */
	u8 * pNrDrives = (u8*)(0x475);
	// printf("NrDrives:%d.\n", *pNrDrives);
	disp_str("NrDrives:");
	disp_int(*pNrDrives);
	disp_str("\n");
	
	int drive = DRV_OF_DEV(device);
	hd_identify(drive);

	if (hd_info[drive].open_cnt++ == 0) {
		partition(drive * (NR_PART_PER_DRIVE + 1), P_PRIMARY);
		// print_hdinfo(&hd_info[drive]);
	}
}

/*****************************************************************************
 *                                hd_close
 *****************************************************************************/
/**
 * <Ring 1> This routine handles DEV_CLOSE message. 
 * 
 * @param device The device to be opened.
 *****************************************************************************/
PUBLIC void hd_close(int device)
{
	int drive = DRV_OF_DEV(device);

	hd_info[drive].open_cnt--;
}


/*****************************************************************************
 *                                hd_rdwt
 *****************************************************************************/
/**
 * <Ring 1> This routine handles DEV_READ and DEV_WRITE message.
 * 
 * @param p Message ptr.
 *****************************************************************************/
PUBLIC void hd_rdwt(MESSAGE * p)
{
	int drive = DRV_OF_DEV(p->DEVICE);

	u64 pos = p->POSITION;

	/**
	 * We only allow to R/W from a SECTOR boundary:
	 */

	u32 sect_nr = (u32)(pos >> SECTOR_SIZE_SHIFT); /* pos / SECTOR_SIZE */
	int logidx = (p->DEVICE - MINOR_hd1a) % NR_SUB_PER_DRIVE;
	sect_nr += p->DEVICE < MAX_PRIM ?
		hd_info[drive].primary[p->DEVICE].base :
		hd_info[drive].logical[logidx].base;

	struct hd_cmd cmd;
	cmd.features	= 0;
	cmd.count	= (p->CNT + SECTOR_SIZE - 1) / SECTOR_SIZE;
	cmd.lba_low	= sect_nr & 0xFF;
	cmd.lba_mid	= (sect_nr >>  8) & 0xFF;
	cmd.lba_high	= (sect_nr >> 16) & 0xFF;
	cmd.device	= MAKE_DEVICE_REG(1, drive, (sect_nr >> 24) & 0xF);
	cmd.command	= (p->type == DEV_READ) ? ATA_READ : ATA_WRITE;
	hd_cmd_out(&cmd);

	int bytes_left = p->CNT;
	void * la = (void*)va2la(p->PROC_NR, p->BUF);

	while (bytes_left) {
		int bytes = min(SECTOR_SIZE, bytes_left);
		if (p->type == DEV_READ) {
			interrupt_wait();
			port_read(REG_DATA, hdbuf, SECTOR_SIZE);
			phys_copy(la, (void*)va2la(proc2pid(p_proc_current), hdbuf), bytes);
		}
		else {
			if (!waitfor(STATUS_DRQ, STATUS_DRQ, HD_TIMEOUT))
				disp_str("hd writing error.");

			port_write(REG_DATA, la, bytes);
			interrupt_wait();
		}
		bytes_left -= SECTOR_SIZE;
		la += SECTOR_SIZE;
	}
}															


/*****************************************************************************
 *                                hd_ioctl
 *****************************************************************************/
/**
 * <Ring 1> This routine handles the DEV_IOCTL message.
 * 
 * @param p  Ptr to the MESSAGE.
 *****************************************************************************/
PUBLIC void hd_ioctl(MESSAGE * p)
{
	int device = p->DEVICE;
	int drive = DRV_OF_DEV(device);

	struct hd_info * hdi = &hd_info[drive];

	if (p->REQUEST == DIOCTL_GET_GEO) {
		void * dst = va2la(p->PROC_NR, p->BUF);
		void * src = va2la(proc2pid(p_proc_current),
				   device < MAX_PRIM ?
				   &hdi->primary[device] :
				   &hdi->logical[(device - MINOR_hd1a) %
						NR_SUB_PER_DRIVE]);

		phys_copy(dst, src, sizeof(struct part_info));
	}
	else {
		// assert(0);
	}
}

/*****************************************************************************
 *                                get_part_table
 *****************************************************************************/
/**
 * <Ring 1> Get a partition table of a drive.
 * 
 * @param drive   Drive nr (0 for the 1st disk, 1 for the 2nd, ...)n
 * @param sect_nr The sector at which the partition table is located.
 * @param entry   Ptr to part_ent struct.
 *****************************************************************************/
PRIVATE void get_part_table(int drive, int sect_nr, struct part_ent * entry)
{
	struct hd_cmd cmd;
	cmd.features	= 0;
	cmd.count	= 1;
	cmd.lba_low	= sect_nr & 0xFF;
	cmd.lba_mid	= (sect_nr >>  8) & 0xFF;
	cmd.lba_high	= (sect_nr >> 16) & 0xFF;
	cmd.device	= MAKE_DEVICE_REG(1, /* LBA mode*/
					  drive,
					  (sect_nr >> 24) & 0xF);
	cmd.command	= ATA_READ;
	hd_cmd_out(&cmd);
	interrupt_wait();

	port_read(REG_DATA, hdbuf, SECTOR_SIZE);
	memcpy(entry,
	       hdbuf + PARTITION_TABLE_OFFSET,
	       sizeof(struct part_ent) * NR_PART_PER_DRIVE);
}

/*****************************************************************************
 *                                partition
 *****************************************************************************/
/**
 * <Ring 1> This routine is called when a device is opened. It reads the
 * partition table(s) and fills the hd_info struct.
 * 
 * @param device Device nr.
 * @param style  P_PRIMARY or P_EXTENDED.
 *****************************************************************************/
PRIVATE void partition(int device, int style)
{
	int i;
	int drive = DRV_OF_DEV(device);
	struct hd_info * hdi = &hd_info[drive];

	struct part_ent part_tbl[NR_SUB_PER_DRIVE];

	if (style == P_PRIMARY) {
		get_part_table(drive, drive, part_tbl);

		int nr_prim_parts = 0;
		for (i = 0; i < NR_PART_PER_DRIVE; i++) { /* 0~3 */
			if (part_tbl[i].sys_id == NO_PART) 
				continue;

			nr_prim_parts++;
			int dev_nr = i + 1;		  /* 1~4 */
			hdi->primary[dev_nr].base = part_tbl[i].start_sect;
			hdi->primary[dev_nr].size = part_tbl[i].nr_sects;

			if (part_tbl[i].sys_id == EXT_PART) /* extended */
				partition(device + dev_nr, P_EXTENDED);
		}
	}
	else if (style == P_EXTENDED) {
		int j = device % NR_PRIM_PER_DRIVE; /* 1~4 */
		int ext_start_sect = hdi->primary[j].base;
		int s = ext_start_sect;
		int nr_1st_sub = (j - 1) * NR_SUB_PER_PART; /* 0/16/32/48 */

		for (i = 0; i < NR_SUB_PER_PART; i++) {
			int dev_nr = nr_1st_sub + i;/* 0~15/16~31/32~47/48~63 */

			get_part_table(drive, s, part_tbl);

			hdi->logical[dev_nr].base = s + part_tbl[0].start_sect;
			hdi->logical[dev_nr].size = part_tbl[0].nr_sects;

			s = ext_start_sect + part_tbl[1].start_sect;

			/* no more logical partitions
			   in this extended partition */
			if (part_tbl[1].sys_id == NO_PART)
				break;
		}
	}
	else {
		// assert(0);
	}
}

/*****************************************************************************
 *                                print_hdinfo
 *****************************************************************************/
/**
 * <Ring 1> Print disk info.
 * 
 * @param hdi  Ptr to struct hd_info.
 *****************************************************************************/
PUBLIC void print_hdinfo(struct hd_info * hdi)
{
	int i;
	for (i = 0; i < NR_PART_PER_DRIVE + 1; i++) {
		// printl("%sPART_%d: base %d(0x%x), size %d(0x%x) (in sector)\n",
		//        i == 0 ? " " : "     ",
		//        i,
		//        hdi->primary[i].base,
		//        hdi->primary[i].base,
		//        hdi->primary[i].size,
		//        hdi->primary[i].size);
		if(i == 0) {	
			disp_str(" ");
		}
		else {
			disp_str("     ");
		}
		disp_str("PART_");
		disp_int(i);
		disp_str(": base ");
		disp_int(hdi->primary[i].base);
		disp_str("), size");
		disp_int(hdi->primary[i].size);
		disp_str(" (in sector)\n");
	}
	for (i = 0; i < NR_SUB_PER_DRIVE; i++) {
		if (hdi->logical[i].size == 0)
			continue;
		// printl("         "
		//        "%d: base %d(0x%x), size %d(0x%x) (in sector)\n",
		//        i,
		//        hdi->logical[i].base,
		//        hdi->logical[i].base,
		//        hdi->logical[i].size,
		//        hdi->logical[i].size);
		disp_str("         ");
		disp_int(i);
		disp_str(": base ");
		disp_int(hdi->logical[i].base);
		disp_str(", size ");
		disp_int(hdi->logical[i].size);
		disp_str(" (in sector)\n");
	}
}

/*****************************************************************************
 *                                hd_identify
 *****************************************************************************/
/**
 * <Ring 1> Get the disk information.
 * 
 * @param drive  Drive Nr.
 *****************************************************************************/
PUBLIC void hd_identify(int drive)
{
	struct hd_cmd cmd;
	cmd.device  = MAKE_DEVICE_REG(0, drive, 0);
	cmd.command = ATA_IDENTIFY;
	hd_cmd_out(&cmd);
	interrupt_wait();
	port_read(REG_DATA, hdbuf, SECTOR_SIZE);

	print_identify_info((u16*)hdbuf);

	u16* hdinfo = (u16*)hdbuf;

	hd_info[drive].primary[0].base = 0;
	/* Total Nr of User Addressable Sectors */
	hd_info[drive].primary[0].size = ((int)hdinfo[61] << 16) + hdinfo[60];
}

/*****************************************************************************
 *                            print_identify_info
 *****************************************************************************/
/**
 * <Ring 1> Print the hdinfo retrieved via ATA_IDENTIFY command.
 * 
 * @param hdinfo  The buffer read from the disk i/o port.
 *****************************************************************************/
PUBLIC void print_identify_info(u16* hdinfo)
{
	int i, k;
	char s[64];

	struct iden_info_ascii {
		int idx;
		int len;
		char * desc;
	} iinfo[] = {{10, 20, "HD SN"}, /* Serial number in ASCII */
		     {27, 40, "HD Model"} /* Model number in ASCII */ };

	for (k = 0; k < sizeof(iinfo)/sizeof(iinfo[0]); k++) {
		char * p = (char*)&hdinfo[iinfo[k].idx];
		for (i = 0; i < iinfo[k].len/2; i++) {
			s[i*2+1] = *p++;
			s[i*2] = *p++;
		}
		s[i*2] = 0;
		// printl("%s: %s\n", iinfo[k].desc, s);
		disp_str(iinfo[k].desc);
		disp_str(":");
		disp_str(s);
		disp_str("\n");
	}

	int capabilities = hdinfo[49];
	// printl("LBA supported: %s\n", (capabilities & 0x0200) ? "Yes" : "No");
	disp_str("LBA supported:");
	if((capabilities & 0x0200))
		disp_str("YES  ") ;
	else disp_str("NO  ");
	// disp_str("\n");

	int cmd_set_supported = hdinfo[83];
	// printl("LBA48 supported: %s\n", (cmd_set_supported & 0x0400) ? "Yes" : "No");
	disp_str("LBA48 supported:");
	if((cmd_set_supported & 0x0400))
		disp_str("YES  ");
	else disp_str("NO  ");
	// disp_str("\n");

	int sectors = ((int)hdinfo[61] << 16) + hdinfo[60];
	// printl("HD size: %dMB\n", sectors * 512 / 1000000);
	disp_str("HD size:");
	disp_int(sectors * 512 / 1000000);
	disp_str("MB\n");
}

/*****************************************************************************
 *                                hd_cmd_out
 *****************************************************************************/
/**
 * <Ring 1> Output a command to HD controller.
 * 
 * @param cmd  The command struct ptr.
 *****************************************************************************/
PUBLIC void hd_cmd_out(struct hd_cmd* cmd)
{
	/**
	 * For all commands, the host must first check if BSY=1,
	 * and should proceed no further unless and until BSY=0
	 */
	if (!waitfor(STATUS_BSY, 0, HD_TIMEOUT))
		// panic("hd error.");
		disp_str("hd error.");

	/* Activate the Interrupt Enable (nIEN) bit */
	out_byte(REG_DEV_CTRL, 0);
	/* Load required parameters in the Command Block Registers */
	out_byte(REG_FEATURES, cmd->features);
	out_byte(REG_NSECTOR,  cmd->count);
	out_byte(REG_LBA_LOW,  cmd->lba_low);
	out_byte(REG_LBA_MID,  cmd->lba_mid);
	out_byte(REG_LBA_HIGH, cmd->lba_high);
	out_byte(REG_DEVICE,   cmd->device);
	/* Write the command code to the Command Register */
	out_byte(REG_CMD,     cmd->command);
}

/*****************************************************************************
 *                                interrupt_wait
 *****************************************************************************/
/**
 * <Ring 1> Wait until a disk interrupt occurs.
 * 
 *****************************************************************************/
/// modified by zcr(using Huper's method.)
// PUBLIC void interrupt_wait()
// {
// 	while(hd_int_waiting_flag) {
// 		// milli_delay(20);/// waiting for the harddisk interrupt.
// 	}
// 	hd_int_waiting_flag = 1;
// }

PUBLIC void interrupt_wait()
{
	while(hd_int_waiting_flag) {
		/* milli_delay invoke syscall get_ticks, so we can't use it.
		 * for this scene, just do nothing is OK. modified by xw, 18/6/1
		 */
		//milli_delay(5);/// waiting for the harddisk interrupt.
	}
	hd_int_waiting_flag = 1;
}

/*****************************************************************************
 *                                waitfor
 *****************************************************************************/
/**
 * <Ring 1> Wait for a certain status.
 * 
 * @param mask    Status mask.
 * @param val     Required status.
 * @param timeout Timeout in milliseconds.
 * 
 * @return One if sucess, zero if timeout.
 *****************************************************************************/
PUBLIC int waitfor(int mask, int val, int timeout)
{
	//we can't use syscall get_ticks before process run. modified by xw, 18/5/31
	/*
	int t = get_ticks();
	
	while(((get_ticks() - t) * 1000 / HZ) < timeout)
		if ((in_byte(REG_STATUS) & mask) == val)
			return 1;
	*/
	int t = sys_get_ticks();
	
	while(((sys_get_ticks() - t) * 1000 / HZ) < timeout){
		if ((in_byte(REG_STATUS) & mask) == val)
			return 1;
	}
	
	return 0;
}

/*****************************************************************************
 *                                hd_handler
 *****************************************************************************/
/**
 * <Ring 0> Interrupt handler.
 * 
 * @param irq  IRQ nr of the disk interrupt.
 *****************************************************************************/
PUBLIC void hd_handler(int irq)
{
	/*
	 * Interrupts are cleared when the host
	 *   - reads the Status Register,
	 *   - issues a reset, or
	 *   - writes to the Command Register.
	 */
	hd_status = in_byte(REG_STATUS);
	inform_int();
	
	/* There is two stages - in kernel intializing or in process running.
	 * Some operation shouldn't be valid in kernel intializing stage.
	 * added by xw, 18/6/1
	 */
	if(kernel_initial == 1){
		return;
	}
	
	//some operation only for process
	
	return;
}

/*****************************************************************************
 *                                inform_int
 *****************************************************************************/
PUBLIC void inform_int()
{
	hd_int_waiting_flag = 0;
	return;
}