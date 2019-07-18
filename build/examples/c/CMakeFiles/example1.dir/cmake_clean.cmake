FILE(REMOVE_RECURSE
  "CMakeFiles/example1.dir/example1.c.o"
  "example1.pdb"
  "example1"
)

# Per-language clean rules from dependency scanning.
FOREACH(lang C)
  INCLUDE(CMakeFiles/example1.dir/cmake_clean_${lang}.cmake OPTIONAL)
ENDFOREACH(lang)
