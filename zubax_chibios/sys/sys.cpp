/*
 * Copyright (c) 2014 Zubax, zubax.com
 * Distributed under the MIT License, available in the file LICENSE.
 * Author: Pavel Kirienko <pavel.kirienko@zubax.com>
 */

#include "sys.hpp"
#include <ch.hpp>
#include <unistd.h>
#include <cstdio>
#include <cassert>
#include <cstring>
#include <cstdarg>

namespace os
{

extern void emergencyPrint(const char* str);

void panic(const char* msg)
{
    chSysHalt(msg);
    while (1) { }
}

__attribute__((weak))
void applicationHaltHook(void) { }

void sleepUntilChTime(systime_t sleep_until)
{
    chSysLock();
    sleep_until -= chVTGetSystemTime();
    if (((int)sleep_until) > 0)
    {
        chThdSleepS(sleep_until);
    }
    chSysUnlock();

#if defined(DEBUG_BUILD) && DEBUG_BUILD
    if (((int)sleep_until) < 0)
    {
        lowsyslog("%s: Lag %d ts\n", chThdGetSelfX()->p_name, (int)sleep_until);
    }
#endif
}

} // namespace os

extern "C"
{

__attribute__((weak))
void *__dso_handle;

__attribute__((weak))
int __errno;


void zchSysHaltHook(const char* msg)
{
    using namespace os;

    applicationHaltHook();

    port_disable();
    emergencyPrint("\nPANIC [");
    const thread_t *pthread = chThdGetSelfX();
    if (pthread && pthread->p_name)
    {
        emergencyPrint(pthread->p_name);
    }
    emergencyPrint("] ");

    if (msg != NULL)
    {
        emergencyPrint(msg);
    }
    emergencyPrint("\n");

#if defined(DEBUG_BUILD) && DEBUG_BUILD && defined(CoreDebug_DHCSR_C_DEBUGEN_Msk)
    if (CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk)
    {
        __asm volatile ("bkpt #0\n"); // Break into the debugger
    }
#endif
}


static void reverse(char* s)
{
    for (int i = 0, j = strlen(s) - 1; i < j; i++, j--)
    {
        const char c = s[i];
        s[i] = s[j];
        s[j] = c;
    }
}

char* itoa(int n, char* pbuf)
{
    const int sign = n;
    if (sign < 0)
    {
        n = -n;
    }
    unsigned i = 0;
    do
    {
        pbuf[i++] = n % 10 + '0';
    }
    while ((n /= 10) > 0);
    if (sign < 0)
    {
        pbuf[i++] = '-';
    }
    pbuf[i] = '\0';
    reverse(pbuf);
    return pbuf;
}


void __assert_func(const char* file, int line, const char* func, const char* expr)
{
    (void)file;
    (void)line;
    (void)func;
    (void)expr;
    port_disable();

    char line_buf[11];
    itoa(line, line_buf);

    char buf[256]; // We don't care about possible stack overflow because we're going to die anyway
    char* ptr = buf;
    const unsigned size = sizeof(buf);
    unsigned pos = 0;

#define APPEND_MSG(text) { \
        (void)std::strncpy(ptr + pos, text, (size > pos) ? (size - pos) : 0); \
        pos += strlen(text); \
    }

    APPEND_MSG(file);
    APPEND_MSG(":");
    APPEND_MSG(line_buf);
    if (func != NULL)
    {
        APPEND_MSG(" ");
        APPEND_MSG(func);
    }
    APPEND_MSG(": ");
    APPEND_MSG(expr);

#undef APPEND_MSG

    chSysHalt(buf);
    while (1) { }
}

/// From unistd
int usleep(useconds_t useconds)
{
    chThdSleepMicroseconds(useconds);
    return 0;
}

/// From unistd
unsigned sleep(unsigned int seconds)
{
    chThdSleepSeconds(seconds);
    return 0;
}

void* malloc(size_t sz)
{
    return chCoreAlloc(sz);
}

void* calloc(size_t, size_t)
{
    os::panic("calloc");
    return nullptr;
}

void* realloc(void*, size_t)
{
    os::panic("realloc");
    return nullptr;
}

void free(void*)
{
    os::panic("free");
}

}