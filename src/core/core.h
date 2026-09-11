#ifndef YIALITE_CORE_H
#define YIALITE_CORE_H

#include <cassert>

#define YIALITE_API

#ifdef _DEBUG
    #define YIALITE_ASSERT(expression) assert(expression)
#else
    #define YIALITE_ASSERT(expression) ((void)0)
#endif

#endif
