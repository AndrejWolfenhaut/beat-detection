#include <stdio.h>
#include <stdlib.h>
#include <aubio/aubio.h>
#include <pipewire/pipewire.h>



#define PRINT_MSG(...) fprintf(stdout, __VA_ARGS__)
#define PRINT_WARN(...) fprintf(stderr, __VA_ARGS__)



int main (int argc, char **argv) {
  pw_init(&argc, &argv);

  PRINT_MSG(
    "Compiled with libpipewire %s\nLinked with libpipewire %s\n",
    pw_get_headers_version(),
    pw_get_library_version()
  );

  return 0;
}
