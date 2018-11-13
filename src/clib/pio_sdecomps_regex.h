#ifndef __PIO_SDECOMPS_REGEX_H___
#define __PIO_SDECOMPS_REGEX_H___

/* The C API that matches user specified regular expression to save
 * I/O decomposition with the ioid, file and variables names provided
 * @param ioid : The I/O decomposition ID to match
 * @param fname : File name (C string) to match
 * @param vname : Variable name (C string) to match
 * @returns true if the specified ioid, file and variable name matches the 
 * previously specified regular expression, false otherwise
 */
bool pio_save_decomps_regex_match(int ioid, const char *fname, const char *vname);

#endif /* __PIO_SDECOMPS_REGEX_H__ */
