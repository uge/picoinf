#ifndef _M0_FAULT_DISPATCH_
#define _M0_FAULT_DISPATCH_

#include <stdint.h>


#define EXC_m0_CAUSE_MEM_READ_ACCESS_FAIL		1	//address provided
#define EXC_m0_CAUSE_MEM_WRITE_ACCESS_FAIL		2	//address provided
#define EXC_m0_CAUSE_BAD_CPU_MODE				3	//arm mode entered
#define EXC_m0_CAUSE_DATA_UNALIGNED				4	//addres provided
#define EXC_m0_CAUSE_UNDEFINSTR16				5
#define EXC_m0_CAUSE_UNDEFINSTR32				6
#define EXC_m0_CAUSE_BKPT_HIT					7
#define EXC_m0_CAUSE_CLASSIFIER_ERROR			8	//classifier itself crashed
#define EXC_m0_CAUSE_UNCLASSIFIABLE				9	//classification failed

struct CortexExcFrame {
	union {
		struct {
			uint32_t r0, r1, r2, r3;
		};
		uint32_t r0_r3[3];
	};
	uint32_t r12, lr, pc, sr;
};

struct CortexPushedRegs {	//when we push regs, we push them in this order. here for unification
	uint32_t regs8_11[4];
	uint32_t regs4_7[4];
};

//fault handling code, cause is EXC_m0_CAUSE_, extraData is usually an address. both unused on C-M3
void faultHandlerWithExcFrame(struct CortexExcFrame *exc, uint32_t cause, uint32_t extraData, struct CortexPushedRegs *pushedRegs);




#endif
