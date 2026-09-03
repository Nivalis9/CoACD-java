
#ifndef DISABLE_SPDLOG
#include "logger.h"
#include <spdlog/sinks/stdout_color_sinks.h>

namespace coacd
{
    namespace logger
    {

        std::shared_ptr<spdlog::logger> get()
        {
            static std::shared_ptr<spdlog::logger> logger = [] {
                auto instance = spdlog::stdout_color_mt("CoACD");
                instance->set_level(spdlog::level::info);
                return instance;
            }();
            return logger;
        }
    } // namespace log
}
#endif
