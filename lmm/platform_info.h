#ifndef PLATFORM_INFO_H
#define PLATFORM_INFO_H

#if defined(__ARM_ARCH_5__) || defined(__ARM_ARCH_5E__) || defined(__ARM_ARCH_5T__) || defined(__ARM_ARCH_5TE__) || defined(__ARM_ARCH_5TEJ__)
	#define cpu_is_x86() 0
	#define cpu_is_arm() 1
	#define cpu_is_armv5() 1
	#define cpu_is_armv7() 1
	#define cpu_is_armv64() 0
#elif defined(__ARM_ARCH_7__) || defined(__ARM_ARCH_7A__) || defined(__ARM_ARCH_7R__) || defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7S__)
	#define cpu_is_x86() 0
	#define cpu_is_arm() 1
	#define cpu_is_armv5() 0
	#define cpu_is_armv7() 1
	#define cpu_is_armv64() 0
#elif defined(__aarch64__)
	#define cpu_is_x86() 0
	#define cpu_is_arm() 1
	#define cpu_is_armv5() 0
	#define cpu_is_armv7() 0
	#define cpu_is_armv64() 1
#else
	#define cpu_is_x86() 1
	#define cpu_is_arm() 0
	#define cpu_is_armv5() 0
	#define cpu_is_armv7() 0
	#define cpu_is_armv64() 0
#endif

#endif // PLATFORM_INFO_H
