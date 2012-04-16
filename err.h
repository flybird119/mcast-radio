#ifndef _ERR_
#define _ERR_

#define TRY_SYS(code) if ((code) < 0) syserr(#code"; on line %d;", __LINE__)
#define TRY_TRUE(code) if (!(code)) syserr(#code"; on line %d;", __LINE__)
#define ASSERT(code) if (!(code)) fatal(#code"; on line %d;", __LINE__)
#define EXPECT(code, msg) if (!(code)) fprintf(stderr, "WARNING: "msg"\n")

/* wypisuje informacje o blędnym zakonczeniu funkcji systemowej i kończy działanie */
extern void syserr(const char *fmt, ...);

/* wypisuje informacje o blędzie i kończy działanie */
extern void fatal(const char *fmt, ...);

#endif
