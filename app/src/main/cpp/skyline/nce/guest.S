// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

.text
.global SaveCtx
SaveCtx:
    /* Prepare Scratch Register */
    STR LR, [SP, #8] // It is assumed that 8B of stack memory has already been allocated before calling this
    MRS LR, TPIDR_EL0

    /* Store GP Registers */
    STP X0, X1, [LR, #(8 * 0)]
    STP X2, X3, [LR, #(8 * 2)]
    STP X4, X5, [LR, #(8 * 4)]
    STP X6, X7, [LR, #(8 * 6)]
    STP X8, X9, [LR, #(8 * 8)]
    STP X10, X11, [LR, #(8 * 10)]
    STP X12, X13, [LR, #(8 * 12)]
    STP X14, X15, [LR, #(8 * 14)]
    STP X16, X17, [LR, #(8 * 16)]
    STR X18, [LR, #(8 * 18)]

    /* Store FP Registers */
    STP Q0, Q1, [LR, #(0xA0 + 16 * 0)]
    STP Q2, Q3, [LR, #(0xA0 + 16 * 2)]
    STP Q4, Q5, [LR, #(0xA0 + 16 * 4)]
    STP Q6, Q7, [LR, #(0xA0 + 16 * 6)]
    STP Q8, Q9, [LR, #(0xA0 + 16 * 8)]
    STP Q10, Q11, [LR, #(0xA0 + 16 * 10)]
    STP Q12, Q13, [LR, #(0xA0 + 16 * 12)]
    STP Q14, Q15, [LR, #(0xA0 + 16 * 14)]
    STP Q16, Q17, [LR, #(0xA0 + 16 * 16)]
    STP Q18, Q19, [LR, #(0xA0 + 16 * 18)]
    STP Q20, Q21, [LR, #(0xA0 + 16 * 20)]
    STP Q22, Q23, [LR, #(0xA0 + 16 * 22)]
    STP Q24, Q25, [LR, #(0xA0 + 16 * 24)]
    STP Q26, Q27, [LR, #(0xA0 + 16 * 26)]
    STP Q28, Q29, [LR, #(0xA0 + 16 * 28)]
    STP Q30, Q31, [LR, #(0xA0 + 16 * 30)]


    /* Store System Registers */
    STR X0, [SP, #-16]!
    MRS	X0, FPSR
    STR	W0, [LR, #0x298]
    MRS	X0, FPCR
    STR	W0, [LR, #0x29C]
    MRS	X0, NZCV
    STR	W0, [LR, #0x2C0]
    LDR X0, [SP], #16

    /* Restore Scratch Register */
    LDR LR, [SP, #8]
    RET

.global LoadCtx
LoadCtx:
    /* Prepare Scratch Register */
    STR LR, [SP, #8] // It is assumed that 8B of stack memory has already been allocated before calling this
    MRS LR, TPIDR_EL0

    /* Load FP Registers */
    LDP Q0, Q1, [LR, #(0xA0 + 16 * 0)]
    LDP Q2, Q3, [LR, #(0xA0 + 16 * 2)]
    LDP Q4, Q5, [LR, #(0xA0 + 16 * 4)]
    LDP Q6, Q7, [LR, #(0xA0 + 16 * 6)]
    LDP Q8, Q9, [LR, #(0xA0 + 16 * 8)]
    LDP Q10, Q11, [LR, #(0xA0 + 16 * 10)]
    LDP Q12, Q13, [LR, #(0xA0 + 16 * 12)]
    LDP Q14, Q15, [LR, #(0xA0 + 16 * 14)]
    LDP Q16, Q17, [LR, #(0xA0 + 16 * 16)]
    LDP Q18, Q19, [LR, #(0xA0 + 16 * 18)]
    LDP Q20, Q21, [LR, #(0xA0 + 16 * 20)]
    LDP Q22, Q23, [LR, #(0xA0 + 16 * 22)]
    LDP Q24, Q25, [LR, #(0xA0 + 16 * 24)]
    LDP Q26, Q27, [LR, #(0xA0 + 16 * 26)]
    LDP Q28, Q29, [LR, #(0xA0 + 16 * 28)]
    LDP Q30, Q31, [LR, #(0xA0 + 16 * 30)]

    /* Load System Registers */
    LDR	W0, [LR, #0x298]
    MSR	FPSR, X0
    LDR	W0, [LR, #0x29C]
    MSR	FPCR, X0
    LDR	W0, [LR, #0x2C0]
    MSR	NZCV, X0

    /* Load GP Registers */
    LDP X0, X1, [LR, #(8 * 0)]
    LDP X2, X3, [LR, #(8 * 2)]
    LDP X4, X5, [LR, #(8 * 4)]
    LDP X6, X7, [LR, #(8 * 6)]
    LDP X8, X9, [LR, #(8 * 8)]
    LDP X10, X11, [LR, #(8 * 10)]
    LDP X12, X13, [LR, #(8 * 12)]
    LDP X14, X15, [LR, #(8 * 14)]
    LDP X16, X17, [LR, #(8 * 16)]
    LDR X18, [LR, #(8 * 18)]

    /* Restore Scratch Register */
    LDR LR, [SP, #8]
    RET
