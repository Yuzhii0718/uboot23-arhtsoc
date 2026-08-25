/*(C) Copyright 2023 Airoha Technology Corp.
 * 
 *  Shubham Jain, Shubham.jain@airoha.common.
 *  Zhengping Zhang , zhengping.zhang@airoha.com
*/ 

#ifndef __ARHT_RESET_H
#define __ARHT_RESET_H

struct udevice;

int arht_reset_bind(struct udevice *pdev, u32 regofs, u32 num_regs);

#endif	
