// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

.text
.global SaveCtx
SaveCtx:
    STR LR, [SP, #-16]!
    MRS LR, TPIDR_EL0
    STP X0, X1, [LR, #16]
    STP X2, X3, [LR, #32]
    STP X4, X5, [LR, #48]
    STP X6, X7, [LR, #64]
    STP X8, X9, [LR, #80]
    STP X10, X11, [LR, #96]
    STP X12, X13, [LR, #112]
    STP X14, X15, [LR, #128]
    STP X16, X17, [LR, #144]
    STP X18, X19, [LR, #160]
    STP X20, X21, [LR, #176]
    STP X22, X23, [LR, #192]
    STP X24, X25, [LR, #208]
    STP X26, X27, [LR, #224]
    STP X28, X29, [LR, #240]
    LDR LR, [SP], #16
    RET

.global LoadCtx
LoadCtx:
    STR LR, [SP, #-16]!
    MRS LR, TPIDR_EL0
    LDP X0, X1, [LR, #16]
    LDP X2, X3, [LR, #32]
    LDP X4, X5, [LR, #48]
    LDP X6, X7, [LR, #64]
    LDP X8, X9, [LR, #80]
    LDP X10, X11, [LR, #96]
    LDP X12, X13, [LR, #112]
    LDP X14, X15, [LR, #128]
    LDP X16, X17, [LR, #144]
    LDP X18, X19, [LR, #160]
    LDP X20, X21, [LR, #176]
    LDP X22, X23, [LR, #192]
    LDP X24, X25, [LR, #208]
    LDP X26, X27, [LR, #224]
    LDP X28, X29, [LR, #240]
    LDR LR, [SP], #16
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

