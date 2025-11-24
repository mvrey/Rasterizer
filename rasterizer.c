#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include <SDL/SDL.h>
#include "sdl_funcs.h"
#include "chrono.h"

int cpu_mhz = 3400;

inline static void ChronoShow ( char* name, int computations)
{
  float ms = ChronoWatchReset();
  float cycles = ms * (1000000.0f/1000.0f) * (float)cpu_mhz;
  float cyc_per_comp = cycles / (float)computations;
  fprintf ( stdout, "%s: %f ms, %d cycles, %f cycles/iteration\n", name, ms, (int)cycles, cyc_per_comp);
}

#define LCF (200.0f)    // En flotantes
#define PI 3.1416f
#define DEBUG 0
#define ASSEMBLY 0

//Encapsulated point
typedef struct {
  int x,y,z;
  int used;
} Point;


//Point "constructor"
inline static Point* makePoint(int x, int y, int z) {
  Point* p = malloc(sizeof(Point));
  p->x = x;
  p->y = y;
  p->z = z;
  return p;
}


// Floats
static float cubef [24] = {
    -LCF,-LCF, LCF,
     LCF,-LCF, LCF,
     LCF, LCF, LCF,
    -LCF, LCF, LCF,

    -LCF,-LCF, -LCF,
     LCF,-LCF, -LCF,
     LCF, LCF, -LCF,
    -LCF, LCF, -LCF,
};



/// Sets the color of a pixel
static inline void setColorPixel(unsigned int* pixels, int x, int y, int color) {
  pixels [ x + y] = color;
}


/// Shortcuts for the rotation matrix for a given axis. Axis => 0=x, 1=y, 2=z
/// Returns the rotated point
static Point rotatePoint(Point p, float rads, int axis) {
  float sinr = sin(rads);
  float cosr = cos(rads);
  Point trans;

  switch(axis) {
    // X-AXIS ROTATION
    case 0:
      trans.x = p.x;
      trans.y = p.y*cosr - p.z*sinr;
      trans.z = p.y*sinr + p.z*cosr;
      break;
    // Y-AXIS ROTATION
    case 1:
      trans.x = p.z*sinr + p.x*cosr;
      trans.y = p.y;
      trans.z = p.z*cosr - p.x*sinr;
      break;
    // Z-AXIS ROTATION (2D)
    case 2:
      trans.x = p.x*cosr - p.y*sinr;
      trans.y = p.x*sinr + p.y*cosr;
      trans.z = p.z;
      break;
    default:
      trans = p;
      break;
  }

  trans.used = 0;

  return trans;
}

#if ASSEMBLY
extern int min(int n1, int n2, int n3, int n4);
extern int max(int n1, int n2, int n3, int n4);
#else
static inline int min(int n1, int n2, int n3, int n4) {
  float minn = n1;

  if (n2 < minn) minn = n2;
  if (n3 < minn) minn = n3;
  if (n4 < minn) minn = n4;

  return minn;
}


static inline int max(int n1, int n2, int n3, int n4) {
  float maxn = n1;

  if (n2 > maxn) maxn = n2;
  if (n3 > maxn) maxn = n3;
  if (n4 > maxn) maxn = n4;

  return maxn;
}
#endif


static void rasterize(unsigned int* pixels, Point** p, int pitch, int color) {
  int x,y;

  //Use halfspace line equation method
  int y1 = p[0]->y;
  int y2 = p[1]->y;
  int y3 = p[2]->y;
  int y4 = p[3]->y;

  int x1 = p[0]->x;
  int x2 = p[1]->x;
  int x3 = p[2]->x;
  int x4 = p[3]->x;

  int minx = (int) min(x1,x2,x3,x4);
  int maxx = (int) max(x1,x2,x3,x4);
  int miny = (int) min(y1,y2,y3,y4);
  int maxy = (int) max(y1,y2,y3,y4);

  int x1x2 = x1-x2;
  int x2x3 = x2-x3;
  int x3x4 = x3-x4;
  int x4x1 = x4-x1;

  int y1y2 = y1-y2;
  int y2y3 = y2-y3;
  int y3y4 = y3-y4;
  int y4y1 = y4-y1;

  int x1x2y, x2x3y, x3x4y, x4x1y;
  int pitched_y;
  
  for (y=miny; y< maxy; y++) {
    x1x2y = x1x2 * (y-y1);
    x2x3y = x2x3 * (y-y2);
    x3x4y = x3x4 * (y-y3);
    x4x1y = x4x1 * (y-y4);

	  pitched_y = y * pitch;
	
    for (x=minx; x< maxx; x++) {
      if (x1x2y - (y1y2 * (x - x1)) < 0 &&
          x2x3y - (y2y3 * (x - x2)) < 0 &&
          x3x4y - (y3y4 * (x - x3)) < 0 &&
          x4x1y - (y4y1 * (x - x4)) < 0) {
        setColorPixel(pixels, x, pitched_y, color);
      }
    }
  }
}


static void PaintCubeInFloat ( unsigned int* pixels, float w, float h, int pitch, float trans_x, float trans_z, float proy, float rot) {
  int i,j;

  Point* points[8];
  int base_index;

  //Indices for drawing quads counter-clockwise
  int indices[24] = { 0,1,2,3,
                      7,6,5,4,
                      0,4,5,1,
                      2,6,7,3,
                      1,5,6,2,
                      3,7,4,0 };

  //Transform and paint all vertices in square

  float rot_a = rot*1.5f;
  float rot_b = rot*2.0f;
  float rot_c = rot;

  for ( i=0; i<8; i++) {
    float xp, yp;
    base_index = i * 3;
    float x = cubef [ base_index + 0];
    float y = cubef [ base_index + 1];
    float z = cubef [ base_index + 2];

    //Construct point
    Point* p = makePoint(x, y, z);

    //Rotate in all axis
    *p = rotatePoint(*p, rot_a, 0);
    *p = rotatePoint(*p, rot_b, 1);
    *p = rotatePoint(*p, rot_c, 2);

    //project Z axis on 2 dimensions
    p->z -= trans_z;
    float inv_z = 1.0f / p->z;
    xp = (p->x * proy) * inv_z;
    yp = (p->y * proy) * inv_z;

    //translate points to the middle of the screen
    xp += w * 0.5f;
    yp += h * 0.5f;

    //Save point
    points[i] = makePoint(xp,yp,p->z);
    free(p);
  }

  int colors[6] = { 0xFF0000, 0x00FF00, 0x0000FF, 0xFFFF00, 0xFF00FF, 0x00FFFF };
  
  //RASTERIZE POLYGONS
  for (i=0,j=0; i<24; i+=4,j++) {
    Point* sqp[4] = { points[indices[i + 0]], points[indices[i + 1]], points[indices[i + 2]], points[indices[i + 3]] };
    rasterize(pixels, sqp, pitch, colors[j]);
  }

  for ( i=0; i<8; i++) {
    free (points[i]);
  }

}


/*******************************/
/********* MAIN CODE  **********/
/*******************************/


int main ( int argc, char **argv) {

  init_SDL();

  // Horizontal field of view
  float hfov = 60.0f * ((PI * 2.0f) / 360.0f);  // De grados a radianes
  // Proyeccion usando la tangente
  float half_scr_w = (float)(g_SDLSrf->w >> 1);
  float projection = (1.0f / tan ( hfov * 0.5f)) * half_scr_w;

  float ang = 0;

  g_end = 0;
  while (!g_end) {
    clear_SDL();

    int pitch = g_SDLSrf->pitch >> 2;


    // Girar los cubos en pantalla
    float x, z, offs_z = 1200.0f;
    float radius = 100.0f;
    x = 0.0f   + radius;
    z = offs_z + radius;


    ChronoWatchReset();

    PaintCubeInFloat ( g_screen_pixels, (float)g_SDLSrf->w, (float)g_SDLSrf->h, pitch,
                       x, z,
                       projection, ang);

    ChronoShow ( "Paint cube : ", g_SDLSrf->w * g_SDLSrf->h);
    
    if (g_keydown == 1)
      ang += 0.0005f;

    frame_SDL();

    input_SDL();

    //SDL_Delay(40.0f);
  }

  return 0;
}
