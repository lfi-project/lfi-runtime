project('argtable3', 'c')

srcs = files(
  'src/arg_cmd.c',
  'src/arg_date.c',
  'src/arg_dbl.c',
  'src/arg_dstr.c',
  'src/arg_end.c',
  'src/arg_file.c',
  'src/arg_getopt_long.c',
  'src/arg_hashtable.c',
  'src/arg_int.c',
  'src/arg_lit.c',
  'src/arg_rem.c',
  'src/arg_rex.c',
  'src/arg_str.c',
  'src/arg_utils.c',
  'src/argtable3.c',
)

libargtable = static_library('argtable3', srcs)

argtable_dep = declare_dependency(
  link_with: libargtable,
  include_directories: include_directories('src'),
)
