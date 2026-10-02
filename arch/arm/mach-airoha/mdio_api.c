/*
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/*extern function for mdio*/
extern void miiStationWrite(uint32_t enetPhyAddr, uint32_t phyReg, uint32_t miiData);
extern uint32_t miiStationRead(uint32_t enetPhyAddr, uint32_t phyReg);

unsigned int client_mdio_addr = 0xffffffff;

int mdio_msg_send(unsigned int phy_addr, unsigned int addr, unsigned int data);

int mdio_msg_rcv(unsigned int phy_addr, unsigned int addr)
{
	int result1 = 0, result2 = 0;
	int ret = 0;
	/* enable MDIO output before read*/
	mdio_msg_send(phy_addr, 0x1fa20160, 0x0);

	miiStationWrite(phy_addr, 1, (addr&0xffff));
	miiStationWrite(phy_addr, 2, ((addr >> 16)&0xffff));
	miiStationWrite(phy_addr, 0, 0xf0);
	result1 = miiStationRead(phy_addr, 3);
	result2 = miiStationRead(phy_addr, 4);

	/* disable MDIO output after read*/
	mdio_msg_send(phy_addr, 0x1fa20160, 0x800);
    ret = result1+(result2<<16);
	return ret;

}

int mdio_msg_send(unsigned int phy_addr, unsigned int addr, unsigned int data)
{
	miiStationWrite(phy_addr, 1, (addr&0xffff));
	miiStationWrite(phy_addr, 2, ((addr >> 16)&0xffff));
	miiStationWrite(phy_addr, 3, (data&0xffff));
	miiStationWrite(phy_addr, 4, ((data >> 16)&0xffff));
	miiStationWrite(phy_addr, 0, 0xff);
	return 0;
}

 


