!|||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||

 module kinds_mod

!
! !MODULE: kinds_mod
!
! !DESCRIPTION:
!  This module defines default numerical data types for all common data
!  types like integer, character, logical, real4 and real8.
!
! !REVISION HISTORY:
!  CVS:$Id: kinds_mod.F90,v 1.1.1.1 2006/07/31 16:15:30 dennis Exp $
!  CVS:$Name:  $

! !USES:
!  uses no other modules

   implicit none
   private
   save

! !DEFINED PARAMETERS:

   integer, parameter, public ::          &
      char_len  = 100                    ,&
      log_kind  = kind(.true.)           ,&
      int_kind  = kind(1)                ,&
      i4        = selected_int_kind(6)   ,&
      i8        = selected_int_kind(13)  ,&
      r4        = selected_real_kind(6)  ,&
      r8        = selected_real_kind(13)

!
!
!
!***********************************************************************

 end module kinds_mod

!|||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
