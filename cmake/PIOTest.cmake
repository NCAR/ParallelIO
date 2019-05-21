include (LibMPI)

#============================================================================
# Add PIO tests
#
# Adds a serial (mpi-serial) or multiple parallel MPI tests
# 
# When using the mpi-serial library only 1 serial test is added
#
# When using an MPI library parallel tests are added for the ranges 
# of MPI processes, I/O processes and strides specified by the user.
# The number of MPI processes are scaled linearly (+1) for the range
# [MINNUMPROCS, 4] and then scaled exponentially (power of 2)
# The number of I/O processes are scaled exponentially (power of 2) for
# the ranges specified by the user
#
# e.g. : For MINNUMPROCS=2, MAXNUMPROCS=18, MINNUMIOPROCS=1,
#   MAXNUMIOPROCS=4, MINSTRIDE=1, MAXSTRIDE=4 the function creates
#   testname_np[2,3,4]_nio[1,2,4]_st[1,2,4] &
#   testname_np[8,16]_nio[1,2,4]_st[1,2,4] tests
#
# Syntax:  add_pio_test (<TESTNAME>
#                        EXECUTABLE <command>
#                        ARGUMENTS <arg1> <arg2> ...
#                        MINNUMPROCS <min_num_procs>
#                        MAXNUMPROCS <max_num_procs>
#                        MINNUMIOPROCS <min_num_io_procs>
#                        MAXNUMIOPROCS <max_num_io_procs>
#                        MINSTRIDE <min_stride>
#                        MAXSTRIDE <max_stride>
#                        TIMEOUT <timeout>)
function (add_pio_test TESTNAME)

    # Parse the input arguments
    set (options)
    set (oneValueArgs EXECUTABLE MINNUMPROCS MAXNUMPROCS MINNUMIOPROCS MAXNUMIOPROCS MINSTRIDE MAXSTRIDE TIMEOUT)
    set (multiValueArgs ARGUMENTS)
    cmake_parse_arguments (${TESTNAME} "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    
    # Store parsed arguments for convenience
    set (exec_file ${${TESTNAME}_EXECUTABLE})
    set (exec_args ${${TESTNAME}_ARGUMENTS})
    set (min_num_procs ${${TESTNAME}_MINNUMPROCS})
    set (max_num_procs ${${TESTNAME}_MAXNUMPROCS})
    set (min_num_io_procs ${${TESTNAME}_MINNUMIOPROCS})
    set (max_num_io_procs ${${TESTNAME}_MAXNUMIOPROCS})
    set (min_stride ${${TESTNAME}_MINSTRIDE})
    set (max_stride ${${TESTNAME}_MAXSTRIDE})
    set (timeout ${${TESTNAME}_TIMEOUT})

    # Sanity checks on the user args
    if (min_num_procs GREATER max_num_procs)
      message(FATAL_ERROR "Invalid arguments, MINNUMPROCS (${min_num_procs}) > MAXNUMPROCS (${max_num_procs})")
    endif ()
    if (min_num_io_procs GREATER max_num_io_procs)
      message(FATAL_ERROR "Invalid arguments, MINNUMIOPROCS (${min_num_io_procs}) > MAXNUMIOPROCS (${max_num_io_procs})")
    endif ()
    if (min_stride GREATER max_stride)
      message(FATAL_ERROR "Invalid arguments, MINSTRIDE (${min_stride}) > MAXSTRIDE (${max_stride})")
    endif ()
    
    # Add tests
    if (PIO_USE_MPISERIAL)
      # For serial tests we ignore the number of procs (its always 1),
      # number of I/O processes (always 1) and the stride (always 1)
      # Add a test (serial) to CTest
      add_test (NAME ${TESTNAME}_1proc COMMAND ${exec_file} ${exec_args})
      
      # Adjust the test timeout
      set_tests_properties (${TESTNAME}_1proc PROPERTIES TIMEOUT ${timeout})

    else () # if (PIO_USE_MPISERIAL)

      #message(STATUS "Adding parallel tests [ ${min_num_procs} .. ${max_num_procs}]")
      # Linear scaling (for number of MPI procs) upper bound
      # We scale MPI procs linearly (+1) till lscale_ub and then 
      # exponentially (power 2)
      set (lscale_ub "4")

      set (inprocs ${min_num_procs})

      if (min_num_procs GREATER lscale_ub)
        # Scale the number of procs by power of 2 until we find
        # a number, that is power of 2, greater than min_num_procs
        set (inprocs ${lscale_ub})
        while (min_num_procs GREATER inprocs)
          math (EXPR inprocs "${inprocs} * 2")
        endwhile () # while (min_num_procs ...)
      endif () # if (min_num_procs GREATER ...)

      while ((inprocs LESS max_num_procs)
              OR (inprocs EQUAL max_num_procs))
        # Add tests for io procs in [min_num_io_procs, max_num_io_procs]
        # scaling the io procs exponentially (power 2)
        set (inioprocs ${min_num_io_procs})
        while ((inioprocs LESS max_num_io_procs) OR
              (inioprocs EQUAL max_num_io_procs))
          # We cannot allocate more I/O procs than the number of MPI procs
          if (inioprocs GREATER inprocs)
            break ()
          endif ()

          set (istride ${min_stride})
          while ((istride LESS max_stride) OR
                (istride EQUAL max_stride))
            # Maximum stride possible between inioprocs is
            # No of MPI procs / (No of IO procs - 1)
            # FIXME: We assume root == 0
            if (inioprocs EQUAL "1")
              # Only 1 stride, stride == 1, is available with 1 I/O process
              set (max_istride "1")
            else ()
              math (EXPR max_istride "(${inprocs} - 1)/ (${inioprocs} - 1)")
            endif ()
            if (istride GREATER max_istride)
              break ()
            endif ()

            # Add an MPI test (parallel)
            add_mpi_test (${TESTNAME}_np${inprocs}_nio${inioprocs}_st${istride}
              EXECUTABLE ${exec_file}
              ARGUMENTS ${exec_args} --pio-tf-num-io-tasks=${inioprocs} --pio-tf-stride=${istride}
              NUMPROCS ${inprocs}
              TIMEOUT ${timeout})

            math (EXPR istride "${istride} * 2")
          endwhile () # while ((istride ...)

          math (EXPR inioprocs "${inioprocs} * 2")
        endwhile () # while ((inioprocs...)

        if (inprocs LESS lscale_ub)
          # Scale number of MPI procs linearly (+1)
          math (EXPR inprocs "${inprocs} + 1")
        else ()
          # Scale number of MPI procs by power of 2
          math (EXPR inprocs "${inprocs} * 2")
        endif ()
      endwhile () # while((inprocs...)

    endif () # if (PIO_USE_MPISERIAL)

endfunction()
