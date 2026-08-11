/* */
#include <math.h>

int main(int argc, char** argv)
{
  (void)argv;
#ifndef log
  return ((int*)(&log))[argc];
#else
  (void)argc;
  return 0;
#endif
}
