
#ifndef	_INTERRUPTS_H_
#define	_INTERRUPTS_H_

#define MAX_INT_NUM 192
#define INVERSE_NULL ((void *)(~0))


/* function declaration */
int irq_register (unsigned int irq_num, void (*fxn)(void),
	unsigned char level0edge1);


#endif /* _INTERRUPTS_H_ */
