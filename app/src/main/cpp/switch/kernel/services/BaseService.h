#include "../../common.h"

namespace lightSwitch::kernel::service {
    class BaseService {
        virtual const char* name() = 0;
        BaseService() {}
    };
}
