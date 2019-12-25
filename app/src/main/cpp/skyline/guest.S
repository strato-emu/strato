.text
.global saveCtx
saveCtx:
    STR LR, [SP, #-16]!
    MRS LR, TPIDR_EL0
    #LDR LR, [LR]
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
    LDR LR, [SP], #16
    RET

.global loadCtx
loadCtx:
    STR LR, [SP, #-16]!
    MRS LR, TPIDR_EL0
    #LDR LR, [LR]
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
    LDR LR, [SP], #16
    RET
