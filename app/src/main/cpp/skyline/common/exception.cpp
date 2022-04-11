// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "signal.h"
#include "exception.h"

namespace skyline {
    std::vector<void *> exception::GetStackFrames() {
        std::vector<void*> frames;
        signal::StackFrame *frame{};
        asm("MOV %0, FP" : "=r"(frame));
        if (frame)
            frame = frame->next; // We want to skip the first frame as it's going to be the caller of this function
        while (frame && frame->lr) {
            frames.push_back(frame->lr);
            frame = frame->next;
        }
        return frames;
    }
}
