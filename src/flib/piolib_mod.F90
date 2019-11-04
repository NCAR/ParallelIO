#define __PIO_FILE__ "piolib_mod.f90"
#define debug_rearr 0

!>
!! @file
!! @brief Initialization Routines for PIO
!!
!<
module piolib_mod
  use iso_c_binding
  !--------------
  use pio_kinds
  !--------------
  use pio_types, only : file_desc_t, iosystem_desc_t, var_desc_t, io_desc_t, &
        pio_iotype_netcdf, pio_iotype_pnetcdf, pio_iotype_netcdf4p, pio_iotype_netcdf4c, &
        pio_noerr, pio_rearr_subset, pio_rearr_box, pio_rearr_opt_t
  !--------------
  use pio_support, only : piodie, debug, debugio, debugasync, checkmpireturn
  use pio_nf, only : pio_set_log_level
  !


#ifdef TIMING
  use perf_mod, only : t_startf, t_stopf     ! _EXTERNAL
#endif
#ifndef NO_MPIMOD
  use mpi    ! _EXTERNAL
#endif
  implicit none
  private
#ifdef NO_MPIMOD
  include 'mpif.h'    ! _EXTERNAL
#endif
  ! !public member functions:

  public :: PIO_init,     &
       PIO_finalize,      &
       PIO_initdecomp,    &
       PIO_openfile,      &
       PIO_syncfile,      &
       PIO_createfile,    &
       PIO_closefile,     &
       PIO_setframe,      &
       PIO_advanceframe,  &
       PIO_setdebuglevel, &
       PIO_seterrorhandling, &
       PIO_get_local_array_size, &
       PIO_freedecomp,     &
       PIO_getnumiotasks, &
       PIO_set_hint,      &
       PIO_FILE_IS_OPEN, &
       PIO_deletefile, &
       PIO_get_numiotasks, &
       PIO_iotype_available, &
       PIO_set_rearr_opts

#ifdef MEMCHK
!> this is an internal variable for memory leak debugging
!! it is used when macro memchk is defined and it causes each task to print the
!! memory resident set size anytime it changes within pio.
!<
  integer :: lastrss=0
#endif

  !eop
  !boc
  !-----------------------------------------------------------------------
  !
  !  module variables
  !
  !-----------------------------------------------------------------------
!>
!! @defgroup PIO_openfile PIO_openfile
!<
  interface PIO_openfile
     module procedure PIO_openfile
  end interface

!>
!! @defgroup PIO_syncfile PIO_syncfile
!<
  interface PIO_syncfile
     module procedure syncfile
  end interface

!>
!! @defgroup PIO_createfile PIO_createfile
!<
  interface PIO_createfile
     module procedure createfile
  end interface

!>
!! @defgroup PIO_setframe PIO_setframe
!! @brief sets the unlimited dimension for netcdf file
!<
  interface PIO_setframe
     module procedure setframe
  end interface

!>
!! @defgroup PIO_advanceframe PIO_advanceframe
!<
  interface PIO_advanceframe
     module procedure advanceframe
  end interface

!>
!! @defgroup PIO_closefile PIO_closefile
!<
  interface PIO_closefile
     module procedure closefile
  end interface


!>
!! @defgroup PIO_freedecomp PIO_freedecomp
!! free memory associated with a io descriptor
!<
  interface PIO_freedecomp
     module procedure freedecomp_ios
     module procedure freedecomp_file
  end interface

!>
!! @defgroup PIO_init PIO_init
!! initializes the pio subsystem
!<
  interface PIO_init
     module procedure init_intracom
     module procedure init_intercom

  end interface

!>
!! @defgroup PIO_finalize PIO_finalize
!! Shuts down and cleans up any memory associated with the pio library.
!<
  interface PIO_finalize
     module procedure finalize
  end interface

!>
!! @defgroup PIO_initdecomp PIO_initdecomp
!! @brief PIO_initdecomp is an overload interface the models decomposition to pio.
!! @details initdecomp_1dof_bin_i8, initdecomp_1dof_nf_i4, initdecomp_2dof_bin_i4,
!! and initdecomp_2dof_nf_i4 are all depreciated, but supported for backwards
!! compatibility.
!<
  interface PIO_initdecomp
     module procedure PIO_initdecomp_dof_i4  ! previous name: initdecomop_1dof_nf_box
     module procedure PIO_initdecomp_dof_i8  ! previous name: initdecomop_1dof_nf_box
     module procedure PIO_initdecomp_bc
!     module procedure initdecomp_1dof_nf_i4
!     module procedure initdecomp_1dof_nf_i8
!     module procedure initdecomp_1dof_bin_i4
!     module procedure initdecomp_1dof_bin_i8
!     module procedure initdecomp_2dof_nf_i4
!     module procedure initdecomp_2dof_nf_i8
!     module procedure initdecomp_2dof_bin_i4
!     module procedure initdecomp_2dof_bin_i8
!     module procedure PIO_initdecomp_dof_dof
  end interface

!>

!>
!! @defgroup PIO_getnumiotasks PIO_getnumiotasks
!!  returns the actual number of IO-tasks used.  PIO
!!  will reset the total number of IO-tasks if certain
!!  conditions are meet
!<
  interface PIO_get_numiotasks
     module procedure getnumiotasks
  end interface
  interface PIO_getnumiotasks
     module procedure getnumiotasks
  end interface

!>
!!  @defgroup PIO_setdebuglevel PIO_setdebuglevel
!!  sets the level of debug information that pio will generate.
!<
  interface PIO_setdebuglevel
     module procedure setdebuglevel
  end interface

!>
!!  @defgroup PIO_seterrorhandling PIO_seterrorhandling
!!  sets the form of error handling for pio.
!!
!! By default pio handles errors internally by printing a string
!! describing the error and calling mpi_abort.  Application
!! developers can change this behavior for calls to the underlying netcdf
!! libraries with a call to PIO_seterrorhandling. For example if a
!! developer wanted to see if an input netcdf format file contained the variable
!! 'u' they might write the following
!! @verbinclude errorhandle
!<
  interface PIO_seterrorhandling
     module procedure seterrorhandlingfile
     module procedure seterrorhandlingiosystem
     module procedure seterrorhandlingiosysid
  end interface

!>
!! @defgroup PIO_get_local_array_size PIO_get_local_array_size
!<

  !eoc
  !***********************************************************************

contains

!!$#ifdef __GFORTRAN__
!!$    pure function fptr ( inArr ) result ( ptr )
!!$        integer (PIO_OFFSET_KIND), dimension(:), target, intent(in) :: inArr
!!$        integer (PIO_OFFSET_KIND), target :: ptr
!!$        ptr = inArr(1)
!!$    end function fptr
!!$#elif CPRNAG
!!$! no-op -- nothing here for nag.
!!$#else
#define fptr(arg) arg
!!$#endif

!>
!! @public
!! @ingroup PIO_file_is_open
!! @brief This logical function indicates if a file is open.
!! @details
!! @param File @copydoc file_desc_t
!<
  logical function PIO_FILE_IS_OPEN(File)
    type(file_desc_t), intent(in) :: file
    interface
       integer(C_INT) function PIOc_File_is_Open(ncid) &
            bind(C,NAME="PIOc_File_is_Open")
         use iso_c_binding
         implicit none
         integer(c_int), value :: ncid
       end function PIOc_File_is_Open
    end interface
    PIO_FILE_IS_OPEN = .false.
    if(associated(file%iosystem)) then
      if(PIOc_File_is_Open(file%fh)==1) then
        PIO_FILE_IS_OPEN = .true.
      endif
    endif

  end function PIO_FILE_IS_OPEN

!>
!! @public
!! @ingroup PIO_get_local_array_size
!! @brief This function returns the expected local size of an array associated with iodesc
!! @details
!! @param iodesc
!! @copydoc io_desc_t
!<
  integer function PIO_get_local_array_size(iodesc)
    type(io_desc_t), intent(in) :: iodesc
    interface
       integer(C_INT) function PIOc_get_local_array_size(ioid) &
            bind(C,NAME="PIOc_get_local_array_size")
         use iso_c_binding
         implicit none
         integer(C_INT), value :: ioid
       end function PIOc_get_local_array_size
    end interface
    PIO_get_local_array_size = PIOc_get_local_array_size(iodesc%ioid)
  end function PIO_get_local_array_size

!>
!! @public
!! @ingroup PIO_advanceframe
!! @brief advances the record dimension of a variable in a netcdf format file
!!  or the block address in a binary file
!! @details
!! @param[in,out] vardesc @copybrief var_desc_t
!<
  subroutine advanceframe(file, vardesc)
    type(file_desc_t), intent(in) :: file
    type(var_desc_t), intent(inout) :: vardesc
    integer ierr;
    interface
       integer(C_INT) function PIOc_advanceframe(fileid, varid) &
            bind(C,NAME="PIOc_advanceframe")
         use iso_c_binding
         implicit none
         integer(C_INT), value :: fileid
         integer(C_INT), value :: varid
       end function PIOc_advanceframe
    end interface
    ierr = PIOc_advanceframe(file%fh, vardesc%varid-1)
  end subroutine advanceframe

!>
!! @public
!! @ingroup PIO_setframe
!! @brief sets the record dimension of a variable in a netcdf format file
!! or the block address in a binary file
!! @details
!! @param vardesc @copydoc var_desc_t
!! @param frame   : frame number to set
!<
  subroutine setframe(file, vardesc,frame)
    type(file_desc_t) :: file
    type(var_desc_t), intent(inout) :: vardesc
    integer(PIO_OFFSET_KIND), intent(in) :: frame
    integer :: ierr, iframe
    interface
       integer(C_INT) function PIOc_setframe(ncid, varid, frame) &
            bind(C,NAME="PIOc_setframe")
         use iso_c_binding
         implicit none
         integer(C_INT), value :: ncid
         integer(C_INT), value :: varid
         integer(C_INT), value :: frame
       end function PIOc_setframe
    end interface
    iframe = int(frame-1)
    ierr = PIOc_setframe(file%fh, vardesc%varid-1, iframe)
  end subroutine setframe

!>
!! @public
!! @ingroup PIO_setdebuglevel
!! @brief sets the level of debug information output to stdout by pio
!! @details
!! @param level : default value is 0, allowed values 0-6
!<
  subroutine setdebuglevel(level)
    integer(i4), intent(in) :: level
    integer :: ierr
    if(level.eq.0) then
       debug=.false.
       debugio=.false.
       debugasync=.false.
    else if(level.eq.1) then
       debug=.true.
       debugio=.false.
       debugasync=.false.
    else if(level.eq.2) then
       debug=.false.
       debugio=.true.
       debugasync=.false.
    else if(level.eq.3) then
       debug=.true.
       debugio=.true.
       debugasync=.false.
    else if(level.eq.4) then
       debug=.false.
       debugio=.false.
       debugasync=.true.
    else if(level.eq.5) then
       debug=.true.
       debugio=.false.
       debugasync=.true.
    else if(level.ge.6) then
       debug=.true.
       debugio=.true.
       debugasync=.true.
    end if
    ierr = PIO_set_log_level(level)
    if(ierr /= PIO_NOERR) then
      ! This is not a fatal error
      print *, __PIO_FILE__, __LINE__, "Setting log level failed, ierr =",ierr
    end if
  end subroutine setdebuglevel

!>
!! @ingroup PIO_seterrorhandling
!! @public
!! @brief set the pio error handling method for a file
!!
!! @param file @copydoc file_desc_t
!! @param method :
!! @copydoc PIO_error_method
!<
  subroutine seterrorhandlingfile(file, method, oldmethod)
    type(file_desc_t), intent(inout) :: file
    integer, intent(in) :: method
    integer, intent(out), optional :: oldmethod
    call seterrorhandlingiosysid(file%iosystem%iosysid, method, oldmethod)
  end subroutine seterrorhandlingfile

!>
!! @ingroup PIO_seterrorhandling
!! @public
!! @brief set the pio error handling method for a pio system
!! @param iosystem : a defined pio system descriptor, see PIO_types
!! @param method :
!! @copydoc PIO_error_method
!<
  subroutine seterrorhandlingiosystem(iosystem, method, oldmethod)
    type(iosystem_desc_t), intent(inout) :: iosystem
    integer, intent(in) :: method
    integer, intent(out), optional :: oldmethod
    call seterrorhandlingiosysid(iosystem%iosysid, method, oldmethod)
  end subroutine seterrorhandlingiosystem

!>
!! @ingroup PIO_seterrorhandling
!! @public
!! @brief set the pio error handling method for a pio system or globally
!! @param iosysid : a pio system ID (pass PIO_DEFAULT to change the global default error handling)
!! @param method :
!! @copydoc PIO_error_method
!<
  subroutine seterrorhandlingiosysid(iosysid, method, oldmethod)
    integer, intent(in) :: iosysid
    integer, intent(in) :: method
    integer, intent(out), optional :: oldmethod

    interface
       integer(c_int) function PIOc_Set_IOSystem_Error_Handling(iosysid, method) &
            bind(C,name="PIOc_Set_IOSystem_Error_Handling")
         use iso_c_binding
         integer(c_int), value :: iosysid
         integer(c_int), value :: method
       end function PIOc_Set_IOSystem_Error_Handling
    end interface
    integer(c_int) ::  loldmethod

    loldmethod = PIOc_Set_IOSystem_Error_Handling(iosysid, method)
    if(present(oldmethod)) oldmethod = loldmethod

  end subroutine seterrorhandlingiosysid


!>
!! @public
!! @ingroup PIO_initdecomp
!! @brief Implements the @ref decomp_bc for PIO_initdecomp
!! @details  This provides the ability to describe a computational
!! decomposition in PIO that has a block-cyclic form.  That is
!! something that can be described using start and count arrays.
!! Optional parameters for this subroutine allows for the specification
!! of io decomposition using iostart and iocount arrays.  If iostart
!! and iocount arrays are not specified by the user, and rearrangement
!! is turned on then PIO will calculate a suitable IO decomposition
!! @param iosystem @copydoc iosystem_desc_t
!! @param basepiotype @copydoc use_PIO_kinds
!! @param dims An array of the global length of each dimesion of the variable(s)
!! @param compstart The start index into the block-cyclic computational decomposition
!! @param compcount The count for the block-cyclic computational decomposition
!! @param iodesc @copydoc iodesc_generate
!<
  subroutine PIO_initdecomp_bc(iosystem,basepiotype,dims,compstart,compcount,iodesc)
    type (iosystem_desc_t), intent(inout) :: iosystem
    integer(i4), intent(in)               :: basepiotype
    integer(i4), intent(in)               :: dims(:)
    integer (kind=PIO_OFFSET_KIND)             :: compstart(:)
    integer (kind=PIO_OFFSET_KIND)             :: compcount(:)
    type (IO_desc_t), intent(out)         :: iodesc

    interface
       integer(C_INT) function PIOc_InitDecomp_bc(iosysid, basetype, ndims, dims, compstart, compcount, ioidp) &
            bind(C,name="PIOc_InitDecomp_bc")
         use iso_c_binding
         integer(C_INT), value :: iosysid
         integer(C_INT), value :: basetype
         integer(C_INT), value :: ndims
         integer(C_INT) :: dims(*)
         integer(C_INT) :: ioidp
         integer(C_SIZE_T) :: compstart(*)
         integer(C_SIZE_T) :: compcount(*)
       end function PIOc_InitDecomp_bc
    end interface
    integer :: i, ndims
    integer, allocatable ::  cdims(:)
    integer(PIO_Offset_kind), allocatable :: cstart(:), ccount(:)
    integer :: ierr

    ndims = size(dims)

    allocate(cstart(ndims), ccount(ndims), cdims(ndims))

    do i=1,ndims
       cdims(i) = dims(ndims-i+1)
       cstart(i)  = compstart(ndims-i+1)-1
       ccount(i)  = compcount(ndims-i+1)
    end do

    ierr = PIOc_InitDecomp_bc(iosystem%iosysid, basepiotype, ndims, cdims, &
         cstart, ccount, iodesc%ioid)


    deallocate(cstart, ccount, cdims)


  end subroutine PIO_initdecomp_bc

!>
!! @public
!! @ingroup PIO_initdecomp
!! @brief Implements the @ref decomp_dof for PIO_initdecomp (previous name: \b initdecomp_1dof_nf_box)
!! @details  This provides the ability to describe a computational
!! decomposition in PIO using degrees of freedom method. This is
!! a decomposition that can not be easily described using a start
!! and count method (see @ref decomp_dof).
!! Optional parameters for this subroutine allows for the specififcation of
!! io decomposition using iostart and iocount arrays.  If iostart
!! and iocount arrays are not specified by the user, and rearrangement
!! is turned on then PIO will calculate an suitable IO decomposition.
!! Note that this subroutine was previously called \em initdecomp_1dof_nf_box
!! @param iosystem @copydoc iosystem_desc_t
!! @param basepiotype @copydoc use_PIO_kinds
!! @param dims An array of the global length of each dimesion of the variable(s)
!! @param compdof Mapping of the storage order for the computational decomposition to its memory order
!! @param iodesc @copydoc iodesc_generate
!! @param iostart   The start index for the block-cyclic io decomposition
!! @param iocount   The count for the block-cyclic io decomposition
!<
  subroutine PIO_initdecomp_dof_i4(iosystem,basepiotype,dims,compdof, iodesc, rearr, iostart, iocount)
    type (iosystem_desc_t), intent(inout) :: iosystem
    integer(i4), intent(in)           :: basepiotype
    integer(i4), intent(in)          :: compdof(:)   ! global degrees of freedom for computational decomposition
    integer, optional, target :: rearr
    integer (PIO_OFFSET_KIND), optional :: iostart(:), iocount(:)
    type (io_desc_t), intent(inout)     :: iodesc
    integer(PIO_OFFSET_KIND), pointer :: internal_compdof(:)
    integer(i4), intent(in)           :: dims(:)

    allocate(internal_compdof(size(compdof)))
    internal_compdof = int(compdof,PIO_OFFSET_KIND)

    if(present(iostart) .and. present(iocount) ) then
       call pio_initdecomp_dof_i8(iosystem, basepiotype, dims, internal_compdof, iodesc, &
            PIO_REARR_SUBSET, iostart, iocount)
    else
       call pio_initdecomp_dof_i8(iosystem, basepiotype, dims, internal_compdof, iodesc, rearr)
    endif
    deallocate(internal_compdof)

  end subroutine PIO_initdecomp_dof_i4


  subroutine PIO_initdecomp_internal(iosystem,basepiotype,dims,maplen, compdof, iodesc, rearr, iostart, iocount)
    type (iosystem_desc_t), intent(in) :: iosystem
    integer(i4), intent(in)           :: basepiotype
    integer(i4), intent(in)           :: dims(:)
    integer, intent(in) :: maplen
    integer (PIO_OFFSET_KIND), intent(in) :: compdof(maplen)   ! global degrees of freedom for computational decomposition
    integer, optional, target :: rearr
    integer (PIO_OFFSET_KIND), optional :: iostart(:), iocount(:)
    type (io_desc_t), intent(inout)     :: iodesc

    integer(c_int) :: ndims
    integer(c_int), dimension(:), allocatable, target :: cdims
    integer(PIO_OFFSET_KIND), dimension(:), allocatable, target :: cstart, ccount

    type(C_PTR) :: crearr
    interface
       integer(C_INT) function PIOc_InitDecomp(iosysid,basetype,ndims,dims, &
            maplen, compmap, ioidp, rearr, iostart, iocount)  &
            bind(C,name="PIOc_InitDecomp")
         use iso_c_binding
         integer(C_INT), value :: iosysid
         integer(C_INT), value :: basetype
         integer(C_INT), value :: ndims
         integer(C_INT) :: dims(*)
         integer(C_INT), value :: maplen
         integer(C_SIZE_T) :: compmap(*)
         integer(C_INT) :: ioidp
         type(C_PTR), value :: rearr
         type(C_PTR), value :: iostart
         type(C_PTR), value :: iocount
       end function PIOc_InitDecomp
    end interface
    integer :: ierr,i

    ndims = size(dims)
    allocate(cdims(ndims))
    do i=1,ndims
       cdims(i) = dims(ndims-i+1)
    end do

    if(present(rearr)) then
       crearr = C_LOC(rearr)
    else
       crearr = C_NULL_PTR
    endif

    if(present(iostart) .and. present(iocount)) then
       allocate(cstart(ndims), ccount(ndims))
       do i=1,ndims
          cstart(i) = iostart(ndims-i+1)-1
          ccount(i) = iocount(ndims-i+1)
       end do

       ierr = PIOc_InitDecomp(iosystem%iosysid, basepiotype, ndims, cdims, &
            maplen, compdof, iodesc%ioid, crearr, C_LOC(cstart), C_LOC(ccount))
       deallocate(cstart, ccount)
    else
        ierr = PIOc_InitDecomp(iosystem%iosysid, basepiotype, ndims, cdims, &
            maplen, compdof, iodesc%ioid, crearr, C_NULL_PTR, C_NULL_PTR)
    end if

    deallocate(cdims)


  end subroutine PIO_initdecomp_internal


  subroutine PIO_initdecomp_dof_i8(iosystem,basepiotype,dims,compdof, iodesc, rearr, iostart, iocount)
    type (iosystem_desc_t), intent(in) :: iosystem
    integer(i4), intent(in)           :: basepiotype
    integer(i4), intent(in)           :: dims(:)
    integer (PIO_OFFSET_KIND), intent(in) :: compdof(:)   ! global degrees of freedom for computational decomposition
    integer, optional, target :: rearr
    integer (PIO_OFFSET_KIND), optional :: iostart(:), iocount(:)
    type (io_desc_t), intent(inout)     :: iodesc
    integer :: maplen

#ifdef TIMING
    call t_startf("PIO:initdecomp_dof")
#endif

    maplen = size(compdof)

    call PIO_initdecomp_internal(iosystem, basepiotype, dims, maplen, compdof, iodesc, rearr, iostart,iocount)


#ifdef TIMING
    call t_stopf("PIO:initdecomp_dof")
#endif

  end subroutine PIO_initdecomp_dof_i8

!>
!! @public
!! @ingroup PIO_init
!! @brief initialize the pio subsystem.
!! @details  This is a collective call.  Input parameters are read on comp_rank=0
!!   values on other tasks are ignored.  This variation of PIO_init locates the IO tasks on a subset
!!   of the compute tasks.
!! @param comp_rank mpi rank of each participating task,
!! @param comp_comm the mpi communicator which defines the collective.
!! @param num_iotasks the number of iotasks to define.
!! @param num_aggregator the mpi aggregator count
!! @param stride the stride in the mpi rank between io tasks.
!! @param rearr @copydoc PIO_rearr_method
!! @param iosystem a derived type which can be used in subsequent pio operations (defined in PIO_types).
!! @param base @em optional argument can be used to offset the first io task - default base is task 1.
!<
  subroutine init_intracom(comp_rank, comp_comm, num_iotasks, num_aggregator, stride,  rearr, iosystem,base, rearr_opts)
    use pio_types, only : pio_internal_error, pio_rearr_opt_t
    use iso_c_binding

    integer(i4), intent(in) :: comp_rank
    integer(i4), intent(in) :: comp_comm
    integer(i4), intent(in) :: num_iotasks
    integer(i4), intent(in) :: num_aggregator
    integer(i4), intent(in) :: stride
    integer(i4), intent(in) :: rearr
    type (iosystem_desc_t), intent(out)  :: iosystem  ! io descriptor to initalize
    integer(i4), intent(in),optional :: base
    type (pio_rearr_opt_t), intent(in), optional :: rearr_opts

    integer :: lbase
    integer :: ierr
    interface
       integer(c_int) function PIOc_Init_Intracomm_from_F90(f90_comp_comm, num_iotasks, stride,base,rearr,rearr_opts,iosysidp) &
            bind(C,name="PIOc_Init_Intracomm_from_F90")
         use iso_c_binding
         use pio_types
         integer(C_INT), value :: f90_comp_comm
         integer(C_INT), value :: num_iotasks
         integer(C_INT), value :: stride
         integer(C_INT), value :: base
         integer(C_INT), value :: rearr
          type(pio_rearr_opt_t) :: rearr_opts
         integer(C_INT) :: iosysidp
       end function PIOc_Init_Intracomm_from_F90
    end interface

#ifdef TIMING
    call t_startf("PIO:init")
#endif
    lbase=0
    if(present(base)) lbase=base
    ierr = PIOc_Init_Intracomm_from_F90(comp_comm,num_iotasks,stride,lbase,rearr,rearr_opts,iosystem%iosysid)

    call CheckMPIReturn("Bad Initialization in PIO_Init_Intracomm:  ", ierr,__FILE__,__LINE__)
#ifdef TIMING
    call t_stopf("PIO:init")
#endif
  end subroutine init_intracom


!>
!! @public
!! @ingroup PIO_init
!! @brief Initialize the pio subsystem.
!! @details  This is a collective call.  Input parameters are read on comp_rank=0
!!   values on other tasks are ignored.  This variation of PIO_init sets up a distinct set of tasks
!!   to handle IO, these tasks do not return from this call.  Instead they go to an internal loop
!!   and wait to receive further instructions from the computational tasks
!! @param component_count The number of computational components to associate with this IO component
!! @param peer_comm  The communicator from which all other communicator arguments are derived
!! @param comp_comms The computational communicator for each of the computational components
!! @param io_comm    The io communicator
!! @param iosystem a derived type which can be used in subsequent pio operations (defined in PIO_types).
!! @param rearranger The rearranger to use (optional)
!<
  subroutine init_intercom(component_count, peer_comm, comp_comms, io_comm, iosystem, rearranger)
    use iso_c_binding
    integer, intent(in) :: component_count
    integer, intent(in) :: peer_comm
    integer, target, intent(in) :: comp_comms(component_count)   !  The compute communicator
    integer, intent(in) :: io_comm     !  The io communicator
    type (iosystem_desc_t), intent(out)  :: iosystem(component_count)  ! io descriptor to initalize
    integer, intent(in), optional :: rearranger  ! The rearranger to use

    integer :: i, ierr
    integer :: lrearranger = pio_rearr_box
    integer, target :: iosystem_ioids(component_count)
    type(C_PTR) :: cptr_iosystem_ioids
    type(C_PTR) :: cptr_comp_comms

    interface
      integer(c_int) function PIOc_Init_Intercomm_from_F90(component_count, f90_peer_comm, f90_comp_comms, f90_io_comm, rearranger, ioids) &
           bind(C,name="PIOc_Init_Intercomm_from_F90")
          use iso_c_binding
          use pio_types
          integer(C_INT), value :: component_count
          integer(C_INT), value :: f90_peer_comm
          type(C_PTR), value :: f90_comp_comms
          integer(C_INT), value :: f90_io_comm
          integer(C_INT), value :: rearranger
          type(C_PTR), value :: ioids
      end function PIOc_Init_Intercomm_from_F90
    end interface

#ifdef TIMING
    call t_startf("PIO:init")
#endif
    cptr_comp_comms = C_LOC(comp_comms)
    do i=1,component_count
      iosystem_ioids(i) = -1
    end do
    cptr_iosystem_ioids = C_LOC(iosystem_ioids)
    if(present(rearranger)) then
      lrearranger = rearranger
    end if
    ierr = PIOc_Init_Intercomm_from_F90(component_count, peer_comm, cptr_comp_comms, io_comm, lrearranger, cptr_iosystem_ioids)
    do i=1,component_count
      iosystem(i)%iosysid = iosystem_ioids(i)
    end do
#ifdef TIMING
    call t_stopf("PIO:init")
#endif
  end subroutine init_intercom

!>
!! @public
!! @defgroup PIO_set_hint  PIO_set_hint
!! @brief set file system hints using mpi_info_set
!! @details This is a collective call which expects the following parameters:
!! @param iosystem @copydoc io_desc_t
!! @param hint  the string name of the hint to define
!! @param hintval  the string value to set the hint to
!! @retval ierr @copydoc  error_return
!<
  subroutine PIO_set_hint(iosystem, hint, hintval)
    type (iosystem_desc_t), intent(inout)  :: iosystem  ! io descriptor to initalize
    character(len=*), intent(in) :: hint, hintval
    integer :: ierr

    interface
       integer(C_INT) function PIOc_set_hint(iosysid, key, val) &
            bind(C,name="PIOc_set_hint")
         use iso_c_binding
         integer(C_INT), intent(in), value :: iosysid
         character(C_CHAR), intent(in) :: key
         character(C_CHAR), intent(in) :: val
       end function PIOc_set_hint
    end interface


    ierr = PIOc_set_hint(iosystem%iosysid, hint, hintval)


  end subroutine PIO_set_hint


!>
!! @public
!! @ingroup PIO_finalize
!! @brief finalizes the pio subsystem.
!! @details This is a collective call which expects the following parameters
!! @param iosystem : @copydoc io_desc_t
!! @retval ierr @copydoc  error_return
!<
  subroutine finalize(iosystem,ierr)
     type (iosystem_desc_t), intent(inout) :: iosystem
     integer(i4), intent(out) :: ierr
    interface
       integer(C_INT) function PIOc_finalize(iosysid) &
            bind(C,name="PIOc_finalize")
         use iso_c_binding
         integer(C_INT), intent(in), value :: iosysid
       end function PIOc_finalize
    end interface
    if(iosystem%iosysid /= -1) then
       ierr = PIOc_finalize(iosystem%iosysid)
    endif
  end subroutine finalize


!>
!! @public
!! @ingroup PIO_getnumiotasks
!! @brief This returns the number of IO-tasks that PIO is using
!! @param iosystem : a defined pio system descriptor, see PIO_types
!! @param numiotasks : the number of IO-tasks
!<
   subroutine getnumiotasks(iosystem,numiotasks)
       type (iosystem_desc_t), intent(in) :: iosystem
       integer(i4), intent(out) :: numiotasks
       integer :: ierr
       interface
          integer(C_INT) function PIOc_get_numiotasks(iosysid,numiotasks) &
               bind(C,name="PIOc_get_numiotasks")
            use iso_c_binding
            integer(C_INT), intent(in), value :: iosysid
            integer(C_INT), intent(out) :: numiotasks
          end function PIOc_get_numiotasks
       end interface
       ierr = PIOc_get_numiotasks(iosystem%iosysid, numiotasks)

   end subroutine getnumiotasks

   logical function pio_iotype_available( iotype) result(available)
     integer, intent(in) :: iotype
     interface
        integer(C_INT) function PIOc_iotype_available(iotype) &
             bind(C,name="PIOc_iotype_available")
          use iso_c_binding
          integer(C_INT), intent(in), value :: iotype
        end function PIOc_iotype_available
     end interface
     available= (PIOc_iotype_available(iotype) == 1)

   end function pio_iotype_available


!>
!! @public
!! @ingroup PIO_createfile
!! @brief  Create a NetCDF or PNetCDF file using PIO.
!! @details  Input parameters are read on comp task 0 and ignored elsewhere
!! @param iosystem : A defined pio system descriptor created by a call to @ref PIO_init (see PIO_types)
!! @param file  :  The returned file descriptor
!! @param iotype : @copydoc PIO_iotype
!! @param fname : The name of the file to open
!! @param amode_in : The NetCDF creation mode flag. the following flags are available:
!! (1) zero value or NC_NOWRITE is default and opens the file with read-only access.
!! (2) NC_WRITE for read-write access.
!! (3) NC_SHARE is used for NetCDF classic, and dangerous with this application.
!! (4) NC_WRITE|NC_SHARE
!! @retval ierr @copydoc error_return
!<
  integer function createfile(iosystem, file,iotype, fname, amode_in) result(ierr)
    type (iosystem_desc_t), intent(inout), target :: iosystem
    type (file_desc_t), intent(out) :: file
    integer, intent(in) :: iotype
    character(len=*), intent(in)  :: fname
    integer, optional, intent(in) :: amode_in
    integer :: mode
    interface
       integer(C_INT) function PIOc_createfile(iosysid, fh, iotype, fname,mode) &
         bind(C,NAME='PIOc_createfile')
         use iso_c_binding
         implicit none
         integer(c_int), value :: iosysid
         integer(c_int) :: fh
         integer(c_int) :: iotype
         character(kind=c_char) :: fname(*)
         integer(c_int), value :: mode
       end function PIOc_createfile
    end interface
    character, allocatable :: cfname(:)
    integer :: i, nl
#ifdef TIMING
    call t_startf("PIO:createfile")
#endif
    mode = 0
    if(present(amode_in)) mode = amode_in
    nl = len_trim(fname)
    allocate(cfname(nl+1))
    do i=1,nl
       cfname(i) = fname(i:i)
    enddo
    cfname(nl+1)=C_NULL_CHAR
    ierr = PIOc_createfile(iosystem%iosysid, file%fh, iotype, cfname, mode)
    deallocate(cfname)
    file%iosystem => iosystem
#ifdef TIMING
    call t_stopf("PIO:createfile")
#endif
  end function createfile
!>
!! @public
!! @ingroup PIO_openfile
!! @brief open an existing file using pio
!! @details  Input parameters are read on comp task 0 and ignored elsewhere.
!! @param iosystem : a defined pio system descriptor created by a call to @ref PIO_init (see PIO_types)
!! @param file  :  the returned file descriptor
!! @param iotype : @copybrief PIO_iotype
!! @param fname : the name of the file to open
!! @param mode : a zero value (or PIO_nowrite) specifies the default
!! behavior: open the dataset with read-only access, buffering and
!! caching accesses for efficiency otherwise, the creation mode is
!! PIO_write. setting the PIO_write flag opens the dataset with
!! read-write access. ("writing" means any kind of change to the dataset,
!! including appending or changing data, adding or renaming dimensions,
!! variables, and attributes, or deleting attributes.)
!! @retval ierr @copydoc error_return
!<
  integer function PIO_openfile(iosystem, file, iotype, fname,mode) result(ierr)

!    use ifcore, only: tracebackqq
    type (iosystem_desc_t), intent(inout), target :: iosystem
    type (file_desc_t), intent(out) :: file
    integer, intent(in) :: iotype
    character(len=*), intent(in)  :: fname
    integer, optional, intent(in) :: mode
    interface
       integer(C_INT) function PIOc_openfile(iosysid, fh, iotype, fname,mode) &
         bind(C,NAME='PIOc_openfile')
         use iso_c_binding
         implicit none
         integer(c_int), value :: iosysid
         integer(c_int) :: fh
         integer(c_int) :: iotype
         character(kind=c_char) :: fname(*)
         integer(c_int), value :: mode
       end function PIOc_openfile
    end interface
    integer :: imode=0, i, nl
    character, allocatable :: cfname(:)
#ifdef TIMING
    call t_startf("PIO:openfile")
#endif
    if(present(mode)) imode = mode
    nl = len_trim(fname)
    allocate(cfname(nl+1))
    do i=1,nl
       cfname(i) = fname(i:i)
    enddo
    cfname(nl+1)=C_NULL_CHAR
    ierr = PIOc_openfile( iosystem%iosysid, file%fh, iotype, cfname, imode)
    deallocate(cfname)
    file%iosystem => iosystem

#ifdef TIMING
    call t_stopf("PIO:openfile")
#endif
  end function PIO_openfile

!>
!! @public
!! @ingroup PIO_syncfile
!! @brief synchronizing a file forces all writes to complete before the subroutine returns.
!!
!! @param file @copydoc file_desc_t
!<
  subroutine syncfile(file)
    implicit none
    type (file_desc_t), target :: file
    integer :: ierr
    interface
       integer(C_INT) function PIOc_sync(ncid) &
            bind(C,name="PIOc_sync")
         use iso_c_binding
         integer(C_INT), intent(in), value :: ncid
       end function PIOc_sync
    end interface

    ierr = PIOc_sync(file%fh)

  end subroutine syncfile
!>
!! @public
!! @ingroup PIO_freedecomp
!! @brief free all allocated storage associated with this decomposition
!! @details
!! @param ios :  a defined pio system descriptor created by call to @ref PIO_init (see PIO_types)
!! @param iodesc @copydoc io_desc_t
!<
  subroutine freedecomp_ios(ios,iodesc)
    implicit none
    type (iosystem_desc_t) :: ios
    type (io_desc_t) :: iodesc
    integer :: ierr
    interface
       integer(C_INT) function PIOc_freedecomp(iosysid, ioid) &
            bind(C,name="PIOc_freedecomp")
         use iso_c_binding
         integer(C_INT), intent(in), value :: iosysid, ioid
       end function PIOc_freedecomp
    end interface

    ierr = PIOc_freedecomp(ios%iosysid, iodesc%ioid)

  end subroutine freedecomp_ios
!>
!! @public
!! @ingroup PIO_freedecomp
!! @brief free all allocated storage associated with this decomposition
!! @details
!! @param file @copydoc file_desc_t
!! @param iodesc : @copydoc io_desc_t
!! @retval ierr @copydoc error_return
!<
  subroutine freedecomp_file(file,iodesc)
    implicit none
    type (file_desc_t) :: file
    type (io_desc_t) :: iodesc

    call syncfile(file)

    call freedecomp_ios(file%iosystem, iodesc)

  end subroutine freedecomp_file

!>
!! @public
!! @ingroup PIO_closefile
!! @brief close a disk file
!! @details
!! @param file @copydoc file_desc_t
!<
  subroutine closefile(file)
    type(file_desc_t) :: file
    integer :: ierr
    interface
       integer(c_int) function PIOc_closefile(ncid) &
            bind(C,name="PIOc_closefile")
         use iso_c_binding
         integer(C_INT), value :: ncid
       end function PIOc_closefile
    end interface
#ifdef TIMING
    call t_startf("PIO:closefile")
#endif
    ierr = PIOc_closefile(file%fh)
    nullify(file%iosystem)
#ifdef TIMING
    call t_stopf("PIO:closefile")
#endif

  end subroutine closefile

!>
!! @public
!! @ingroup PIO_deletefile
!! @brief Delete a file
!! @details
!! @param ios : a pio system handle
!! @param fname : a filename
!<
  subroutine pio_deletefile(ios, fname)
    type(iosystem_desc_t) :: ios
    character(len=*) :: fname
    integer :: ierr
    interface
       integer(c_int) function PIOc_deletefile(iosid, fname) &
            bind(C,name="PIOc_deletefile")
         use iso_c_binding
         integer(C_INT), value :: iosid
         character(kind=c_char) :: fname
       end function PIOc_deletefile
    end interface

    ierr = PIOc_deletefile(ios%iosysid, trim(fname)//C_NULL_CHAR)

  end subroutine pio_deletefile

!>
!! @public
!! @ingroup PIO_set_rearr_opts
!! @brief Set the rerranger options
!! @details
!! @param ios : handle to pio iosystem
!! @param comm_type : @copydoc PIO_rearr_comm_t
!! @param fcd : @copydoc PIO_rearr_comm_dir
!! @param enable_hs_c2i : Enable handshake (compute procs to io procs)
!! @param enable_isend_c2i : Enable isends (compute procs to io procs)
!! @param max_pend_req_c2i: Maximum pending requests (compute procs to io procs)
!! @param enable_hs_i2c : Enable handshake (io procs to compute procs)
!! @param enable_isend_i2c : Enable isends (io procs to compute procs)
!! @param max_pend_req_i2c: Maximum pending requests (io procs to compute procs)
!! @copydoc PIO_rearr_comm_fc_options
!<
  function pio_set_rearr_opts(ios, comm_type, fcd,&
                              enable_hs_c2i, enable_isend_c2i,&
                              max_pend_req_c2i,&
                              enable_hs_i2c, enable_isend_i2c,&
                              max_pend_req_i2c) result(ierr)

    type(iosystem_desc_t), intent(inout) :: ios
    integer, intent(in) :: comm_type, fcd
    logical, intent(in) :: enable_hs_c2i, enable_hs_i2c
    logical, intent(in) :: enable_isend_c2i, enable_isend_i2c
    integer, intent(in) :: max_pend_req_c2i, max_pend_req_i2c
    integer :: ierr
    interface
      integer(c_int) function PIOc_set_rearr_opts(iosysid, comm_type, fcd,&
                                                  enable_hs_c2i, enable_isend_c2i,&
                                                  max_pend_req_c2i,&
                                                  enable_hs_i2c, enable_isend_i2c,&
                                                  max_pend_req_i2c)&
        bind(C,name="PIOc_set_rearr_opts")
        use iso_c_binding
        integer(C_INT), intent(in), value :: iosysid
        integer(C_INT), intent(in), value :: comm_type
        integer(C_INT), intent(in), value :: fcd
        logical(C_BOOL), intent(in), value :: enable_hs_c2i
        logical(C_BOOL), intent(in), value :: enable_isend_c2i
        integer(C_INT), intent(in), value :: max_pend_req_c2i
        logical(C_BOOL), intent(in), value :: enable_hs_i2c
        logical(C_BOOL), intent(in), value :: enable_isend_i2c
        integer(C_INT), intent(in), value :: max_pend_req_i2c
      end function PIOc_set_rearr_opts
    end interface

    ierr = PIOc_set_rearr_opts(ios%iosysid, comm_type, fcd,&
                                logical(enable_hs_c2i, kind=c_bool),&
                                logical(enable_isend_c2i, kind=c_bool),&
                                max_pend_req_c2i,&
                                logical(enable_hs_i2c, kind=c_bool),&
                                logical(enable_isend_i2c, kind=c_bool),&
                                max_pend_req_i2c)

  end function pio_set_rearr_opts


end module piolib_mod

  !|||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
