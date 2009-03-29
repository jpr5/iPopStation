/*
 * $Id$
 */

// this is the program the generate the pictureflow table
#include <stdio.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define PFREAL_ONE 1024
#define IANGLE_MAX 1024

int main(int, char**)
{
  FILE*f = fopen("table.c","wt");
  fprintf(f,"PFreal sinTable[] = {\n");
  for(int i = 0; i < 128; i++)
  {
    for(int j = 0; j < 8; j++)
    {
      int iang = j+i*8;
      double ii = (double)iang + 0.5;
      double angle = ii * 2 * M_PI / IANGLE_MAX;
      double sinAngle = sin(angle);
      fprintf(f,"%6d, ", (int)(floor(PFREAL_ONE*sinAngle)));
    }
    fprintf(f,"\n");
  }
  fprintf(f,"};\n");
  fclose(f);

  return 0;
}
