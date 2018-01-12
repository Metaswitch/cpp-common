/**
 * @file log.cpp
 *
 * Copyright (C) Metaswitch Networks 2014
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */


#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

// Define the "Warn unused return" macro (to be nothing) as this macro is
// needed by printf.h and is apparently undefined under -O2 optimisation
#define __wur
#include <printf.h>

#include <pthread.h>
#include <algorithm>
#include <boost/format.hpp>
#include "log.h"

const char* log_level[] = {"Error", "Warning", "Status", "Info", "Verbose", "Debug"};

#define MAX_LOGLINE 8192

#define RAM_BUFSIZE (1024 * 1024)

// This macro gives the size of a trace entry in the RAM trace buffer for a given
// number of parameters.
//
// In principle, we should worry about whether adjacent trace entries are
// going to be aligned properly - in the worst case, we might get an alignment
// exception if we attempt to point at a trace entry in the buffer that
// starts, e.g., on an odd address.  Fortunately, the fact that the last entry in the
// structure is the one with the greatest alignment restrictions (containing
// the largest possible C basic types) there is no risk that the size of the
// record is going to give us alignment problems of this type.
#define RAM_SIZE(NUM_PARAMS) (offsetof(TRC_RAMTRC_ENTRY, params) + (NUM_PARAMS * sizeof(TRC_RAMTRC_PARAM)))

// The following structure is used to cache the parameters expected on a given
// TRC_... call in the code.  Each entry is associated with a specific trace
// ID which is written into each trace entry in the RAM trace buffer so that the
// format string, module and line number don't have to be reparsed and
// rewritten into the RAM trace buffer for every trace call.
//
// The trace id is 1-based so that
// -  Zero filling unused space at the end of the RAM trace buffer following a wrap
//    means that such space can be easily identified as not containing a
//    a valid trace entry
// -  The calling code (see TRC_RAMTRACE in log.h) can safely treat a trc_id
//    value of zero as meaning "trace call not cached yet".
typedef struct
{
  int         line_number;
  const char *module;
  const char *fmt;
  int         num_params;
  int         entry_size;
  int         param_types[1];

} TRC_RAMTRC_CACHE;

// The following structure is a union of basic types that can be used to
// store an basic type that can be passed to a printf call.  It is used by the
// RAM trace code to store the parameter data passed on a given trace instance.
// The decoder uses the trace cache to determine how to interpret each value.
typedef union
{
  int i;
  long l;
  long long ll;
  double d;
  long double ld;
  void *p;

} TRC_RAMTRC_PARAM;

// The following structure defines an entry in the cyclic RAM trace buffer.
// The number of params structures in any given entry is variable, so the
// size of any given entry in the RAM trace buffer is not sizeof(TRC_RAMTRC_ENTRY),
// but (offsetof(TRC_RAMTRC_ENTRY,params) + <number of parameters> *
// sizeof(TRC_RAMTRC_PARAM)).
//
// There is special handling for those trace entries which just consist of a
// single string passed by value.  These are indicated by having no format
// string in the cache of trace calls (and a cached entry size of -1, as the
// entry size is variable from instance to instance).
//
// @@@ Note that this implementation is a bit wasteful on space, as each entry
// in params is 128 bits long (the size of a long long or long double),
// despite the fact that the vast majority of parameters are going to be 32-bit
// ints.  Packing parameter data on byte boundaries would be an alternative
// implementation that would be much more efficient in its use of space.
// However, in order to avoid alignment issues, parameter values would need to
// be memcpied into place which is far less performance-efficient for ints
// than straightforward assignment. @@@
typedef struct
{
  pthread_t        thread;
  int              trc_id;
  struct timeval   time;
  int              entry_size;
  TRC_RAMTRC_PARAM params[1];

} TRC_RAMTRC_ENTRY;


namespace Log
{
  static Logger logger_static;
  static Logger *logger = &logger_static;
  static pthread_mutex_t serialization_lock = PTHREAD_MUTEX_INITIALIZER;
  int loggingLevel = 4;

  // RAM trace buffer and associated static parameters.  The RAM trace buffer
  // consists of an area of memory (of arbitrary size) to which structures of type
  // TRC_RAMTRC_ENTRY are written (these structures being of variable size
  // depending on the number of arguments represented by the entry).  Note
  // that the buffer is deliberately unioned with a TRC_RAMTRC_ENTRY to ensure
  // that it is correctly aligned.
  //
  // ram_next_slot points to the next available slot in the buffer (which might
  // or might not be big enough for the next record to be written - if not, the
  // buffer is wrapped when that next record is written).
  //
  // ram_head_slot points to the oldest TRC_RAMUF_ENTRY structure in the buffer.
  //
  // Special care needs to be taken when wrapping (i.e. when the next record to
  // be written won't fit in the buffer).  We need to make sure that any
  // existing valid log data at the end of the buffer that we're having to skip
  // over gets blatted to zero so that the decode routine doesn't regard the
  // skipped bit as containing valid log data and spuriously dump rubbish.  As
  // trc_id cannot be validly 0, just checking whether this field is 0
  // is enough to tell the dumper that its reached the end of the buffer
  // and needs to wrap back to the start.
  static union
  {
    char buf[RAM_BUFSIZE];
    TRC_RAMTRC_ENTRY align;
  } ram_buffer = {{0}};

  static pthread_mutex_t ram_lock = PTHREAD_MUTEX_INITIALIZER;
  pthread_mutex_t trc_ram_trc_cache_lock = PTHREAD_MUTEX_INITIALIZER;
  static TRC_RAMTRC_ENTRY* ram_next_slot = &ram_buffer.align;

  static TRC_RAMTRC_ENTRY* ram_head_slot = NULL;

  // The trace cache is an inventory of all the trace calls in the code,
  // built up dynamically as trace calls are made.  It avoids the need to
  // duplicate the details of a given log call in the trace buffer or parse
  // the format string every time that trace call is made.
  //
  // It is indexed by ("trace id" - 1).
  static TRC_RAMTRC_CACHE** ram_trc_cache = NULL;
  static int ram_trc_cache_len = 0;
}

// Trace call cache routine.  This function caches the details of format
// string, file and line number and expected parameters.  The caller (an
// instance of the TRC_RAMTRACE macro) stores the returned ID - in actuality a
// 1-based index into an array of TRC_RAMTRC_CACHE structures - the first
// time that path through the code is executed (in a static int) and then calls
// into ramTrace below to make the RAM trace entry itself.  Subsequently the
// same trace ID can be passed directly to ramTrace for maximally efficient
// tracing of the call instance and the parameters passed.
int Log::ramCacheTrcCall(const char *module, int lineno, const char*fmt, ...)
{
  int trc_id;

  // Lock access to the RAM trace buffer.  We can't allow any ram tracing to take
  // place while we fiddle with the cache as ram_trc_cache might change under
  // the realloc call.
  pthread_mutex_lock(&Log::ram_lock);

  // Realloc (or allocate, if not yet allocated) the RAM trace cache
  Log::ram_trc_cache_len++;
  Log::ram_trc_cache = (TRC_RAMTRC_CACHE **)realloc(Log::ram_trc_cache, sizeof(TRC_RAMTRC_CACHE*) * Log::ram_trc_cache_len);

  // Release the ram trace buffer lock.  In principle, this will release any other
  // threads waiting to make trace calls, though in practise most of them will
  // be blocked on the trc_ram_trc_cache_lock lock anyway.  Note that any
  // simultaneous initial calls to this particular TRC_RAMTRACE call WILL
  // be waiting on trc_ram_trc_cache_lock until the first has successfully
  // filled the cache slot, so won't e.g. end up referencing the cache slot
  // before its been initialised.
  pthread_mutex_unlock(&Log::ram_lock);

  // The trace ID we are going to return is simply the current value of the
  // cache size (since its a 1-based index)
  trc_id = Log::ram_trc_cache_len;

  // Firstly, determine whether the format string is just "%s".  If so, then
  // the intention is clearly to trace out a string that has already been
  // formatted, and, since we normally store the pointer to any string
  // parameter rather than the string itself (and therefore will be lucky if
  // the string is still valid at dump time), we would normally not get any
  // useful info from such a trace.  Therefore, for "%s" logs only, we store
  // the string data by value rather than reference
  size_t num_params;
  int entry_size;
  TRC_RAMTRC_CACHE *cache_ent;

  if (strcmp(fmt, "%s") == 0)
  {
    // Allocate an "empty" cache structure (no params)
    cache_ent = (TRC_RAMTRC_CACHE *)malloc(offsetof(TRC_RAMTRC_CACHE, param_types));

    // Leave the format blank to indicate that no formatting is to be done
    cache_ent->fmt = NULL;
    num_params = 0;

    // Set entry size to -1 to indicate that this parameter is not used (the
    // size of an instance of this log cannot be known in advance)
    entry_size = -1;
  }
  else
  {
    // Determine how many parameters to allow for using the parse_printf_format
    // function.
    num_params = parse_printf_format(fmt, 0, NULL);
    entry_size = RAM_SIZE(num_params);

    // Get sufficient memory for the cache entry using the ANSI standard
    // algorithm for determining variable structure lengths
    cache_ent = (TRC_RAMTRC_CACHE *)malloc(offsetof(TRC_RAMTRC_CACHE, param_types) + (num_params * sizeof(int)));

    // Save the format string
    cache_ent->fmt = fmt;

    // Call the parse function again, this time allowing enough space for the
    // parameter types to be written out.
    parse_printf_format(fmt, num_params, cache_ent->param_types);
  }

  // Save the cache entry at the trace ID offset
  Log::ram_trc_cache[trc_id - 1] = cache_ent;

  // Fill in the rest of the entry
  cache_ent->num_params = num_params;
  cache_ent->line_number = lineno;

  // Strip off any path header from the module
  const char *nopath = strrchr(module, '/');
  if (nopath != NULL)
  {
    cache_ent->module = nopath + 1;
  }
  else
  {
    cache_ent->module = module;
  }

  cache_ent->entry_size = entry_size;

  return(trc_id);
}

// Fast RAM trace buffer write routine
void Log::ramTrace(int trc_id, const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);

  // Lock access to the RAM trace buffer
  pthread_mutex_lock(&Log::ram_lock);

  // Determine the entry size by looking at the cached trace object
  TRC_RAMTRC_CACHE *this_cache = Log::ram_trc_cache[trc_id - 1];
  int entry_size = this_cache->entry_size;
  int string_length;
  char *string_value = NULL;

  // If entry size is -1, this implies that our argument list is a single
  // string and we need to pass the argument by value rather than data
  if (entry_size == -1)
  {
    // Get the string value and length
    string_value = va_arg(args, char *);
    string_length = strlen(string_value);

    // Calculate the number of TRC_RAMTRC_PARAM structures we'll need in order
    // to pass the string parameter by value.  To explain the slightly odd
    // looking calculation below (which will be sensibly optimised by the
    // compiler).
    // -  We need enough space to store the string and its null terminator,
    //    hence the "+1"
    // -  We need to round UP to the number of TRC_RAMTRC_PARAMs that can
    //    store this amount (hence the +(sizeof(TRC_RAMTRC_PARAM) - 1))
    int num_params = ((string_length + 1 + (sizeof(TRC_RAMTRC_PARAM) - 1)) / sizeof(TRC_RAMTRC_PARAM));
    entry_size = RAM_SIZE(num_params);
  }

  // Find space in the buffer to write our trace entry into
  if (((char *)Log::ram_next_slot + entry_size) > (Log::ram_buffer.buf + RAM_BUFSIZE))
  {
    // The trace isn't going to fit at the end of the buffer, so we need to wrap it.
    // Set any remaining space in the buffer to zero (so that it can't be accidentally
    // interpreted as good data).
    memset((char *)Log::ram_next_slot, 0, RAM_BUFSIZE - ((char *)Log::ram_next_slot - Log::ram_buffer.buf));

    // Check whether the head slot is in the space we have just destroyed (in which case this
    // needs to be reset to the start of the buffer).
    if (Log::ram_head_slot >= Log::ram_next_slot)
    {
      Log::ram_head_slot = &Log::ram_buffer.align;
    }

    // Point the next slot at the head of the buffer
    Log::ram_next_slot = &Log::ram_buffer.align;
  }

  // If we have an oldest trace in the buffer (i.e., a "head" entry), bump this
  // along if we're about to overwrite it
  while ((Log::ram_head_slot >= Log::ram_next_slot) &&
         ((char *)Log::ram_head_slot < ((char *)Log::ram_next_slot + entry_size)))
  {
    Log::ram_head_slot = (TRC_RAMTRC_ENTRY *)((char *)Log::ram_head_slot + Log::ram_head_slot->entry_size);

    if ((((char *)Log::ram_head_slot + RAM_SIZE(0)) > (Log::ram_buffer.buf + RAM_BUFSIZE)) ||
        (Log::ram_head_slot->trc_id == 0))
    {
      // Head slot no longer points to a valid thread entry.  Wrap it and stop looking
      Log::ram_head_slot = &Log::ram_buffer.align;
      break;
    }
  }

  // We now have a slot we can write our trace entry into and a head entry
  // which we are not about to overwrite.
  TRC_RAMTRC_ENTRY *this_slot = Log::ram_next_slot;

  // Fill in the fields in the slot
  this_slot->thread = pthread_self();
  this_slot->trc_id = trc_id;
  this_slot->entry_size = entry_size;
  gettimeofday(&this_slot->time, NULL);

  // Fill in the parameters, if any.  This is slightly involved, as we need to
  // be careful to pull the arguments off the stack according to their size,
  // which we determine from the trace cache entry.
  //
  // If we have already cracked out a single string parameter, it means we
  // aren't using pass by value and are just strcpying the string into the
  // preallocated space
  if (string_value != NULL)
  {
    strcpy((char *)this_slot->params, string_value);
  }
  else
  {
    int *type = this_cache->param_types;
    TRC_RAMTRC_PARAM *value = this_slot->params;

    for (int i = 0; i < this_cache->num_params; i++)
    {
      if (*type & PA_FLAG_PTR)
      {
        // This is a pointer and can be safely treated as a void *
        value->p = va_arg(args, void *);
      }
      else
      {
        switch (*type & ~PA_FLAG_MASK)
        {
        case PA_INT:
          // use int, long or long long as appropriate.  Check for the
          // int case first for performance thats the most likely type.
          //
          // shorts are passed as ints and you get annoying compiler warnings if
          // you do "va_arg(args, short)", so ignore the PA_FLAG_SHORT flag.
          if (!(*type & (PA_FLAG_LONG|PA_FLAG_LONG_LONG)))
          {
            value->i = va_arg(args, int);
          }
          else if (*type & PA_FLAG_LONG)
          {
            value->l = va_arg(args, long);
          }
          else
          {
            // Must be a long long
            value->ll = va_arg(args, long long);
          }
          break;

        case PA_CHAR:
        case PA_WCHAR:
          // chars and wide chars are passed as ints and you get annoying
          // compiler warnings if you do "va_arg(args, char)", so treat as an
          // integer.
          value->i = va_arg(args, int);
          break;

        case PA_STRING:
        case PA_WSTRING:
        case PA_POINTER:
          // These can all be treated as void *s
          value->p = va_arg(args, void *);
          break;

        case PA_FLOAT:
          // floats are passed as doubles and you get annoying compiler warnings
          // if you do "va_arg(args, float)", so treat as a double.
          value->d = va_arg(args, double);
          break;

        case PA_DOUBLE:
          // Allow for a long double
          if (*type & PA_FLAG_LONG_DOUBLE)
          {
            value->ld = va_arg(args, long double);
          }
          else
          {
            value->d = va_arg(args, double);
          }
          break;

        default:
          // Assume any other parameter is an int
          value->i = va_arg(args, int);
          break;
        }
      }

      // Next type/value
      type++;
      value++;
    }
  }

  // Bump the next_slot pointer past the trace entry we've just written.
  // Note that it doesn't matter if there isn't enough space in the buffer to
  // write here - the next tracing thread will spot this and wrap it
  // automatically.
  Log::ram_next_slot = (TRC_RAMTRC_ENTRY *)((char *)this_slot + entry_size);

  // If we haven't got a "head" entry yet, set this up
  if (Log::ram_head_slot == NULL)
  {
    Log::ram_head_slot = this_slot;
  }

  // Release the ram trace buffer lock
  pthread_mutex_unlock(&Log::ram_lock);

  va_end(args);
}

// RAM trace buffer decode routine
//
// Note that this routine does not acquire locks.  It is intended to be run
// from termination signal handlers and is hardened to allow for possible data
// corruption.
//
// If it is ever decided to run this live, expose a separate ram_lock wrapper
// for this routine, but don't call such a wrapper from a termination
// signal handler or you may deadlock against a thread which has been
// terminated.
//
// Strings:  This routine satisfies %s format specifiers by printing the pointer
// to the string rather than the original string data.  This is because, in
// general, the string pointer will no longer be valid at the point at which
// the trace buffer was dumped (and we deliberately trace string pointers by
// pointer value rather than string contents for performance).  Note that
// trace calls that use the format string "%s" are an exception.  Such calls
// typically indicate that any already-formatted buffer is being logged, and
// logging just the pointer to this format buffer tells us nothing.  Since the
// performance hit of formatting the string has already been taken, the delta
// of writing the string to the RAM trace buffer is outweighed by the diagnostic
// benefits and so in this case (only) the string is written out in full (see
// ramCacheTrcCall above for details).
//
// In case this proves to be too limiting, code for attempting to print string
// contents is included (disabled) below.
void Log::ramDecode(FILE *output)
{
  // Start at the head of the buffer and print out eveything to the end.
  if (Log::ram_head_slot == NULL)
  {
    fprintf(output, "!!!RAM trace buffer is empty!!!\n");
    return;
  }

  TRC_RAMTRC_ENTRY *this_entry = Log::ram_head_slot;

  // We should be able to scan the buffer (expecting to wrap at most once) until
  // we get to Log::ram_next_slot (effectively the tail pointer).  However, we
  // need to take care we don't run off the end of the buffer or find ourselves
  // wrapping more than once (as might happen if this buffer has been
  // corrupted)
  bool wrapped = false;
  while ((this_entry != Log::ram_next_slot) && ((char *)this_entry <= (Log::ram_buffer.buf + RAM_BUFSIZE)))
  {
    // Print out the thread ID, module and line number
    unsigned char *ptr = (unsigned char *)&this_entry->thread;

    for (unsigned int i = 0; i < sizeof(this_entry->thread); i++)
    {
      fprintf(output, "%2.2X", ptr[i]);
    }

    // Emit the timestamp for the log and the module and line number
    char hr_dateutc[64];
    struct tm dateutc;

    gmtime_r(&this_entry->time.tv_sec, &dateutc);
    strftime(hr_dateutc, sizeof(hr_dateutc), "%d-%m-%Y %H:%M:%S", &dateutc);
    TRC_RAMTRC_CACHE* this_cache = Log::ram_trc_cache[this_entry->trc_id - 1];

    fprintf(output, " %s.%06ld UTC %s:%d: ", hr_dateutc, this_entry->time.tv_usec, this_cache->module, this_cache->line_number);

    // Treat trace lines that have been passed by value (because they're already
    // formatted) separately.  These are identified by having no format string.

#if ATTEMPTING_TO_PRINT_STRING_CONTENTS
    std::vector<void*> strings;
#endif

    if (this_cache->fmt == NULL)
    {
      // Just print out the string starting at the parameter structure
      fprintf(output, "%s", (char *)this_entry->params);
    }
    else
    {
      // Use the boost library to print the format out with the parameters we've
      // saved in place (it would be simpler to build up a va_list and call
      // vfprintf, but for the fact that va_lists can't be constructed unless
      // the parameters are on the call stack).
      boost::basic_format<char> fmt_trace(this_cache->fmt);

      // Unfortunately, the boost library does not support the precision marker
      // '*' (e.g. "... %.*s ....") in format strings, which is a shame because
      // if you pass in the integer parameter that specifies the string length
      // (as returned by the parse_printf_format routine) boost goes
      // "you've specified too many arguments" and bombs out.
      //
      // Therefore, we have to scan through the format string as we're adding
      // parameters to the output object and, if we hit an integer, we have to
      // determine whether its the precision argument and save it off.
      //
      // At the moment, we don't actually use the precision value (other than
      // as a reminder that need to skip the "fmt_trace % this_entry->params[i].i;"
      // for the parameter) as we don't print string contents - just the
      // pointers to them.
      const char *fmt_ptr = this_cache->fmt;
      int precision = -1;

      for (int i = 0; i < this_cache->num_params; i++)
      {
        fmt_ptr = strchr(fmt_ptr, '%');
        // We need to skip escaped '%' characters
        while ((fmt_ptr != NULL) && (memcmp(fmt_ptr, "%%", 2) == 0))
        {
          fmt_ptr = strchr(fmt_ptr + 2, '%');
        }

        if (fmt_ptr == NULL)
        {
          // Something's gone wrong.  Break out of the loop
          fprintf(output, "Unexpectedly reached the end of the format specifiers \"%s\"", this_cache->fmt);
          break;
        }

        bool got_pres = false;

        if (precision == -1)
        {
          // Check whether this is going to be a precision specifier by looking
          // at the current format specifier in the format string.
          const char *fmt_end = strchr(fmt_ptr, ' ');
          const char *pres_spec = strstr(fmt_ptr, ".*");

          if ((pres_spec != NULL) &&
              ((fmt_end == NULL) || (pres_spec < fmt_end)))
          {
            got_pres = true;
          }
        }

        if ((precision == -1) && got_pres)
        {
          // We've found a "*" precision marker.  We must save this value
          // and use it to format the following string/wide string
          // appropriately
          //
          // Make sure this is an int (no flags).  If it isn't, something's gone
          // wrong.
          if (this_cache->param_types[i] != PA_INT)
          {
            fprintf(output, "Unexpectedly found field of type %x when was expecting a PA_INT in \"%s\"", this_cache->param_types[i], this_cache->fmt);
            fmt_ptr = NULL;
            break;
          }

          precision = this_entry->params[i].i;

          if (precision < 0)
          {
            // Unexpected.  We must drop out here as otherwise we'll loop
            // indefinitely
            fmt_ptr = NULL;
            fprintf(output, "Unexpectedly found negative precision value %d in \"%s\"", precision, this_cache->fmt);
            break;
          }

          // Don't bump the format specifier.  We'll skip this branch next time
          // round because precision will be set.
        }
        else
        {
          if (this_cache->param_types[i] & PA_FLAG_PTR)
          {
            // This is a pointer
            fmt_trace % this_entry->params[i].p;
          }
          else
          {
            switch (this_cache->param_types[i] & ~PA_FLAG_MASK)
            {
            case PA_INT:
              // use int, long or long long as appropriate.
              if (!(this_cache->param_types[i] & (PA_FLAG_LONG|PA_FLAG_LONG_LONG)))
              {
                fmt_trace % this_entry->params[i].i;
              }
              else if (this_cache->param_types[i] & PA_FLAG_LONG)
              {
                fmt_trace % this_entry->params[i].l;
              }
              else
              {
                // Must be a long long
                fmt_trace % this_entry->params[i].ll;
              }
              break;

            case PA_CHAR:
              fmt_trace % this_entry->params[i].i;
              break;

            case PA_WCHAR:
              fmt_trace % this_entry->params[i].i;
              break;

            case PA_STRING:
#if ATTEMPTING_TO_PRINT_STRING_CONTENTS
              // If we have saved a precision value from the previous parameter,
              // copy the string to a new buffer of the appropriate size
              if (precision != -1)
              {
                char str[precision + 1];
                strncpy(str, (const char *)this_entry->params[i].p, precision);
                str[precision] = 0;
                char *str_ptr = str;
                fmt_trace % str_ptr;
              }
              else
              {
                fmt_trace % (char *)this_entry->params[i].p;
              }

              // Save the string pointer in a list.  The string we've just
              // inserted might well be rubbish, as we only saved the pointer
              // which could have been to data that was on the stack or was in
              // heap space that has since been reallocated.  If that's the case
              // the pointer itself might still have diagnostic value
              strings.push_back(this_entry->params[i].p);
#else
              // We're not attempting to print string contents as they may be
              // misleading.  Just trace out the string pointer
              fmt_trace % this_entry->params[i].p;
#endif
              break;

            case PA_WSTRING:
#if ATTEMPTING_TO_PRINT_STRING_CONTENTS
              // If we have saved a precision value from the previous parameter,
              // copy the string to a new buffer of the appropriate size
              if (precision != -1)
              {
                wchar_t wstr[precision + 1];
                wcsncpy(wstr, (const wchar_t *)this_entry->params[i].p, precision);
                wstr[precision] = 0;
                wchar_t *wstr_ptr = wstr;
                fmt_trace % wstr_ptr;
              }
              else
              {
                fmt_trace % (wchar_t *)this_entry->params[i].p;
              }

              strings.push_back(this_entry->params[i].p);
#else
              // We're not attempting to print string contents as they may be
              // misleading.  Just trace out the string pointer
              fmt_trace % this_entry->params[i].p;
#endif
              break;

            case PA_POINTER:
              fmt_trace % this_entry->params[i].p;
              break;

            case PA_FLOAT:
              fmt_trace % this_entry->params[i].d;
              break;

            case PA_DOUBLE:
              // Allow for a long double
              if (this_cache->param_types[i] & PA_FLAG_LONG_DOUBLE)
              {
                fmt_trace % this_entry->params[i].ld;
              }
              else
              {
                fmt_trace % this_entry->params[i].d;
              }
              break;

            default:
              // Assume any other parameter is an int
              fmt_trace % this_entry->params[i].i;
              break;
            }
          }

          // Bump the format pointer so that we look at the next format specifier
          // (if any) and reset any precision value we might have parsed out
          fmt_ptr++;
          precision = -1;
        }
      }

      // Write the formatted trace line out.  If we couldn't parse the format
      // string, we've already traced the error
      if (fmt_ptr != NULL)
      {
        try
        {
          fwrite(fmt_trace.str().c_str(), strlen(fmt_trace.str().c_str()), 1, output);
        }
        catch (...)
        {
          fprintf(output, "<corrupted trace>");
        }
      }
    }

#if ATTEMPTING_TO_PRINT_STRING_CONTENTS
    // If we had any strings, print their pointer values out as these will
    // often be more useful than the string values themselves (which might
    // have been overwritten).
    if (!strings.empty())
    {
      fprintf(output, ":string pointers");

      for (std::vector<void*>::iterator iter = strings.begin();
           iter != strings.end();
           ++iter)
      {
        fprintf(output, ",%p", *iter);
      }
    }
#endif

    // LF terminate the output line
    fprintf(output, "\n");

    // Now find the next trace entry
    this_entry = (TRC_RAMTRC_ENTRY *)((char *)this_entry + this_entry->entry_size);

    // Is it time to wrap?
    if (this_entry != Log::ram_next_slot)
    {
      if ((((char *)this_entry + RAM_SIZE(0)) > (Log::ram_buffer.buf + RAM_BUFSIZE)) ||
          (this_entry->trc_id == 0))
      {
        // Too near the end of the RAM trace buffer.  It must be time to wrap
        if (wrapped)
        {
          // We've already wrapped.  The buffer must be corrupt
          fprintf(output, "!!!Already wrapped. RAM trace buffer is corrupt!!!\n");
          return;
        }

        wrapped = true;
        this_entry = &Log::ram_buffer.align;
      }
    }
  }

  if (this_entry != Log::ram_next_slot)
  {
    // We ran off the end of the buffer rather than finding the tail.  Warn the
    // caller that the trace buffer is corrupted
    fprintf(output, "!!!Run off the end of the RAM trace buffer.  Trace corrupted!!!\n");
    return;
  }
}

void Log::setLoggingLevel(int level)
{
  if (level > DEBUG_LEVEL)
  {
    level = DEBUG_LEVEL;
  }
  else if (level < ERROR_LEVEL)
  {
    level = ERROR_LEVEL; // LCOV_EXCL_LINE
  }
  Log::loggingLevel = level;
}

// Note that the caller is responsible for deleting the previous
// Logger if it is allocated on the heap.

// Returns the previous Logger (e.g. so it can be stored off and reset).
Logger* Log::setLogger(Logger *log)
{
  pthread_mutex_lock(&Log::serialization_lock);
  Logger* old = Log::logger;
  Log::logger = log;
  if (Log::logger != NULL)
  {
    Log::logger->set_flags(Logger::FLUSH_ON_WRITE|Logger::ADD_TIMESTAMPS);
  }
  pthread_mutex_unlock(&Log::serialization_lock);
  return old;
}

void Log::write(int level, const char *module, int line_number, const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  _write(level, module, line_number, fmt, args);
  va_end(args);
}

static void release_lock(void* notused) { pthread_mutex_unlock(&Log::serialization_lock); } // LCOV_EXCL_LINE

void Log::_write(int level, const char *module, int line_number, const char *fmt, va_list args)
{
  if (level > Log::loggingLevel)
  {
    return;
  }

  pthread_mutex_lock(&Log::serialization_lock);
  if (!Log::logger)
  {
    // LCOV_EXCL_START
    pthread_mutex_unlock(&Log::serialization_lock);
    return;
    // LCOV_EXCL_STOP
  }

  pthread_cleanup_push(release_lock, 0);

  char logline[MAX_LOGLINE];

  int written = 0;
  int truncated = 0;

  const char* mod = strrchr(module, '/');
  module = (mod != NULL) ? mod + 1 : module;

  pthread_t thread = pthread_self();

  if (line_number)
  {
    written = snprintf(logline, MAX_LOGLINE - 2, "[%lx] %s %s:%d: ", thread, log_level[level], module, line_number);
  }
  else
  {
    written = snprintf(logline, MAX_LOGLINE - 2, "[%lx] %s %s: ", thread, log_level[level], module);
  }

  // snprintf and vsnprintf return the bytes that would have been
  // written if their second argument was large enough, so we need to
  // reduce the size of written to compensate if it is too large.
  written = std::min(written, MAX_LOGLINE - 2);

  int bytes_available = MAX_LOGLINE - written - 2;
  written += vsnprintf(logline + written, bytes_available, fmt, args);

  if (written > (MAX_LOGLINE - 2))
  {
    truncated = written - (MAX_LOGLINE - 2);
    written = MAX_LOGLINE - 2;
  }

  // Add a new line and null termination.
  logline[written] = '\n';
  logline[written+1] = '\0';

  Log::logger->write(logline);
  if (truncated > 0)
  {
    char buf[128];
    snprintf(buf, 128, "Previous log was truncated by %d characters\n", truncated);
    Log::logger->write(buf);
  }
  pthread_cleanup_pop(0);
  pthread_mutex_unlock(&Log::serialization_lock);

}

// LCOV_EXCL_START Only used in exceptional signal handlers - not hit in UT

void Log::backtrace(const char *fmt, ...)
{
  if (!Log::logger)
  {
    return;
  }

  va_list args;
  char logline[MAX_LOGLINE];
  va_start(args, fmt);
  // snprintf and vsnprintf return the bytes that would have been
  // written if their second argument was large enough, so we need to
  // reduce the size of written to compensate if it is too large.
  int written = vsnprintf(logline, MAX_LOGLINE - 2, fmt, args);
  written = std::min(written, MAX_LOGLINE - 2);
  va_end(args);

  // Add a new line and null termination.
  logline[written] = '\n';
  logline[written+1] = '\0';

  Log::logger->backtrace_simple(logline);
}

void Log::backtrace_adv()
{
  if (!Log::logger)
  {
    return;
  }

  Log::logger->backtrace_advanced();
}

void Log::commit()
{
  if (!Log::logger)
  {
    return;
  }

  Log::logger->commit();
}

// LCOV_EXCL_STOP
