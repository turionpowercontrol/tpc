
#ifndef __SYSDEP_H
#define __SYSDEP_H

#define CLEARSCREEN_FLAG_SMART 0x01

bool initializeCore(void);
bool deinitializeCore(void);
void ClearScreen(unsigned int flags);

#endif /* __SYSDEP_H */
