#include <stdio.h>
#include <unistd.h>
#if defined(__GLIBC__)
 #define DETECTED_LIBC "glibc"
 #define LIBC_VERSION_MAJOR __GLIBC__
 #define LIBC_VERSION_MINOR __GLIBC_MINOR__
#elif defined(__MUSL__)
 #define DETECTED_LIBC "musl"
#elif defined(__APPLE__) && defined(__MACH__)
 #define DETECTED_LIBC "Apple libc"
#else
 #define DETECTED_LIBC "unknown libc"
#endif
int main() {
 printf("Hello, world!\n");
 // libc 版本信息 
#ifdef __GLIBC__
 printf("libc: %s %d.%d\n", DETECTED_LIBC, LIBC_VERSION_MAJOR, 
LIBC_VERSION_MINOR);
#else
 printf("libc: %s\n", DETECTED_LIBC);
#endif
 // POSIX 版本 
#ifdef _POSIX_VERSION
 printf("POSIX version: %ld\n", _POSIX_VERSION);
#else
 printf("POSIX version: not defined\n");
#endif
 return 0;
}