#ifndef PLATFORM_INFO_H
#define PLATFORM_INFO_H

#if defined(__arm__)
	#define cpu_is_x86() 0
	#define cpu_is_arm() 1
#else
	#define cpu_is_x86() 1
	#define cpu_is_arm() 0
#endif

#endif // PLATFORM_INFO_H
