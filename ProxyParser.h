/*
 * proxy_parse.h -- a HTTP Request Parsing Library.
 *
 * Written by: Matvey Arye
 * For: COS 518
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

#include <ctype.h>

#ifndef PROXY_PARSE
#define PROXY_PARSE

#define DEBUG 1

/*
   ParsedRequest objects are created from parsing a buffer containing a HTTP
   request. The request buffer consists of a request line followed by a number
   of headers. Request line fields such as method, protocol etc. are stored
   explicitly. Headers such as 'Content-Length' and their values are maintained
   in a linked list. Each node in this list is a ParsedHeader and contains a
   key-value pair.

   The buf and buflen fields are used internally to maintain the parsed request
   line.
 */
struct ParsedRequest
{
  char *method;
  char *protocol;
  char *host;
  char *port;
  char *path;
  char *version;
  char *buffer;
  size_t bufferLength;
  struct ParsedHeader *headers;
  size_t headersUsed;
  size_t headersLength;
};

/*
   ParsedHeader: any header after the request line is a key-value pair with the
   format "key:value\r\n" and is maintained in the ParsedHeader linked list
   within ParsedRequest
*/
struct ParsedHeader
{
  char *key;
  size_t keyLength;
  char *value;
  size_t valueLength;
};

/* Create an empty parsing object to be used exactly once for parsing a single
 * request buffer */
struct ParsedRequest *parsedRequestCreate();

/* Parse the request buffer in buf given that buf is of length bufferLength */
int parsedRequestParse(struct ParsedRequest *parse, const char *buffer,
                       int bufferLength);

/* Destroy the parsing object. */
void parsedRequestDestroy(struct ParsedRequest *pr);

/*
   Retrieve the entire buffer from a parsed request object. buffer must be an
   allocated buffer of size bufferLength, with enough space to write the request
   line, headers and the trailing \r\n. buffer will not be NUL terminated by
   unparse().
 */
int parsedRequestUnparse(struct ParsedRequest *pr, char *buffer,
                         size_t bufferLength);

/*
   Retrieve the entire buffer with the exception of request line from a parsed
   request object. buffer must be an allocated buffer of size bufferLength, with enough
   space to write the headers and the trailing \r\n. buffer will not be NUL
   terminated by unparse(). If there are no headers, the trailing \r\n is
   unparsed.
 */
int parsedRequestUnparseHeaders(struct ParsedRequest *pr, char *buffer,
                                size_t bufferLength);

/* Total length including request line, headers and the trailing \r\n*/
size_t parsedRequestTotalLen(struct ParsedRequest *pr);

/* Length including headers, if any, and the trailing \r\n but excluding the
 * request line.
 */
size_t parsedHeadersLen(struct ParsedRequest *pr);

/* Set, get, and remove null-terminated header keys and values */
int parsedHeaderSet(struct ParsedRequest *pr, const char *key,
                    const char *value);
struct ParsedHeader *parsedHeaderGet(struct ParsedRequest *pr,
                                     const char *key);
int parsedHeaderRemove(struct ParsedRequest *pr, const char *key);

/* debug() prints out debugging info if DEBUG is set to 1 */
void debug(const char *format, ...);

#endif
