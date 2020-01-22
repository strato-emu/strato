.text
.global saveCtx
saveCtx:
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
    DSB SY
    RET

.global loadCtx
loadCtx:
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
