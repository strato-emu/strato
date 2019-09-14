#include "../../common.h"
#include "../ipc.h"

namespace lightSwitch::kernel::service {
    class BaseService {
      protected:
        device_state &state;

      public:
        BaseService(device_state &state) : state(state) {}

        virtual const char *Name() = 0;

        virtual ipc::IpcResponse HandleSyncRequest(ipc::IpcRequest &request) = 0;


    };
}
