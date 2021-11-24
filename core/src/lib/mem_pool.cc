/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2013-2021 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/
/*
 * Historically, the following was may have been true, but nowadays operating
 * systems are a lot better at handling memory than we are, so the pooling has
 * been removed.
 *
 * Andreas Rogge
 */
/*
 * BAREOS memory pool routines.
 *
 * The idea behind these routines is that there will be pools of memory that
 * are pre-allocated for quick access. The pools will have a fixed memory size
 * on allocation but if need be, the size can be increased. This is particularly
 * useful for filename buffers where 256 bytes should be sufficient in 99.99%
 * of the cases, but when it isn't we want to be able to increase the size.
 *
 * A major advantage of the pool memory aside from the speed is that the buffer
 * carries around its size, so to ensure that there is enough memory, simply
 * call the CheckPoolMemorySize() with the desired size and it will adjust only
 * if necessary.
 *
 * Kern E. Sibbald
 */
#include "lib/mem_pool.h"

#include <stdarg.h>
#include <string.h>

#include "lib/util.h"
#include "include/baconfig.h"

// Memory allocation control structures and storage.
struct abufhead {
  int32_t ablen;     /* Buffer length in bytes */
  int32_t bnet_size; /* dummy for BnetSend() */
};

constexpr int32_t HEAD_SIZE{BALIGN(sizeof(struct abufhead))};

/*
 * Special version of error reporting using a static buffer so we don't use
 * the normal error reporting which uses dynamic memory e.g. recursivly calls
 * these routines again leading to deadlocks.
 */
[[noreturn]] static void MemPoolErrorMessage(const char* file,
                                             int line,
                                             const char* fmt,
                                             ...)
{
  char buf[256];
  va_list arg_ptr;
  int len;

  len = Bsnprintf(buf, sizeof(buf), _("%s: ABORTING due to ERROR in %s:%d\n"),
                  my_name, get_basename(file), line);

  va_start(arg_ptr, fmt);
  Bvsnprintf(buf + len, sizeof(buf) - len, (char*)fmt, arg_ptr);
  va_end(arg_ptr);

  DispatchMessage(NULL, M_ABORT, 0, buf);
  abort();
}

POOLMEM* GetPoolMemory(int pool)
{
  static constexpr int32_t pool_init_size[] = {
      256,                 /* PM_NOPOOL no pooling */
      MAX_NAME_LENGTH + 2, /* PM_NAME Bareos name */
      256,                 /* PM_FNAME filename buffers */
      512,                 /* PM_MESSAGE message buffer */
      1024,                /* PM_EMSG error message buffer */
      4096,                /* PM_BSOCK message buffer */
      128                  /* PM_RECORD message buffer */
  };
  const int32_t alloc_size = pool_init_size[pool];

  struct abufhead* buf;
  if ((buf = (struct abufhead*)malloc(alloc_size + HEAD_SIZE)) == NULL) {
    MemPoolErrorMessage(__FILE__, __LINE__,
                        _("Out of memory requesting %d bytes\n"), alloc_size);
    return NULL;
  }

  buf->ablen = alloc_size;
  return (POOLMEM*)(((char*)buf) + HEAD_SIZE);
}

/* Get nonpool memory of size requested */
POOLMEM* GetMemory(int32_t size)
{
  struct abufhead* buf;

  if ((buf = (struct abufhead*)malloc(size + HEAD_SIZE)) == NULL) {
    MemPoolErrorMessage(__FILE__, __LINE__,
                        _("Out of memory requesting %d bytes\n"), size);
    return NULL;
  }

  buf->ablen = size;
  return (POOLMEM*)(((char*)buf) + HEAD_SIZE);
}

/* Return the size of a memory buffer */
int32_t SizeofPoolMemory(POOLMEM* obuf)
{
  char* cp = (char*)obuf;

  ASSERT(obuf);
  cp -= HEAD_SIZE;
  return ((struct abufhead*)cp)->ablen;
}

/* Realloc pool memory buffer */
POOLMEM* ReallocPoolMemory(POOLMEM* obuf, int32_t size)
{
  ASSERT(obuf);
  void* buf = realloc((char*)obuf - HEAD_SIZE, size + HEAD_SIZE);
  if (buf == NULL) {
    MemPoolErrorMessage(__FILE__, __LINE__,
                        _("Out of memory requesting %d bytes\n"), size);
    return NULL;
  }

  ((struct abufhead*)buf)->ablen = size;
  return (POOLMEM*)(((char*)buf) + HEAD_SIZE);
}

POOLMEM* CheckPoolMemorySize(POOLMEM* obuf, int32_t size)
{
  ASSERT(obuf);
  if (size <= SizeofPoolMemory(obuf)) { return obuf; }
  return ReallocPoolMemory(obuf, size);
}

/* Free a memory buffer */
void FreePoolMemory(POOLMEM* obuf)
{
  ASSERT(obuf);
  struct abufhead* buf = (struct abufhead*)((char*)obuf - HEAD_SIZE);

  free((char*)buf);
}

/*
 * Concatenate a string (str) onto a pool memory buffer pm
 * Returns: length of concatenated string
 */
int PmStrcat(POOLMEM*& pm, const char* str)
{
  int pmlen = strlen(pm);
  int len;

  if (!str) str = "";

  len = strlen(str) + 1;
  pm = CheckPoolMemorySize(pm, pmlen + len);
  memcpy(pm + pmlen, str, len);
  return pmlen + len - 1;
}

int PmStrcat(POOLMEM*& pm, PoolMem& str)
{
  int pmlen = strlen(pm);
  int len = strlen(str.c_str()) + 1;

  pm = CheckPoolMemorySize(pm, pmlen + len);
  memcpy(pm + pmlen, str.c_str(), len);
  return pmlen + len - 1;
}

int PmStrcat(PoolMem& pm, const char* str)
{
  int pmlen = strlen(pm.c_str());
  int len;

  if (!str) str = "";

  len = strlen(str) + 1;
  pm.check_size(pmlen + len);
  memcpy(pm.c_str() + pmlen, str, len);
  return pmlen + len - 1;
}

int PmStrcat(PoolMem*& pm, const char* str)
{
  int pmlen = strlen(pm->c_str());
  int len;

  if (!str) str = "";

  len = strlen(str) + 1;
  pm->check_size(pmlen + len);
  memcpy(pm->c_str() + pmlen, str, len);
  return pmlen + len - 1;
}

/*
 * Copy a string (str) into a pool memory buffer pm
 * Returns: length of string copied
 */
int PmStrcpy(POOLMEM*& pm, const char* str)
{
  int len;

  if (!str) str = "";

  len = strlen(str) + 1;
  pm = CheckPoolMemorySize(pm, len);
  memcpy(pm, str, len);
  return len - 1;
}

int PmStrcpy(POOLMEM*& pm, PoolMem& str)
{
  int len = strlen(str.c_str()) + 1;

  pm = CheckPoolMemorySize(pm, len);
  memcpy(pm, str.c_str(), len);
  return len - 1;
}

int PmStrcpy(PoolMem& pm, const char* str)
{
  int len;

  if (!str) str = "";

  len = strlen(str) + 1;
  pm.check_size(len);
  memcpy(pm.c_str(), str, len);
  return len - 1;
}

int PmStrcpy(PoolMem*& pm, const char* str)
{
  int len;

  if (!str) str = "";

  len = strlen(str) + 1;
  pm->check_size(len);
  memcpy(pm->c_str(), str, len);
  return len - 1;
}

/*
 * Copy data into a pool memory buffer pm
 * Returns: length of data copied
 */
int PmMemcpy(POOLMEM*& pm, const char* data, int32_t n)
{
  pm = CheckPoolMemorySize(pm, n);
  memcpy(pm, data, n);
  return n;
}

int PmMemcpy(POOLMEM*& pm, PoolMem& data, int32_t n)
{
  pm = CheckPoolMemorySize(pm, n);
  memcpy(pm, data.c_str(), n);
  return n;
}

int PmMemcpy(PoolMem& pm, const char* data, int32_t n)
{
  pm.check_size(n);
  memcpy(pm.c_str(), data, n);
  return n;
}

int PmMemcpy(PoolMem*& pm, const char* data, int32_t n)
{
  pm->check_size(n);
  memcpy(pm->c_str(), data, n);
  return n;
}

/* ==============  CLASS PoolMem   ============== */

// Return the size of a memory buffer
int32_t PoolMem::MaxSize()
{
  int32_t size;
  char* cp = mem;

  cp -= HEAD_SIZE;
  size = ((struct abufhead*)cp)->ablen;

  return size;
}

void PoolMem::ReallocPm(int32_t size)
{
  char* cp = mem;
  char* buf;

  cp -= HEAD_SIZE;
  buf = (char*)realloc(cp, size + HEAD_SIZE);
  if (buf == NULL) {
    MemPoolErrorMessage(__FILE__, __LINE__,
                        _("Out of memory requesting %d bytes\n"), size);
    return;
  }

  ((struct abufhead*)buf)->ablen = size;
  mem = buf + HEAD_SIZE;
}

int PoolMem::strcat(PoolMem& str) { return strcat(str.c_str()); }

int PoolMem::strcat(const char* str)
{
  int pmlen = strlen();
  int len;

  if (!str) str = "";

  len = ::strlen(str) + 1;
  check_size(pmlen + len);
  memcpy(mem + pmlen, str, len);
  return pmlen + len - 1;
}

int PoolMem::strcpy(PoolMem& str) { return strcpy(str.c_str()); }

int PoolMem::strcpy(const char* str)
{
  int len;

  if (!str) str = "";

  len = ::strlen(str) + 1;
  check_size(len);
  memcpy(mem, str, len);
  return len - 1;
}

void PoolMem::toLower() { lcase(mem); }

int PoolMem::bsprintf(const char* fmt, ...)
{
  int len;
  va_list arg_ptr;
  va_start(arg_ptr, fmt);
  len = Bvsprintf(fmt, arg_ptr);
  va_end(arg_ptr);
  return len;
}

int PoolMem::Bvsprintf(const char* fmt, va_list arg_ptr)
{
  int maxlen, len;
  va_list ap;

again:
  maxlen = MaxSize() - 1;
  va_copy(ap, arg_ptr);
  len = ::Bvsnprintf(mem, maxlen, fmt, ap);
  va_end(ap);
  if (len < 0 || len >= maxlen) {
    ReallocPm(maxlen + maxlen / 2);
    goto again;
  }
  return len;
}
