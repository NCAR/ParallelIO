extern "C"{
#include "pio.h"
#include "pio_internal.h"
#include "pio_sdecomps_regex.h"
}

bool pio_save_decomps_regex_match(int ioid, const char *fname, const char *vname)
{
  /* FIXME: We don't support parsing fname and vname */
  if(fname || vname){
    return false;
  }

  /* FIXME: All ioids are written out */
  return true;
}
