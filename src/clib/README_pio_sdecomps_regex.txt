Introduction
--------------
PIO supports saving I/O decompositions specified by the user using simple regular expressions. The user can filter the I/O decompositions to be saved by,
1. I/O decomposition id : The unique id assigned to the decomposition by PIO
2. Variable name : The decomposition used to read/write the variable is written out
3. File name : Decompositions used to read/write variables in the file is written out.

The regular expression specified by the user could combine the above filters into a single regular expression using logical not, and, or operators.

The user needs to specify the regular expression to filter the I/O decompositions to be saved while configuring PIO. There are two PIO configure options that control the I/O decompositions saved by PIO,

1. PIO_SAVE_DECOMPS : If this option is set to "ON", PIO will save I/O decompositions into individual text files
2. PIO_SAVE_DECOMPS_REGEX : If PIO_SAVE_DECOMPS is set to "ON", this option can be optionally used to specify a regular expression to filter the I/O decompositions to be saved. If this option is not specified and PIO_SAVE_DECOMPS is set to "ON", all I/O decompositions are saved by PIO.

Regular Expression Filters
----------------------------
As mentioned above PIO supports filtering I/O decompositions to be saved using the decomposition id, variable name or file name.

1. Filtering using I/O decomposition id : A regular expression of the form

    '(ID=\"526\")'

    can be used to filter out decompositions for a specific decomposition id (in this case id=526). Note that instead of a number the ID filter could also be a regular expression that specifies multiple ioids (e.g. '(ID=\"52.*\")' to save all decompositions with id that starts with '52').

2. Filtering using Variable names : A regular expression of the form

    '(VAR=\".*test_var1.*\")'

    can be used to filter out decompositions for a set of variables. In the example above all decompositions associated with variables with variable names that include 'test_var1' is written out to disk.

3. Filtering using File names : A regular expression of the form

    '(FILE=\".*testfile1\")'

    can be used to filter out decompositions used to write variables to a file with name specified by a regular expression. In the example above all decompositions used to read/write variables to file with name ending in testfile1 is written out to disk

4. Combining multiple filters : Multiple regular expressions can be combined using logical not/and/or to create a more complex regular expression to filter out the I/O decompositions to be saved. For example,

    '(ID=\"526\")|| ((VAR=\".*test_var1.*\")&&(FILE=\".*testfile1\"))'

    can be used to save I/O decompositions that either have 526 as the id or are used to read/write variables with names that include "test_var1" into files with names that end in "testfile1"


A simple example
------------------
To save decompositions using the option 4 ('(ID=\"526\")|| ((VAR=\".*test_var1.*\")&&(FILE=\".*testfile1\"))') above configure PIO as described below,

cmake \
...
-DPIO_SAVE_DECOMPS:BOOL=ON \
-DPIO_SAVE_DECOMPS_REGEX:STRING='(ID=\"526\")|| ((VAR=\".*test_var1.*\")&&(FILE=\".*testfile1\"))' \
...


