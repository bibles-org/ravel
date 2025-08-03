#pragma once

#include "dispatcher.h"

namespace cli {
    class repl {
    public:
        repl();
        void run();

    private:
        dispatcher m_dispatcher;
    };
} // namespace cli
