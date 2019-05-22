# This is part of PIO. It creates the doc Makefile.

# Ed Hartnett 4/1/19

EXTRA_DIST = CMakeLists.txt customdoxygen.css Doxyfile.in	\
DoxygenLayout.xml doxygen.sty

# Run doxygen, then confirm warning log file is empty.
check: all
all:
	doxygen Doxyfile
	ls -l doxywarn.log
	cat doxywarn.log
	[ ! -s doxywarn.log ]

CLEANFILES = *.log
