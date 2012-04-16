# versatile Makefile
CC          := gcc -g
CFLAGS      := -Wall -Wextra -pedantic -std=gnu11
LDFLAGS     := -Wall -Wextra -pedantic -std=gnu11 -levent

# C sources
SOURCES     := $(wildcard *.c)
# objects containing main() definition
MAINOBJECTS := $(subst .c,.o,$(shell grep -l main $(SOURCES)))
# executables (linked from MAINOBJECTS)
ALL         := $(subst .o,,$(MAINOBJECTS))
# submakefiles
DEPENDS     := $(subst .c,.d,$(SOURCES))
# all objects
ALLOBJECTS  := $(subst .c,.o,$(SOURCES))
# objects not containing main() definition
OBJECTS	    := $(filter-out $(MAINOBJECTS),$(ALLOBJECTS)) 

all: $(DEPENDS) $(ALL)

# create submakefiles (s/-e:/-e/ makes things work)
$(DEPENDS) : %.d : %.c
	$(CC) -MM $< > $@
	@echo -e "\t"$(CC) -c $(CFLAGS) $< >> $@

# link objects
$(ALL) : % : %.o $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $^		

# include submakefiles
-include $(DEPENDS)

clean:
	-rm -f *.o $(ALL) $(ALLOBJECTS) $(DEPENDS)

