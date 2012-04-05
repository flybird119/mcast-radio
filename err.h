#ifndef _ERR_
#define _ERR_

#define TRY_ZERO(code) if ((code) < 0) syserr(#code"; on line %d;", __LINE__)
#define TRY_TRUE(code) if (!(code)) fatal(#code"; on line %d;", __LINE__)
#define ASSERT(code) if (!(code)) fatal(#code"; on line %d;", __LINE__)

// TODO
#define debug(args) fprintf(stderr, args)

/* wypisuje informacje o blędnym zakonczeniu funkcji systemowej i kończy działanie */
extern void syserr(const char *fmt, ...);

/* wypisuje informacje o blędzie i kończy działanie */
extern void fatal(const char *fmt, ...);

#endif
