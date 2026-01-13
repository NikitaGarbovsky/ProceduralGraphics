module;

export module DebugUtilities;

import <print>;

export void Log(const char* _message)
{
	std::println("{}", _message);
}

export void LogWarning(const char* _message)
{
	std::string convertedMessage = _message;
	std::println("----- ERROR WARNING! -----{}", convertedMessage);
}

export void LogCrash(const char* _message)
{
	std::string convertedMessage = _message;
	std::println("===== ERROR CRASH! ===== {}", convertedMessage);
}