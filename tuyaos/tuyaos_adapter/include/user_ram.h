#ifndef _USER_RAM_H_
#define _USER_RAM_H_

#define SECTION(_name) __attribute__ ((__section__(_name)))
#define USER_RAM_SECTION                     \
                SECTION(".user.ram.data")

#endif // _USER_RAM_H_
// eof

