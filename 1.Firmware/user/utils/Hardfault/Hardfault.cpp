#include "Hardfault.hpp"

__attribute__((naked, noreturn)) void HardFault_Handler(void)
{
    __asm volatile(
        ".syntax unified\n"
        "tst lr, #4\n"
        "ite eq\n"
        "mrseq r0, msp\n"
        "mrsne r0, psp\n"
        "b HardFault_HandlerC\n");
}

[[noreturn]] void HardFault_HandlerC(unsigned int *pStack)
{
    HardFaultRegs.syshndctrl.byte = SYSHND_CTRL;
    HardFaultRegs.mfsr.byte = NVIC_MFSR;
    HardFaultRegs.bfsr.byte = NVIC_BFSR;
    HardFaultRegs.bfar = NVIC_BFAR;
    HardFaultRegs.ufsr.byte = NVIC_UFSR;
    HardFaultRegs.hfsr.byte = NVIC_HFSR;
    HardFaultRegs.dfsr.byte = NVIC_DFSR;
    HardFaultRegs.afsr = NVIC_AFSR;

    HardFaultRegs.SavedRegs.r0 = pStack[0];
    HardFaultRegs.SavedRegs.r1 = pStack[1];
    HardFaultRegs.SavedRegs.r2 = pStack[2];
    HardFaultRegs.SavedRegs.r3 = pStack[3];
    HardFaultRegs.SavedRegs.r12 = pStack[4];
    HardFaultRegs.SavedRegs.lr = pStack[5];
    HardFaultRegs.SavedRegs.pc = pStack[6];
    HardFaultRegs.SavedRegs.psr.byte = pStack[7];
    for (;;) {
    }
}
