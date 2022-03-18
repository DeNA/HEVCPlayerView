/**
 * The MIT License (MIT)
 *
 * Copyright (c) 2022 DeNA Co., Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef BASE_LOG_H_
#define BASE_LOG_H_

#include <stdio.h>

/**
 * Writes a verbose message to the debug console.
 * @param {const char*} text
 */
#ifdef HEVC_VERBOSE
#define HEVC_LOG_V(...) printf("hevc:verbose: " __VA_ARGS__)
#else
#define HEVC_LOG_V(...)
#endif  // VERBOSE

/**
 * Writes a debug message to the debug console.
 * @param {const char*} text
 */
#if HEVC_DEBUG
#define HEVC_LOG_D(...) printf("hevc:debug: " __VA_ARGS__)
#else
#define HEVC_LOG_D(...)
#endif

/**
 * Writes an info message to the debug console.
 * @param {const char*} format
 * @param {...(int|float|const char*)} var_args
 */
#define HEVC_LOG_I(...) printf("hevc:info: " __VA_ARGS__)

/**
 * Writes a warning message to the debug console.
 * @param {const char*} format
 * @param {...(int|float|const char*)} var_args
 */
#define HEVC_LOG_W(...) printf("hevc:warning: " __VA_ARGS__)

/**
 * Writes an error message to the debug console.
 * @param {const char*} format
 * @param {...(int|float|const char*)} var_args
 */
#define HEVC_LOG_E(...) printf("hevc:error: " __VA_ARGS__)

/**
 * Throws a breakpoint exception.
 */
#if HEVC_DEBUG == 1 && _WIN32
#define HEVC_DEBUGGER() __debugbreak()
#else
#define HEVC_DEBUGGER()
#endif
 
/**
 * Writes an assertion message to the debug console.
 * @param {int} expr
 */
#if HEVC_DEBUG == 1
#define HEVC_ASSERT(expr) \
    if (!(expr)) { \
      HEVC_LOG_D( \
          "%s:%u: failed assertion '%s'.", __FILE__, __LINE__, #expr); \
      HEVC_DEBUGGER(); \
    }
#else
#define HEVC_ASSERT(expr)
#endif

 /**
  * Writes a 'not implemented' message to the debug console.
  */
#ifndef HEVC_NOT_IMPLEMENTED
#define HEVC_NOT_IMPLEMENTED() { \
    HEVC_LOG_D("%s(): not implemented.\n", __FUNCTION__); \
  }
#endif

#endif  // BASE_LOG_H_
