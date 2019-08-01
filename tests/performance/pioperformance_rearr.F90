!#define VARINT 1
#define VARREAL 1
!#define VARDOUBLE 1

program pioperformance_rearr
#ifndef NO_MPIMOD
  use mpi
#endif
  use perf_mod, only : t_initf, t_finalizef
  use pio, only : pio_iotype_netcdf, pio_iotype_pnetcdf, pio_iotype_netcdf4p, &
       pio_iotype_netcdf4c, pio_rearr_subset, pio_rearr_box, PIO_MAX_NAME,&
        pio_rearr_opt_t, pio_rearr_comm_p2p, pio_rearr_comm_coll,&
        pio_rearr_comm_fc_2d_disable, pio_rearr_comm_fc_1d_comp2io,&
        pio_rearr_comm_fc_1d_io2comp, pio_rearr_comm_fc_2d_enable,&
        pio_rearr_comm_unlimited_pend_req, PIO_NOERR
  implicit none
#ifdef NO_MPIMOD
#include <mpif.h>
#endif  
  integer, parameter :: MAX_IO_TASK_ARRAY_SIZE=256, MAX_DECOMP_FILES=256
  integer, parameter :: MAX_FNAME_LEN = 1024
  integer, parameter :: MAX_PIO_TYPENAME_LEN = 8
  integer, parameter :: MAX_PIO_TYPES = 4, MAX_PIO_REARRS = 2
  integer, parameter :: MAX_NVARS = 12

  character(len=*), parameter :: PIO_NML_FNAME = 'pioperf_rearr.nl'

  integer :: ierr, mype, npe, i
  logical :: Mastertask
  character(len=MAX_FNAME_LEN) :: decompfile(MAX_DECOMP_FILES)
  integer :: piotypes(MAX_PIO_TYPES), niotypes
  integer :: rearrangers(MAX_PIO_REARRS)
  integer :: niotasks(MAX_IO_TASK_ARRAY_SIZE)
  integer :: nv, nframes, nvars(MAX_NVARS)
  integer :: vs, varsize(MAX_NVARS) !  Local size of array for idealized decomps
  logical :: unlimdimindof
  type(pio_rearr_opt_t) :: rearr_opts
  logical :: nml_file_exists = .false. , use_gptl = .false.
#ifdef BGQTRY
  external :: print_memusage
#endif
  external :: gptlinitialize, gptlfinalize
#ifdef _PIO1
  integer, parameter :: PIO_FILL_INT   = 02147483647
  real, parameter    :: PIO_FILL_FLOAT = 9.969209968E+36
  double precision, parameter :: PIO_FILL_DOUBLE = 9.969209968E+36
#endif
  !
  ! Initialize MPI
  !
  call MPI_Init(ierr)
  call CheckMPIreturn(__LINE__,ierr)
  call MPI_Comm_rank(MPI_COMM_WORLD, mype, ierr)
  call CheckMPIreturn(__LINE__,ierr)
  call MPI_Comm_size(MPI_COMM_WORLD, npe,  ierr)
  call CheckMPIreturn(__LINE__,ierr)
  if(mype==0) then
     Mastertask=.true.
  else
     Mastertask=.false.
  endif
#ifdef BGQTRY
  call print_memusage()
#endif
  nvars = 0
  niotasks = -1 ! loop over all possible values
  rearrangers = 0
  nframes = 5
  decompfile = ' '
  piotypes = -1
  varsize = 0
  varsize(1) = 1
  unlimdimindof=.false.
  call read_user_input(mype, decompfile, piotypes, rearrangers,&
        niotasks, nframes, unlimdimindof, nvars, varsize,&
        rearr_opts, ierr)

  inquire(file=PIO_NML_FNAME,exist=nml_file_exists)
  if(nml_file_exists) then
    use_gptl = .false.
  else
    use_gptl = .true.
  end if
  if(use_gptl) then
    call gptlinitialize()
  else
    call t_initf(PIO_NML_FNAME, LogPrint=.false.,&
      mpicom=MPI_COMM_WORLD, MasterTask=MasterTask)
  end if
  niotypes = 0
  do i=1,MAX_PIO_TYPES
     if (piotypes(i) > -1) niotypes = niotypes+1
  enddo
  if(rearrangers(1)==0) then
    rearrangers(1)=1
    rearrangers(2)=2
  endif  

  do i=1,MAX_DECOMP_FILES
     if(len_trim(decompfile(i))==0) exit
     if(mype == 0) print *, ' Testing decomp: ',trim(decompfile(i))
     do vs = 1, MAX_NVARS
        if(varsize(vs) > 0 ) then
           do nv=1,MAX_NVARS
              if(nvars(nv)>0) then
                 call pioperformance_rearrtest(decompfile(i), piotypes(1:niotypes),&
                      mype, npe, rearrangers, rearr_opts, niotasks, nframes,&
                      nvars(nv), varsize(vs),unlimdimindof) 
              endif
           enddo
        endif
     enddo
  enddo

  if(use_gptl) then
    call gptlfinalize()
  else
    call t_finalizef()
  end if

  call MPI_Finalize(ierr)
contains

  ! Initialize an array from a comma separated list
  ! The function only accepts either an integer array or a
  ! character array (if you provide both, the function 
  ! currently only parses for the integer array)
  ! Note: When directly reading a list of comma separated list
  ! of integers into an array using fortran read(str,*) we
  ! need,
  ! 1) To know the number of elements read (size of array <=
  !    number of elements read)
  ! 2) This read does not handle simple user errors in input
  subroutine init_arr_from_list(argv, iarr, carr, ierr)
    character(len=*), intent(in) :: argv
    integer, dimension(:), intent(out), optional :: iarr
    character(len=*), dimension(:), intent(out), optional :: carr
    integer, intent(out), optional :: ierr

    !integer, parameter :: MAX_STR_LEN = 4096

    !character(len=MAX_STR_LEN) :: tmp_argv
    character, parameter :: LIST_DELIM = ','
    integer :: arr_idx
    integer :: prev_pos, pos, rel_pos, max_arr_sz, totlen, remlen

    if(present(ierr)) then
      ierr = PIO_NOERR
    end if
    !print *, "Parsing :", trim(argv)
    max_arr_sz = 0
    if(present(iarr)) then
      max_arr_sz = size(iarr)
    else if(present(carr)) then
      max_arr_sz = size(carr)
    end if

    totlen = len_trim(argv)
    remlen = totlen
    ! The substring considered is always from posn prev_pos+1
    prev_pos = 0
    pos = index(argv, LIST_DELIM)

    if(totlen == 0) then
      return
    end if

    arr_idx = 1
    do while((remlen > 0) .and. (arr_idx <= max_arr_sz))
      if(pos == 0) then
        ! Last element in list
        pos = totlen + 1
        remlen = 0
      else
        remlen = totlen - pos
      end if
      !print *, "prev_pos = ", prev_pos, ", pos=", pos, ", remlen=", remlen
      if(prev_pos+1 <= pos-1) then
        if(present(iarr)) then
          read(argv(prev_pos+1:pos-1), *) iarr(arr_idx) 
          !print *, "Parser : read : ", iarr(arr_idx)
        else if(present(carr)) then
          read(argv(prev_pos+1:pos-1), *) carr(arr_idx) 
          !print *, "Parser : read : ", carr(arr_idx)
        end if
        arr_idx = arr_idx + 1
      else
        ! Ignore this invalid value and continue parsing
        print *, "Warning : INVALID user input - not well formed list"
      end if

      if(remlen > 0) then
        prev_pos = pos
        rel_pos = index(argv(pos+1:), LIST_DELIM)
        pos = pos + rel_pos
        if(rel_pos == 0) then
          ! Last element in the list
          pos = 0
        end if
      end if
    end do
    
  end subroutine init_arr_from_list

  subroutine pio_typename2type(pio_typename, pio_type)
    character(len=*), intent(in) :: pio_typename
    integer, intent(out) :: pio_type

    if(pio_typename .eq. 'netcdf') then
      pio_type = PIO_IOTYPE_NETCDF
    else if(pio_typename .eq. 'netcdf4p') then
      pio_type = PIO_IOTYPE_NETCDF4P
    else if(pio_typename .eq. 'netcdf4c') then
      pio_type = PIO_IOTYPE_NETCDF4C
    else if(pio_typename .eq. 'pnetcdf') then
      pio_type = PIO_IOTYPE_PNETCDF
    else
      !print *, "ERROR: Unrecognized pio type :", pio_typename,&
      !          __FILE__, __LINE__
    endif
  end subroutine pio_typename2type

  ! Convert pio rearr option string to option
  subroutine pio_rearr_str2opt(rearr_opt_str, rearr_opt)
    character(len=*), intent(in) :: rearr_opt_str
    integer, intent(out) :: rearr_opt

    if(rearr_opt_str .eq. 'p2p') then
      rearr_opt = pio_rearr_comm_p2p
    else if(rearr_opt_str .eq. 'coll') then
      rearr_opt = pio_rearr_comm_coll
    else if(rearr_opt_str .eq. '2d_enable') then
      rearr_opt = pio_rearr_comm_fc_2d_enable
    else if(rearr_opt_str .eq. '1d_comp2io') then
      rearr_opt = pio_rearr_comm_fc_1d_comp2io
    else if(rearr_opt_str .eq. '1d_io2comp') then
      rearr_opt = pio_rearr_comm_fc_1d_io2comp
    else if(rearr_opt_str .eq. '2d_disable') then
      rearr_opt = pio_rearr_comm_fc_2d_disable
    else
      !print *, "ERROR: Unrecognized pio rearr opt :", rearr_opt_str,&
      !          __FILE__, __LINE__
    end if
    
  end subroutine pio_rearr_str2opt

  ! Parse a single command line arg
  subroutine parse_and_process_input(argv, decompfiles, piotypes,&
        rearrangers, niotasks, nframes, unlimdimindof, nvars, varsize,&
        rearr_opts, ierr)
    character(len=*), intent(in)  :: argv
    character(len=MAX_FNAME_LEN), intent(out) :: decompfiles(MAX_DECOMP_FILES)
    integer, intent(out) :: piotypes(MAX_PIO_TYPES)
    integer, intent(out) :: rearrangers(MAX_PIO_REARRS)
    integer, intent(out) :: niotasks(MAX_IO_TASK_ARRAY_SIZE)
    integer, intent(out) :: nframes
    logical, intent(out) :: unlimdimindof
    integer, intent(out) :: nvars(MAX_NVARS)
    integer, intent(out) :: varsize(MAX_NVARS)
    type(pio_rearr_opt_t), intent(out) :: rearr_opts
    integer, intent(out) :: ierr

    integer, parameter :: MAX_PIO_REARR_OPT_LEN = 128
    character(len=MAX_PIO_TYPENAME_LEN) :: pio_typenames(MAX_PIO_TYPES)
    character(len=MAX_PIO_REARR_OPT_LEN) :: tmp_rearr_opt
    integer :: pos, i

    ! All input arguments are of the form <INPUT_ARG_NAME>=<INPUT_ARG>
    !print *, argv
    pos = index(argv, "=")
    if (pos == 0) then
      ! Ignore unrecognized args
      return
    else
      ! Check if it an input to PIO testing framework
      if (argv(:pos) == "--pio-decompfiles=") then
        call init_arr_from_list(argv(pos+1:), carr=decompfiles, ierr=ierr)
        !print *, "Read decompfiles : ", decompfiles
      else if (argv(:pos) == "--pio-types=") then
        pio_typenames = ' '
        call init_arr_from_list(argv(pos+1:), carr=pio_typenames, ierr=ierr)
        !print *, "Read types : ", pio_typenames
        do i=1,MAX_PIO_TYPES
          call pio_typename2type(pio_typenames(i), piotypes(i))
        end do
      else if (argv(:pos) == "--pio-rearrangers=") then
        call init_arr_from_list(argv(pos+1:), iarr=rearrangers, ierr=ierr)
        !print *, "Read rearrangers : ", rearrangers
      else if (argv(:pos) == "--pio-rearr-comm-type=") then
        tmp_rearr_opt = ''
        read(argv(pos+1:), *) tmp_rearr_opt
        !print *, "Read rearr opt comm type : ", tmp_rearr_opt
        call pio_rearr_str2opt(tmp_rearr_opt,rearr_opts%comm_type)
      else if (argv(:pos) == "--pio-rearr-fcd=") then
        tmp_rearr_opt = ''
        read(argv(pos+1:), *) tmp_rearr_opt
        !print *, "Read rearr opt fcd : ", tmp_rearr_opt
        call pio_rearr_str2opt(tmp_rearr_opt,rearr_opts%fcd)
      else if (argv(:pos) == "--pio-rearr-comp2io-enable-hs=") then
        read(argv(pos+1:), *) rearr_opts%comm_fc_opts_comp2io%enable_hs
        !print *, "Read rearr opt enable_hs (comp2io): ",&
        !            rearr_opts%comm_fc_opts_comp2io%enable_hs 
      else if (argv(:pos) == "--pio-rearr-comp2io-enable-isend=") then
        read(argv(pos+1:), *) rearr_opts%comm_fc_opts_comp2io%enable_isend
        !print *, "Read rearr opt enable_isend (comp2io): ",&
        !            rearr_opts%comm_fc_opts_comp2io%enable_isend
      else if (argv(:pos) == "--pio-rearr-comp2io-max-pend-req=") then
        read(argv(pos+1:), *) rearr_opts%comm_fc_opts_comp2io%max_pend_req
        !print *, "Read rearr opt max pend req (comp2io): ",&
        !            rearr_opts%comm_fc_opts_comp2io%max_pend_req
      else if (argv(:pos) == "--pio-rearr-io2comp-enable-hs=") then
        read(argv(pos+1:), *) rearr_opts%comm_fc_opts_io2comp%enable_hs
        !print *, "Read rearr opt enable_hs (io2comp): ",&
        !            rearr_opts%comm_fc_opts_io2comp%enable_hs 
      else if (argv(:pos) == "--pio-rearr-io2comp-enable-isend=") then
        read(argv(pos+1:), *) rearr_opts%comm_fc_opts_io2comp%enable_isend
        !print *, "Read rearr opt enable_isend (io2comp): ",&
        !            rearr_opts%comm_fc_opts_io2comp%enable_isend
      else if (argv(:pos) == "--pio-rearr-io2comp-max-pend-req=") then
        read(argv(pos+1:), *) rearr_opts%comm_fc_opts_io2comp%max_pend_req
        !print *, "Read rearr opt max pend req (io2comp): ",&
        !            rearr_opts%comm_fc_opts_io2comp%max_pend_req
      else if (argv(:pos) == "--pio-niotasks=") then
        call init_arr_from_list(argv(pos+1:), iarr=niotasks, ierr=ierr)
        !print *, "Read niotasks : ", niotasks
      else if (argv(:pos) == "--pio-nframes=") then
        read(argv(pos+1:), *) nframes
        !print *, "Read nframes = ", nframes
      else if (argv(:pos) == "--pio-unlimdimindof=") then
        read(argv(pos+1:), *) unlimdimindof
        !print *, "Read unlimdimindof = ", unlimdimindof
      else if (argv(:pos) == "--pio-nvars=") then
        call init_arr_from_list(argv(pos+1:), iarr=nvars, ierr=ierr)
        !print *, "Read nvars : ", nvars
      else if (argv(:pos) == "--pio-varsize=") then
        call init_arr_from_list(argv(pos+1:), iarr=varsize, ierr=ierr)
        !print *, "Read varsize : ", varsize
      end if
    end if

  end subroutine parse_and_process_input

  ! Parse command line user options
  subroutine read_cmd_line_input(decompfile, piotypes, rearrangers,&
        niotasks, nframes, unlimdimindof, nvars, varsize,&
        rearr_opts, ierr)
    character(len=MAX_FNAME_LEN), intent(out) :: decompfile(MAX_DECOMP_FILES)
    integer, intent(out) :: piotypes(MAX_PIO_TYPES)
    integer, intent(out) :: rearrangers(MAX_PIO_REARRS)
    integer, intent(out) :: niotasks(MAX_IO_TASK_ARRAY_SIZE)
    integer, intent(out) :: nframes
    logical, intent(out) :: unlimdimindof
    integer, intent(out) :: nvars(MAX_NVARS)
    integer, intent(out) :: varsize(MAX_NVARS)
    type(pio_rearr_opt_t), intent(out) :: rearr_opts
    integer, intent(out) :: ierr

    integer, parameter :: MAX_STDIN_ARG_LEN = 8192
    character(len=MAX_STDIN_ARG_LEN) :: argv
    integer :: i, nargs

    nargs = command_argument_count()
    do i=1,nargs
      call get_command_argument(i, argv)
      call parse_and_process_input(argv, decompfile, piotypes, rearrangers,&
            niotasks, nframes, unlimdimindof, nvars, varsize,&
            rearr_opts, ierr)
    end do

  end subroutine read_cmd_line_input

  ! Read the namelist file, if it exists
  subroutine read_nml_input(decompfile, piotypes, rearrangers,&
        niotasks, nframes, unlimdimindof, nvars, varsize,&
        rearr_opts, ierr)
    character(len=MAX_FNAME_LEN), intent(out) :: decompfile(MAX_DECOMP_FILES)
    integer, intent(out) :: piotypes(MAX_PIO_TYPES)
    integer, intent(out) :: rearrangers(MAX_PIO_REARRS)
    integer, intent(out) :: niotasks(MAX_IO_TASK_ARRAY_SIZE)
    integer, intent(out) :: nframes
    logical, intent(out) :: unlimdimindof
    integer, intent(out) :: nvars(MAX_NVARS)
    integer, intent(out) :: varsize(MAX_NVARS)
    type(pio_rearr_opt_t), intent(out) :: rearr_opts
    integer :: ipio_rearr_comm_type, ipio_rearr_fcd
    character(len=PIO_MAX_NAME) :: pio_rearr_comm_type, pio_rearr_fcd
    logical :: pio_rearr_comp2io_enable_hs, pio_rearr_comp2io_enable_isend
    integer :: pio_rearr_comp2io_max_pend_req
    logical :: pio_rearr_io2comp_enable_hs, pio_rearr_io2comp_enable_isend
    integer :: pio_rearr_io2comp_max_pend_req
    integer, intent(out) :: ierr

    character(len=MAX_PIO_TYPENAME_LEN) :: pio_typenames(MAX_PIO_TYPES)
    logical :: file_exists = .false.

    namelist /pioperf/ decompfile, pio_typenames, rearrangers, niotasks, nframes, &
         nvars, varsize, unlimdimindof,&
          pio_rearr_comm_type, pio_rearr_fcd, &
          pio_rearr_comp2io_enable_hs, pio_rearr_comp2io_enable_isend, &
          pio_rearr_comp2io_max_pend_req,&
          pio_rearr_io2comp_enable_hs, pio_rearr_io2comp_enable_isend, &
          pio_rearr_io2comp_max_pend_req

    ierr = PIO_NOERR
    pio_typenames = ' '
    pio_rearr_comm_type = ' '
    pio_rearr_fcd = ' '

    ipio_rearr_comm_type = pio_rearr_comm_coll
    ipio_rearr_fcd = pio_rearr_comm_fc_2d_disable
    pio_rearr_comp2io_enable_hs = .false.
    pio_rearr_comp2io_enable_isend = .false.
    pio_rearr_comp2io_max_pend_req = pio_rearr_comm_unlimited_pend_req
    pio_rearr_io2comp_enable_hs = .false.
    pio_rearr_io2comp_enable_isend = .false.
    pio_rearr_io2comp_max_pend_req = pio_rearr_comm_unlimited_pend_req

    inquire(file=PIO_NML_FNAME,exist=file_exists)
    if(file_exists) then
      open(unit=12,file=PIO_NML_FNAME,status='old')
      read(12,pioperf)
      close(12)
      call pio_rearr_str2opt(pio_rearr_comm_type, ipio_rearr_comm_type)
      call pio_rearr_str2opt(pio_rearr_fcd, ipio_rearr_fcd)
      do i=1,MAX_PIO_TYPES
         call pio_typename2type(pio_typenames(i), piotypes(i))
      enddo
    end if

    rearr_opts%comm_type = ipio_rearr_comm_type
    rearr_opts%fcd = ipio_rearr_fcd
    rearr_opts%comm_fc_opts_comp2io%enable_hs = pio_rearr_comp2io_enable_hs
    rearr_opts%comm_fc_opts_comp2io%enable_isend = pio_rearr_comp2io_enable_isend
    rearr_opts%comm_fc_opts_comp2io%max_pend_req =&
      pio_rearr_comp2io_max_pend_req
    rearr_opts%comm_fc_opts_io2comp%enable_hs = pio_rearr_io2comp_enable_hs
    rearr_opts%comm_fc_opts_io2comp%enable_isend = pio_rearr_io2comp_enable_isend
    rearr_opts%comm_fc_opts_io2comp%max_pend_req =&
      pio_rearr_io2comp_max_pend_req

  end subroutine read_nml_input

  ! Bcast rearranger options from rank 0 to all other ranks
  subroutine bcast_rearr_opts(rearr_opts)
    type(pio_rearr_opt_t), intent(out) :: rearr_opts

    enum, bind(c)
      enumerator :: COMM_TYPE_SIDX = 1
      enumerator :: FCD_SIDX
      enumerator :: COMP2IO_ENABLE_HS_SIDX
      enumerator :: COMP2IO_ENABLE_ISEND_SIDX
      enumerator :: COMP2IO_MAX_PEND_REQ_SIDX
      enumerator :: IO2COMP_ENABLE_HS_SIDX
      enumerator :: IO2COMP_ENABLE_ISEND_SIDX
      enumerator :: IO2COMP_MAX_PEND_REQ_SIDX
      enumerator :: NUM_OPTS = IO2COMP_MAX_PEND_REQ_SIDX
    end enum
    integer :: irearr_opts(NUM_OPTS)

    irearr_opts(COMM_TYPE_SIDX) = rearr_opts%comm_type
    irearr_opts(FCD_SIDX) = rearr_opts%fcd
    if(rearr_opts%comm_fc_opts_comp2io%enable_hs) then
      irearr_opts(COMP2IO_ENABLE_HS_SIDX) = 1
    else
      irearr_opts(COMP2IO_ENABLE_HS_SIDX) = 0
    end if
    if(rearr_opts%comm_fc_opts_comp2io%enable_isend) then
      irearr_opts(COMP2IO_ENABLE_ISEND_SIDX) = 1
    else
      irearr_opts(COMP2IO_ENABLE_ISEND_SIDX) = 0
    end if
    irearr_opts(COMP2IO_MAX_PEND_REQ_SIDX) =&
      rearr_opts%comm_fc_opts_comp2io%max_pend_req
    if(rearr_opts%comm_fc_opts_io2comp%enable_hs) then
      irearr_opts(IO2COMP_ENABLE_HS_SIDX) = 1
    else
      irearr_opts(IO2COMP_ENABLE_HS_SIDX) = 0
    end if
    if(rearr_opts%comm_fc_opts_io2comp%enable_isend) then
      irearr_opts(IO2COMP_ENABLE_ISEND_SIDX) = 1
    else
      irearr_opts(IO2COMP_ENABLE_ISEND_SIDX) = 0
    end if
    irearr_opts(IO2COMP_MAX_PEND_REQ_SIDX) =&
      rearr_opts%comm_fc_opts_io2comp%max_pend_req

    call MPI_Bcast(irearr_opts, NUM_OPTS, MPI_INTEGER, 0, MPI_COMM_WORLD, ierr)

    rearr_opts%comm_type = irearr_opts(COMM_TYPE_SIDX)
    rearr_opts%fcd = irearr_opts(FCD_SIDX)
    if(irearr_opts(COMP2IO_ENABLE_HS_SIDX) == 0) then
      rearr_opts%comm_fc_opts_comp2io%enable_hs = .false.
    else
      rearr_opts%comm_fc_opts_comp2io%enable_hs = .true.
    end if
    if(irearr_opts(COMP2IO_ENABLE_ISEND_SIDX) == 0) then
      rearr_opts%comm_fc_opts_comp2io%enable_isend = .false.
    else
      rearr_opts%comm_fc_opts_comp2io%enable_isend = .true.
    end if
    rearr_opts%comm_fc_opts_comp2io%max_pend_req =&
      irearr_opts(COMP2IO_MAX_PEND_REQ_SIDX)
    if(irearr_opts(IO2COMP_ENABLE_HS_SIDX) == 0) then
      rearr_opts%comm_fc_opts_io2comp%enable_hs = .false.
    else
      rearr_opts%comm_fc_opts_io2comp%enable_hs = .true.
    end if
    if(irearr_opts(IO2COMP_ENABLE_ISEND_SIDX) == 0) then
      rearr_opts%comm_fc_opts_io2comp%enable_isend = .false.
    else
      rearr_opts%comm_fc_opts_io2comp%enable_isend = .true.
    end if
    rearr_opts%comm_fc_opts_io2comp%max_pend_req =&
      irearr_opts(IO2COMP_MAX_PEND_REQ_SIDX)

  end subroutine bcast_rearr_opts

  ! Read user input
  ! Read the input from namelist file, if available, and then
  ! read (and override) the command line options
  subroutine read_user_input(mype, decompfile, piotypes, rearrangers,&
        niotasks, nframes, unlimdimindof, nvars, varsize,&
        rearr_opts, ierr)
    integer, intent(in) :: mype
    character(len=MAX_FNAME_LEN), intent(out) :: decompfile(MAX_DECOMP_FILES)
    integer, intent(out) :: piotypes(MAX_PIO_TYPES)
    integer, intent(out) :: rearrangers(MAX_PIO_REARRS)
    integer, intent(out) :: niotasks(MAX_IO_TASK_ARRAY_SIZE)
    integer, intent(out) :: nframes
    logical, intent(out) :: unlimdimindof
    integer, intent(out) :: nvars(MAX_NVARS)
    integer, intent(out) :: varsize(MAX_NVARS)
    type(pio_rearr_opt_t), intent(out) :: rearr_opts
    integer, intent(out) :: ierr

    character(len=MAX_PIO_TYPENAME_LEN) :: pio_typenames(MAX_PIO_TYPES)

    pio_typenames = ' '

    if(mype == 0) then
      ! Read namelist file
      call read_nml_input(decompfile, piotypes, rearrangers,&
            niotasks, nframes, unlimdimindof, nvars, varsize,&
            rearr_opts, ierr)
      ! Allow user to override the values via command line
      call read_cmd_line_input(decompfile, piotypes, rearrangers,&
            niotasks, nframes, unlimdimindof, nvars, varsize,&
            rearr_opts, ierr)
    end if

    call MPI_Bcast(decompfile,MAX_FNAME_LEN*MAX_DECOMP_FILES,MPI_CHARACTER,0, MPI_COMM_WORLD,ierr)
    call MPI_Bcast(piotypes,MAX_PIO_TYPES, MPI_INTEGER, 0, MPI_COMM_WORLD,ierr)
    call MPI_Bcast(rearrangers, MAX_PIO_REARRS, MPI_INTEGER, 0, MPI_COMM_WORLD,ierr)
    call MPI_Bcast(niotasks, MAX_IO_TASK_ARRAY_SIZE, MPI_INTEGER, 0, MPI_COMM_WORLD,ierr)
    call MPI_Bcast(nframes, 1, MPI_INTEGER, 0, MPI_COMM_WORLD,ierr)
    call MPI_Bcast(unlimdimindof, 1, MPI_INTEGER, 0, MPI_COMM_WORLD,ierr)
    call MPI_Bcast(nvars, MAX_NVARS, MPI_INTEGER, 0, MPI_COMM_WORLD,ierr)
    call MPI_Bcast(varsize, MAX_NVARS, MPI_INTEGER, 0, MPI_COMM_WORLD,ierr)

    call bcast_rearr_opts(rearr_opts)

  end subroutine read_user_input

  subroutine get_tstamp(wall, sys, usr)
    use perf_mod
    double precision, intent(out) :: wall, sys, usr
    interface
      function gptlstamp(wall, sys, usr)
        integer :: gptlstamp
        double precision, intent(out) :: wall, sys, usr
      end function
    end interface
    integer :: ierr

    if(use_gptl) then
      ierr = gptlstamp(wall, sys, usr)
    else
      call t_stampf(wall, sys, usr)
    end if
  end subroutine get_tstamp

  subroutine pioperformance_rearrtest(filename, piotypes, mype, npe_base, &
       rearrangers, rearr_opts, niotasks,nframes, nvars, varsize,&
       unlimdimindof)
    use pio
    use pio_support, only : pio_readdof
    use perf_mod
    character(len=*), intent(in) :: filename
    integer, intent(in) :: mype, npe_base
    integer, intent(in) :: piotypes(:)
    integer, intent(in) :: rearrangers(:)
    type(pio_rearr_opt_t), intent(in) :: rearr_opts
    integer, intent(inout) :: niotasks(:)
    integer, intent(in) :: nframes 
    integer, intent(in) :: nvars
    integer, intent(in) :: varsize
    logical, intent(in) :: unlimdimindof
    integer(kind=PIO_Offset_kind), pointer :: compmap(:)
    integer :: ntasks
    integer :: comm
    integer :: npe
    integer :: color
    integer(kind=PIO_Offset_kind) :: maplen, gmaplen
    integer :: ndims
    integer, pointer :: gdims(:)
    character(len=20) :: fname
    type(var_desc_t) :: vari(nvars), varr(nvars), vard(nvars)
    type(iosystem_desc_t) :: iosystem
    integer :: stride, n
    integer, allocatable :: ifld(:,:), ifld_in(:,:,:)
    real, allocatable :: rfld(:,:), rfld_in(:,:,:)
    double precision, allocatable :: dfld(:,:), dfld_in(:,:,:)
    type(file_desc_t) :: File
    type(io_desc_t) :: iodesc_i4, iodesc_r4, iodesc_r8
    integer :: ierr
    integer(kind=pio_offset_kind) :: frame=1, recnum
    integer :: iotype, rearr, rearrtype
    integer(kind=pio_offset_kind) :: j
    integer :: k, errorcnt
    character(len=PIO_MAX_NAME) :: varname
    integer, parameter :: MAX_TIMESTAMPS = 2
    double precision :: wall(MAX_TIMESTAMPS), sys(MAX_TIMESTAMPS),&
                        usr(MAX_TIMESTAMPS)
    integer :: niomin, niomax
    integer :: nv, mode
    integer,  parameter :: c0 = -1
    double precision, parameter :: cd0 = 1.0e30
    integer :: nvarmult
    character(len=*), parameter :: rearr_name(MAX_PIO_REARRS) = (/'   BOX','SUBSET'/)

    nullify(compmap)

    if(trim(filename) .eq. 'ROUNDROBIN' .or. trim(filename).eq.'BLOCK') then
       call init_ideal_dof(filename, mype, npe_base, ndims, gdims, compmap, varsize)
    else
       ! Changed to support PIO1 as well
#ifdef _PIO1
       call pio_readdof(filename, compmap, MPI_COMM_WORLD, 81, ndims, gdims)
#else
       call pio_readdof(filename, ndims, gdims, compmap, MPI_COMM_WORLD)
#endif

!    print *,__FILE__,__LINE__,' gdims=',ndims
    endif
    maplen = size(compmap)
!    color = 0
!    if(maplen>0) then
       color = 1
!    endif

    call MPI_Comm_split(MPI_COMM_WORLD, color, mype, comm, ierr)

    call MPI_Comm_size(comm, npe,  ierr)
    call CheckMPIreturn(__LINE__,ierr)
    niomin=1
    niomax=min(npe,MAX_IO_TASK_ARRAY_SIZE)
    if(niotasks(1)<=0) then
       do j=1,min(MAX_IO_TASK_ARRAY_SIZE, npe)
          niotasks(j)=npe-int(j)+1
       enddo
    endif

    if(mype < npe) then

       call MPI_ALLREDUCE(maplen,gmaplen,1,MPI_INTEGER8,MPI_SUM,comm,ierr)

!       if(gmaplen /= product(gdims)) then
!          print *,__FILE__,__LINE__,gmaplen,gdims
!       endif
    
       allocate(ifld(maplen,nvars))
       allocate(ifld_in(maplen,nvars,nframes))

       allocate(rfld(maplen,nvars))
       allocate(rfld_in(maplen,nvars,nframes))

       allocate(dfld(maplen,nvars))
       allocate(dfld_in(maplen,nvars,nframes))

       ifld = PIO_FILL_INT
       rfld = PIO_FILL_FLOAT
       dfld = PIO_FILL_DOUBLE
       do nv=1,nvars
          do j=1,maplen
             if(compmap(j) > 0) then
               ifld(j,nv) = int(compmap(j))
               dfld(j,nv) = ifld(j,nv)/1000000.0
               rfld(j,nv) = 1.0E5*ifld(j,nv)
             endif
          enddo
        enddo

#ifdef BGQTRY
  call print_memusage()
#endif

       do k=1,size(piotypes)
          iotype = piotypes(k)
          call MPI_Barrier(comm,ierr)
          if(mype==0) then
             !print *,'iotype=',piotypes(k)
          endif
!          if(iotype==PIO_IOTYPE_PNETCDF) then
!             mode = PIO_64BIT_DATA
!          else
             mode = 0
!          endif
          do rearrtype=1,2
             rearr = rearrangers(rearrtype)
             if(rearr /= PIO_REARR_SUBSET .and. rearr /= PIO_REARR_BOX) exit

             do n=niomin,niomax
                ntasks = niotasks(n)

                if(ntasks<=0 .or. ntasks>npe) exit
                stride = max(1,npe/ntasks)

                call pio_init(mype, comm, ntasks, 0, stride, PIO_REARR_SUBSET,&
                  iosystem, rearr_opts=rearr_opts)
                   
                write(fname, '(a,i1,a,i4.4,a,i1,a)') 'pioperf.',rearr,'-',ntasks,'-',iotype,'.nc'
		
                ierr =  PIO_CreateFile(iosystem, File, iotype, trim(fname), mode)

                call WriteMetadata(File, gdims, vari, varr, vard, unlimdimindof)

                call MPI_Barrier(comm,ierr)
                call get_tstamp(wall(1), usr(1), sys(1))

                if(.not. unlimdimindof) then
#ifdef VARINT
                   call PIO_InitDecomp(iosystem, PIO_INT, gdims, compmap, iodesc_i4, rearr=rearr)
#endif
#ifdef VARREAL
                   call PIO_InitDecomp(iosystem, PIO_REAL, gdims, compmap, iodesc_r4, rearr=rearr)
#endif
#ifdef VARDOUBLE
                   call PIO_InitDecomp(iosystem, PIO_DOUBLE, gdims, compmap, iodesc_r8, rearr=rearr)
#endif
                endif

                ! print *,__FILE__,__LINE__,minval(dfld),maxval(dfld),minloc(dfld),maxloc(dfld)

                do frame=1,nframes
                   recnum = frame
                   if( unlimdimindof) then
                      recnum = 1 + (frame-1)*gdims(ndims)
!                      compmap = compmap2 + (frame-1)*gdims(ndims)
!                      print *,__FILE__,__LINE__,compmap
#ifdef VARINT
                      call PIO_InitDecomp(iosystem, PIO_INT, gdims, compmap, iodesc_i4, rearr=rearr)
#endif
#ifdef VARREAL
                      call PIO_InitDecomp(iosystem, PIO_REAL, gdims, compmap, iodesc_r4, rearr=rearr)
#endif
#ifdef VARDOUBLE
                      call PIO_InitDecomp(iosystem, PIO_DOUBLE, gdims, compmap, iodesc_r8, rearr=rearr)
#endif
                   endif
                   !if(mype==0) print *,__FILE__,__LINE__,'Frame: ',recnum

                   do nv=1,nvars   
                      !if(mype==0) print *,__FILE__,__LINE__,'var: ',nv
#ifdef VARINT
                      call PIO_setframe(File, vari(nv), recnum)
                      call pio_write_darray(File, vari(nv), iodesc_i4, ifld(:,nv)    , ierr, fillval= PIO_FILL_INT)
#endif
#ifdef VARREAL
                      call PIO_setframe(File, varr(nv), recnum)
                      call pio_write_darray(File, varr(nv), iodesc_r4, rfld(:,nv)    , ierr, fillval= PIO_FILL_FLOAT)
#endif
#ifdef VARDOUBLE
                      call PIO_setframe(File, vard(nv), recnum)
                      call pio_write_darray(File, vard(nv), iodesc_r8, dfld(:,nv)    , ierr, fillval= PIO_FILL_DOUBLE)
#endif
                   enddo
                   if(unlimdimindof) then
#ifdef VARREAL                
                      call PIO_freedecomp(File, iodesc_r4)
#endif
#ifdef VARDOUBLE
                      call PIO_freedecomp(File, iodesc_r8)
#endif
#ifdef VARINT
                      call PIO_freedecomp(File, iodesc_i4)
#endif                
                   endif
                enddo
                call pio_closefile(File)


                call MPI_Barrier(comm,ierr)

                call get_tstamp(wall(2), usr(2), sys(2))
                wall(1) = wall(2)-wall(1)
                call MPI_Reduce(wall(1), wall(2), 1, MPI_DOUBLE_PRECISION, MPI_MAX, 0, comm, ierr)
                if(mype==0) then
                   ! print out performance in MB/s
                   nvarmult = 0
#ifdef VARINT
                   nvarmult = nvarmult+1
#endif
#ifdef VARREAL
                   nvarmult = nvarmult+1
#endif
#ifdef VARDOUBLE
                   nvarmult = nvarmult+2
#endif
                   write(*,'(a15,a9,i10,i10,i10,f20.10)') &	
                   'RESULT: write ',&
                    rearr_name(rearr), piotypes(k), ntasks, nvars, &
                    nvarmult*nvars*nframes*gmaplen*4.0D0/(1048576.0*wall(2))
#ifdef BGQTRY
  call print_memusage()
#endif
                end if
! Now the Read
                ierr = PIO_OpenFile(iosystem, File, iotype, trim(fname), mode=PIO_NOWRITE);
                do nv=1,nvars
#ifdef VARINT
                   write(varname,'(a,i4.4)') 'vari',nv
                   ierr =  pio_inq_varid(File, varname, vari(nv))
#endif
#ifdef VARREAL
                   write(varname,'(a,i4.4)') 'varr',nv
                   ierr =  pio_inq_varid(File, varname, varr(nv))
#endif
#ifdef VARDOUBLE
                   write(varname,'(a,i4.4)') 'vard',nv
                   ierr =  pio_inq_varid(File, varname, vard(nv))
#endif
                enddo

                if( unlimdimindof) then
#ifdef VARINT
                   call PIO_InitDecomp(iosystem, PIO_INT, gdims, compmap, iodesc_i4, rearr=rearr)
#endif
#ifdef VARREAL
                   call PIO_InitDecomp(iosystem, PIO_REAL, gdims, compmap, iodesc_r4, rearr=rearr)
#endif
#ifdef VARDOUBLE
                   call PIO_InitDecomp(iosystem, PIO_DOUBLE, gdims, compmap, iodesc_r8, rearr=rearr)
#endif
                endif


                call MPI_Barrier(comm,ierr)
                call get_tstamp(wall(1), usr(1), sys(1))
                
                do frame=1,nframes                   
                   do nv=1,nvars
#ifdef VARINT
                      call PIO_setframe(File, vari(nv), frame)
                      call pio_read_darray(File, vari(nv), iodesc_i4, ifld_in(:,nv,frame), ierr)
#endif
#ifdef VARREAL
                      call PIO_setframe(File, varr(nv), frame)
                      call pio_read_darray(File, varr(nv), iodesc_r4, rfld_in(:,nv,frame), ierr)
#endif
#ifdef VARDOUBLE
                      call PIO_setframe(File, vard(nv), frame)
                      call pio_read_darray(File, vard(nv), iodesc_r8, dfld_in(:,nv,frame), ierr)
#endif
                   enddo
                enddo
                
                call pio_closefile(File)
                call MPI_Barrier(comm,ierr)
                call get_tstamp(wall(2), usr(2), sys(2))
                wall(1) = wall(2)-wall(1)
                call MPI_Reduce(wall(1), wall(2), 1, MPI_DOUBLE_PRECISION, MPI_MAX, 0, comm, ierr)
                errorcnt = 0
                do frame=1,nframes
                   do nv=1,nvars
                      do j=1,maplen
                         if(compmap(j)>0) then
#ifdef VARINT
#ifdef DEBUG
                             write(*,'(a11,i2,a9,i11,a9,i11,a9,i2)') & 
			        ' Int    PE=',mype,'ifld=',ifld(j,nv),' ifld_in=',ifld_in(j,nv,frame),' compmap=',compmap(j)
#endif
                            if(ifld(j,nv) /= ifld_in(j,nv,frame)) then
                               !if(errorcnt < 10) then
                               !   print *,__LINE__,'Int: ',mype,j,nv,ifld(j,nv),ifld_in(j,nv,frame),compmap(j)
                               !endif
                               write(*,*) '***ERROR:Mismatch!***'
                               write(*,'(a11,i2,a9,i11,a9,i11,a9,i2)') & 
			         ' Int    PE=',mype,'ifld=',ifld(j,nv),' ifld_in=',ifld_in(j,nv,frame),' compmap=',compmap(j)

                               errorcnt = errorcnt+1
                            endif
#endif
#ifdef VARREAL
#ifdef DEBUG
                            write(*,'(a11,i2,a9,f11.2,a9,f11.2,a9,i2)') &
			        ' Real   PE=',mype,'rfld=',rfld(j,nv),' rfld_in=',rfld_in(j,nv,frame),' compmap=',compmap(j)
#endif
                            
                            if(rfld(j,nv) /= rfld_in(j,nv,frame) ) then
                               !if(errorcnt < 10) then
                               !   print *,__LINE__,'Real:', mype,j,nv,rfld(j,nv),rfld_in(j,nv,frame),compmap(j)
                               !endif
                               write(*,*) '***ERROR:Mismatch!***'
                               write(*,'(a11,i2,a9,f11.2,a9,f11.2,a9,i2)') &
			         ' Real   PE=',mype,'rfld=',rfld(j,nv),' rfld_in=',rfld_in(j,nv,frame),' compmap=',compmap(j)

                               errorcnt = errorcnt+1                           
                            endif
#endif
#ifdef VARDOUBLE
#ifdef DEBUG
                            write(*,'(a11,i2,a9,d11.4,a9,d11.4,a9,i2)') &
			        'Double PE=',mype,'dfld=',dfld(j,nv),'dfld_in=',dfld_in(j,nv,frame),'compmap=',compmap(j)
#endif
                            if(dfld(j,nv) /= dfld_in(j,nv,frame) ) then
                               !if(errorcnt < 10) then
                               !   print *,__LINE__,'Dbl:',mype,j,nv,dfld(j,nv),dfld_in(j,nv,frame),compmap(j)
                               !endif
                               write(*,*) '***ERROR:Mismatch!***'
                               write(*,'(a11,i2,a9,d11.4,a9,d11.4,a9,i2)') &
			        'Double PE=',mype,'dfld=',dfld(j,nv),'dfld_in=',dfld_in(j,nv,frame),'compmap=',compmap(j)

                               errorcnt = errorcnt+1
                            endif
#endif
                         endif
                      enddo
                   enddo
                enddo
                j = errorcnt
                call MPI_Reduce(j, errorcnt, 1, MPI_INTEGER, MPI_SUM, 0, comm, ierr)
                
                if(mype==0) then
                   if(errorcnt > 0) then
                      print *,'ERROR: INPUT/OUTPUT data mismatch ',errorcnt
                   endif
                   nvarmult = 0
#ifdef VARINT
                   nvarmult = nvarmult+1
#endif
#ifdef VARREAL
                   nvarmult = nvarmult+1
#endif
#ifdef VARDOUBLE
                   nvarmult = nvarmult+2
#endif
                   write(*,'(a15,a9,i10,i10,i10,f20.10)') &
                        'RESULT: read ',&
                        rearr_name(rearr), piotypes(k), ntasks, nvars, &
                        nvarmult*nvars*nframes*gmaplen*4.0D0/(1048576.0*wall(2))
#ifdef BGQTRY 
  call print_memusage()
#endif
                end if
#ifdef VARREAL                
                call PIO_freedecomp(iosystem, iodesc_r4)
#endif
#ifdef VARDOUBLE
                call PIO_freedecomp(iosystem, iodesc_r8)
#endif
#ifdef VARINT
                call PIO_freedecomp(iosystem, iodesc_i4)
#endif                
                call pio_finalize(iosystem, ierr)
             enddo
          enddo
       enddo
!       deallocate(compmap)
       deallocate(ifld)
       deallocate(ifld_in)
       deallocate(rfld)
       deallocate(dfld)
       deallocate(dfld_in)
       deallocate(rfld_in)
    endif

  end subroutine pioperformance_rearrtest

  subroutine init_ideal_dof(doftype, mype, npe, ndims, gdims, compmap, varsize)
    use pio
    use pio_support, only : piodie
    character(len=*), intent(in) :: doftype
    integer, intent(in) :: mype
    integer, intent(in) :: npe
    integer, intent(out) :: ndims
    integer, pointer :: gdims(:)
    integer(kind=PIO_Offset_kind), pointer :: compmap(:)
    integer, intent(in) :: varsize
    integer :: i

    ndims = 1
    allocate(gdims(1))
    gdims(1) = npe*varsize

    allocate(compmap(varsize))
    if(doftype .eq. 'ROUNDROBIN') then
       do i=1,varsize
          compmap(i) = (i-1)*npe+mype+1 
       enddo
    else if(doftype .eq. 'BLOCK') then
       do i=1,varsize
          compmap(i) =  (i+varsize*mype)
       enddo
    endif
    if(minval(compmap)< 1 .or. maxval(compmap) > gdims(1)) then
       print *,__FILE__,__LINE__,trim(doftype),varsize,minval(compmap),maxval(compmap)
       call piodie(__FILE__,__LINE__,'Compmap out of bounds')
    endif
  end subroutine init_ideal_dof


  subroutine WriteMetadata(File, gdims, vari, varr, vard,unlimdimindof)
    use pio
    type(file_desc_t) :: File
    integer, intent(in) :: gdims(:)
    type(var_desc_t),intent(out) :: vari(:), varr(:), vard(:)
    logical, intent(in) :: unlimdimindof
    integer :: ndims
    character(len=PIO_MAX_NAME) :: dimname
    character(len=PIO_MAX_NAME) :: varname
    integer, allocatable :: dimid(:)
    integer :: i, iostat, nv
    integer :: nvars

    nvars = size(vari)

    ndims = size(gdims)
    if(unlimdimindof) then
       ndims=ndims-1
   endif
   allocate(dimid(ndims+1))

   do i=1,ndims

      write(dimname,'(a,i6.6)') 'dim',i  
      iostat = PIO_def_dim(File, trim(dimname), int(gdims(i),pio_offset_kind), dimid(i))
   enddo
   iostat = PIO_def_dim(File, 'time', PIO_UNLIMITED, dimid(ndims+1))

    do nv=1,nvars
#ifdef VARINT
       write(varname,'(a,i4.4)') 'vari',nv
       iostat = PIO_def_var(File, varname, PIO_INT, dimid, vari(nv))
       iostat = PIO_put_att(File, vari(nv), "_FillValue", PIO_FILL_INT);
#endif
#ifdef VARREAL
       write(varname,'(a,i4.4)') 'varr',nv
       iostat = PIO_def_var(File, varname, PIO_REAL, dimid, varr(nv))
       iostat = PIO_put_att(File, varr(nv), "_FillValue", PIO_FILL_FLOAT);
#endif
#ifdef VARDOUBLE
       write(varname,'(a,i4.4)') 'vard',nv
       iostat = PIO_def_var(File, varname, PIO_DOUBLE, dimid, vard(nv))
       iostat = PIO_put_att(File, vard(nv), "_FillValue", PIO_FILL_DOUBLE);
#endif
    enddo

    iostat = PIO_enddef(File)

  end subroutine WriteMetadata


!=============================================
!  CheckMPIreturn:
!
!      Check and prints an error message
!  if an error occured in a MPI subroutine.
!=============================================
  subroutine CheckMPIreturn(line,errcode)
#ifndef NO_MPIMOD
    use mpi
#endif
    implicit none
#ifdef NO_MPIMOD
#include <mpif.h>
#endif  
    integer, intent(in) :: errcode
    integer, intent(in) :: line
    character(len=MPI_MAX_ERROR_STRING) :: errorstring
    
    integer :: errorlen
    
    integer :: ierr
    
    if (errcode .ne. MPI_SUCCESS) then
       call MPI_Error_String(errcode,errorstring,errorlen,ierr)
       write(*,*) "ERROR: ", errorstring(1:errorlen), " : at line ", line
    end if
  end subroutine CheckMPIreturn

end program pioperformance_rearr
