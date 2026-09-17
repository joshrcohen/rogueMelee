#ifndef MELEE_ROGUE_FORMAT_H
#define MELEE_ROGUE_FORMAT_H
#include <stdarg.h>
#ifdef __MWERKS__
#include <printf.h>
#else
#include <stdio.h>
#endif
static int Rogue_Format(char* s,size_t n,const char* format,...)
{
    va_list args; int result;
    va_start(args,format);result=vsnprintf(s,n,format,args);va_end(args);return result;
}
#define snprintf Rogue_Format
#endif
