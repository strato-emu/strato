// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

.text
.global SaveCtx
SaveCtx:
    /* Prepare Scratch Register */
    STR LR, [SP, #8] // It is assumed that 8B of stack memory has already been allocated before calling this
    MRS LR, TPIDR_EL0

    /* Store GP Registers */
    STP X0, X1, [LR, #0]
    STP X2, X3, [LR, #16]
    STP X4, X5, [LR, #32]
    STP X6, X7, [LR, #48]
    STP X8, X9, [LR, #64]
    STP X10, X11, [LR, #80]
    STP X12, X13, [LR, #96]
    STP X14, X15, [LR, #112]
    STP X16, X17, [LR, #128]
    STP X18, X19, [LR, #144]
    STP X20, X21, [LR, #160]
    STP X22, X23, [LR, #176]
    STP X24, X25, [LR, #192]
    STP X26, X27, [LR, #208]
    STP X28, X29, [LR, #224]

    /* Store FP Registers */
    STP Q0, Q1, [LR, #240]
    STP Q2, Q3, [LR, #272]
    STP Q4, Q5, [LR, #304]
    STP Q6, Q7, [LR, #336]
    STP Q8, Q9, [LR, #368]
    STP Q10, Q11, [LR, #400]
    STP Q12, Q13, [LR, #432]
    STP Q14, Q15, [LR, #464]
    STP Q16, Q17, [LR, #496]
    STP Q18, Q19, [LR, #528]
    STP Q20, Q21, [LR, #560]
    STP Q22, Q23, [LR, #592]
    STP Q24, Q25, [LR, #624]
    STP Q26, Q27, [LR, #656]
    STP Q28, Q29, [LR, #688]
    STP Q30, Q31, [LR, #720]

    /* Store FPCR/FPSR */
    MRS	X0, FPSR
    STR	W0, [LR, #744]
    MRS	X1, FPCR
    STR	W1, [LR, #748]

    /* Restore Scratch Register */
    LDR LR, [SP, #8]
    RET

.global LoadCtx
LoadCtx:
    /* Prepare Scratch Register */
    STR LR, [SP, #8] // It is assumed that 8B of stack memory has already been allocated before calling this
    MRS LR, TPIDR_EL0

    /* Load FP Registers */
    LDP Q0, Q1, [LR, #240]
    LDP Q2, Q3, [LR, #272]
    LDP Q4, Q5, [LR, #304]
    LDP Q6, Q7, [LR, #336]
    LDP Q8, Q9, [LR, #368]
    LDP Q10, Q11, [LR, #400]
    LDP Q12, Q13, [LR, #432]
    LDP Q14, Q15, [LR, #464]
    LDP Q16, Q17, [LR, #496]
    LDP Q18, Q19, [LR, #528]
    LDP Q20, Q21, [LR, #560]
    LDP Q22, Q23, [LR, #592]
    LDP Q24, Q25, [LR, #624]
    LDP Q26, Q27, [LR, #656]
    LDP Q28, Q29, [LR, #688]
    LDP Q30, Q31, [LR, #720]

    /* Store FPCR/FPSR */
    MRS	X0, FPSR
    STR	W0, [LR, #744]
    MRS	X1, FPCR
    STR	W1, [LR, #748]

    /* Load GP Registers */
     LDP X0, X1, [LR, #0]
     LDP X2, X3, [LR, #16]
     LDP X4, X5, [LR, #32]
     LDP X6, X7, [LR, #48]
     LDP X8, X9, [LR, #64]
     LDP X10, X11, [LR, #80]
     LDP X12, X13, [LR, #96]
     LDP X14, X15, [LR, #112]
     LDP X16, X17, [LR, #128]
     LDP X18, X19, [LR, #144]
     LDP X20, X21, [LR, #160]
     LDP X22, X23, [LR, #176]
     LDP X24, X25, [LR, #192]
     LDP X26, X27, [LR, #208]
     LDP X28, X29, [LR, #224]

    /* Restore Scratch Register */
    LDR LR, [SP, #8]
    RET

.global RescaleClock
RescaleClock:
    SUB SP, SP, #32
    STP X0, X1, [SP, #16]
    MOV X0, #30787
    MOVK X0, #29108, LSL #16
    MOVK X0, #23236, LSL #32
    MOVK X0, #2684, LSL #48
    MRS X1, CNTFRQ_EL0
    LSR X1, X1, #5
    UMULH X1, X1, X0
    LSR X1, X1, #7
    MRS X0, CNTVCT_EL0
    UDIV X1, X0, X1
    ADD X1, X1, X1, LSL #1
    LSL X0, X1, #6
    STR X0, [SP, #0]
    LDP X0, X1, [SP, #16]

