#include <sys/resource.h>
#include "KThread.h"
#include "KProcess.h"
#include "../../nce.h"

namespace lightSwitch::kernel::type {
    KThread::KThread(handle_t handle, pid_t pid, uint64_t entry_point, uint64_t entry_arg, uint64_t stack_top, uint64_t tls, uint8_t priority, KProcess* parent, const device_state &state) : handle(handle), pid(pid), entry_point(entry_point), entry_arg(entry_arg), stack_top(stack_top), tls(tls), priority(priority), parent(parent), state(state), KObject(handle, KObjectType::KThread) {
        UpdatePriority(priority);
    }

    KThread::~KThread() {
        kill(pid, SIGKILL);
    }

    void KThread::Start() {
        if(pid==parent->main_thread) parent->process_state = KProcess::process_state_t::Started;
        state.nce->StartProcess(entry_point, entry_arg, stack_top, handle, pid);
    }

    void KThread::UpdatePriority(uint8_t priority) {
        this->priority = priority;
        auto li_priority = static_cast<int8_t>(constant::priority_an.first + ((static_cast<float>(constant::priority_an.second - constant::priority_an.first) / static_cast<float>(constant::priority_nin.second - constant::priority_nin.first)) * (static_cast<float>(priority) - constant::priority_nin.first))); // Remap range priority_nin (Nintendo Priority) to priority_an (Android Priority)
        if(setpriority(PRIO_PROCESS, static_cast<id_t>(pid), li_priority) == -1)
            throw exception(fmt::format("Couldn't set process priority to {} for PID: {}", li_priority, pid));
    }
}
