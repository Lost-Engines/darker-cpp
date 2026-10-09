#pragma once
// The host reports ROM failures; per-register diagnostics are disabled.
#define LOG_MSG(...) ((void)0)
#define LOG(...) [](auto...) {}
