// Flex Compiler - Platform compatibility header
#ifndef FLEX_PLATFORM_H
#define FLEX_PLATFORM_H

// Windows-specific defines
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#endif

#endif // FLEX_PLATFORM_H
