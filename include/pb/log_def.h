#ifndef INCLUDE_LOG_DEF_H
#define INCLUDE_LOG_DEF_H

#include <stdio.h>
#include <config.h>

#if LOGLEVEL >= 2
    #define LOG_INFO(...) \
        do { printf("I %s: " , __func__);\
             printf(__VA_ARGS__);\
             printf("\n\r"); } while (0)

    #define LOG_INFO2(...) \
        do { printf("I: ");\
             printf(__VA_ARGS__); } while (0)
#else
    #define LOG_INFO(...)
    #define LOG_INFO2(...)
#endif

#if LOGLEVEL >= 3
    #define LOG_DBG(...) \
        do { printf("D %s: " , __func__);\
             printf(__VA_ARGS__);\
             printf("\n\r"); } while (0)
#else
    #define LOG_DBG(...)
#endif

#if LOGLEVEL >= 1
    #define LOG_WARN(...) \
        do { printf("W %s: " , __func__);\
             printf(__VA_ARGS__);\
             printf("\n\r"); } while (0)

    #define LOG_ERR(...) \
        do { printf("E %s: " , __func__);\
             printf(__VA_ARGS__);\
             printf("\n\r"); } while (0)
#else
    #define LOG_WARN(...)
    #define LOG_ERR(...)
#endif

#endif
