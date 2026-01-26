
set(TEMPSRCF90 alloc_mod.F90)
foreach(tempfile IN LISTS TEMPSRCF90)
  add_custom_command(
    OUTPUT ${tempfile}
    COMMAND ${GENF90_PATH}/genf90.pl ${CMAKE_CURRENT_SOURCE_DIR}/${tempfile}.in > ${tempfile}
    DEPENDS ${tempfile}.in
  )
endforeach()

set(SRC check_mod.F90 gdecomp_mod.F90 kinds_mod.F90 namelist_mod.F90
  testpio.F90 utils_mod.F90 ${TEMPSRCF90})
set(WSSRC wstest.c)

include_directories(${PIO_INCLUDE_DIRS})
link_directories(${PIO_LIB_DIR})
add_executable(testpio ${SRC})
add_executable(wstest ${WSSRC})
if(${PIO_BUILD_TIMING} MATCHES "ON")
  set(TIMING_LINK_LIB timing)
endif()

if(NETCDF_FOUND)
  target_link_libraries(testpio piof pioc ${NETCDF_Fortran_LIBRARY} ${TIMING_LINK_LIB})
else()
  target_link_libraries(testpio piof pioc ${TIMING_LINK_LIB})
endif()
target_link_libraries(wstest pioc ${TIMING_LINK_LIB})
