
extern "C" {
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_IMAGE_H
#include FT_OUTLINE_H
}

#include <math.h>
#include <sndfile.h>
#include <inttypes.h>
#include <vector>

FT_Library  library;
FT_Face     face;

SNDFILE* sndfile;

int cx,cy;
double xchar_offset;
double ychar_offset = -2500;

const float step = 60;

uint32_t t=0;
uint32_t dt;

int xmin=-32768/2, xmax=32767/2;
int ymin=-32768/2, ymax=32767/2;

int xsum=0, ysum=0;

void outpt(int x,int y, int n=1)
{
  if (x<xmin || x>xmax ||
      y<ymin || y>ymax) {
    return;
  }

  //printf("XY %d %d\n",x,y);

  short s[2*n];
  for (int i=0;i<n;i++) {
    s[2*i + 0]=x;
    s[2*i + 1]=y;
  }

  xsum += x*n;
  ysum += y*n;

  t+=n;

  sf_writef_short(sndfile, s, n);
}

void outpt_force(int x,int y, int n=1)
{
  short s[2*n];
  for (int i=0;i<n;i++) {
    s[2*i + 0]=x;
    s[2*i + 1]=y;
  }

  xsum += x*n;
  ysum += y*n;

  t+=n;

  sf_writef_short(sndfile, s, n);
}

void equalize()
{
  while (xsum<=xmin || xsum>=xmax ||
         ysum<=ymin || ysum>=ymax) {
    int x = -xsum;
    if (x<-32767) x=-32767;
    if (x> 32767) x= 32767;
    int y = -ysum;
    if (y<-32767) y=-32767;
    if (y> 32767) y= 32767;

    outpt_force(x,y);
  }
}

int move_to(const FT_Vector* to, void*)
{
  //printf("move %d %d\n",to->x,to->y);
  for (int i=0;i<5;i++) {
    outpt(cx+xchar_offset,cy+ychar_offset);
  }

  cx = to->x;
  cy = to->y;

  for (int i=0;i<5;i++) {
    outpt(cx+xchar_offset,cy+ychar_offset);
  }

  return 0;
}

int line_to(const FT_Vector* to, void*)
{
  //printf("line %d %d\n",to->x,to->y);

  float len = hypot(to->x-cx, to->y-cy);
  int n = len/step;
  if (n==0) n=1;

  for (int i=0;i<=n;i++) {
    outpt((int)(cx+(to->x-cx)*i/n + xchar_offset), cy+(to->y-cy)*i/n + ychar_offset);
  }

  cx = to->x;
  cy = to->y;

  return 0;
}

void subdiv(float x0,float y0, float x1,float y1,  float x2,float y2)
{
  if (hypot(x2-x0,y2-y0)>step*1.5) {
    float xa = (x0+x1)/2;
    float ya = (y0+y1)/2;

    float xb = (x1+x2)/2;
    float yb = (y1+y2)/2;

    float xn = (xa+xb)/2;
    float yn = (ya+yb)/2;

    subdiv(x0,y0, xa,ya, xn,yn);
    outpt((int)(xn+xchar_offset),(int)(yn+ychar_offset));
    subdiv(xn,yn, xb,yb, x2,y2);
  }
}

int conic_to(const FT_Vector* control, const FT_Vector* to, void*)
{
  //printf("conic %d %d   %d %d\n",control->x,control->y,  to->x,to->y);

  subdiv(cx,cy, control->x,control->y, to->x,to->y);

  cx = to->x;
  cy = to->y;

  return 0;
}

void subdiv_q(float x0,float y0, float x1,float y1,  float x2,float y2, float x3,float y3)
{
  if (hypot(x3-x0,y3-y0)>step*1.5) {
    float xa = (x0+x1)/2;
    float ya = (y0+y1)/2;

    float xb = (x1+x2)/2;
    float yb = (y1+y2)/2;

    float xc = (x2+x3)/2;
    float yc = (y2+y3)/2;

    float xA = (xa+xb)/2;
    float yA = (ya+yb)/2;

    float xB = (xb+xc)/2;
    float yB = (yb+yc)/2;

    float xn = (xA+xB)/2;
    float yn = (yA+yB)/2;

    subdiv_q(x0,y0, xa,ya, xA,yA, xn,yn);
    outpt((int)(xn+xchar_offset),(int)(yn+ychar_offset));
    subdiv_q(xn,yn, xB,yB, xc,yc, x3,y3);
  }
}

int cubic_to(const FT_Vector* control1, const FT_Vector* control2, const FT_Vector* to, void*)
{
  //printf("cubic %d %d   %d %d\n",control1->x,control1->y,  control2->x,control2->y,  to->x,to->y);

  subdiv_q(cx,cy, control1->x,control1->y, control2->x,control2->y, to->x,to->y);

  cx = to->x;
  cy = to->y;

  return 0;
}



bool traceText(const char* text, double xstart)
{
  bool textover = true;

  xchar_offset = xstart;

  for (int i=0; text[i]; i++) {
    FT_UInt glyph_index = FT_Get_Char_Index( face, text[i] );
    //printf("glyph index= %d\n",glyph_index);

    FT_Error error = FT_Load_Glyph(face,          /* handle to face object */
                                   glyph_index,   /* glyph index           */
                                   FT_LOAD_NO_BITMAP );  /* load flags, see below */

    //printf("error=%d\n",error);
    //printf("format %x %x\n",face->glyph->format, FT_GLYPH_FORMAT_OUTLINE);

    FT_Outline outline = face->glyph->outline;

    /*
    printf("adv:%d;%d\n",face->glyph->advance.x, face->glyph->advance.y);
    printf("p:%d c:%d\n",outline.n_points, outline.n_contours);
    for (int i=0;i<outline.n_points;i++) {
      printf("%3d: %d %d %x\n",i,outline.points[i].x,outline.points[i].y, outline.tags[i]);
    }
    */

    FT_Outline_Funcs funcs;
    funcs.move_to = move_to;
    funcs.line_to = line_to;
    funcs.conic_to = conic_to;
    funcs.cubic_to = cubic_to;
    funcs.shift = 0;
    funcs.delta = 0;

    //printf("%f\n",xchar_offset);
    if (xchar_offset + face->glyph->advance.x > xmin && xchar_offset < xmax) {
      FT_Outline_Decompose(&outline, &funcs, NULL);
    }

    if (xchar_offset + face->glyph->advance.x > xmin)
      textover=false;

    xchar_offset += face->glyph->advance.x;
  }

  return textover;
}


struct particle
{
  bool firstinit=true;
  bool init=false;
  double x,y;
  double vx,vy;

  enum { up, explo } type = up;
  int counter = 0;

  bool recycle = false;
};

std::vector<particle> particles;

const double speedfactor = 1/500.0f;

void explode(double x,double y)
{
  int n=(rand()%10)+5;

  for (int i=0;i<n;i++) {
    particle p;
    p.x = x;
    p.y = y;
    double a = (rand()%360)*M_PI/180;
    double s = (rand()%20) + 10;
    p.vx = cos(a)*s;
    p.vy = sin(a)*s;
    p.type = particle::explo;
    p.counter = (rand()%20) + 50;
    p.init = true;
    p.firstinit=false;

    particles.push_back(p);
  }
}

void traceFireworks()
{
  for (auto& p : particles) {
    if (!p.init) {
      p.x = (rand()%10000) - 5000;
      p.y = -16000;

      if (p.firstinit) {
        p.counter = (rand()%2000);
        p.firstinit=false;
      }

      p.vx = (rand()%50) - 25;
      p.vy = (rand()%20) + 80;
      p.init = true;
    }

    //printf("%f %f\n",p.x,p.y);

    if (p.type == particle::up) {
      if (p.counter>0) {
        p.counter--;
      }
      else {
        p.x += p.vx * dt * speedfactor;
        p.y += p.vy * dt * speedfactor;

        outpt(p.x,p.y, 300);

        p.vy -= dt*speedfactor * 0.2;

        if (p.vy < 1) {
          p.init=false;
          p.counter = (rand()%1000);

          explode(p.x,p.y);
        }
      }
    }
    else if (p.type == particle::explo) {
      p.x += p.vx * dt * speedfactor;
      p.y += p.vy * dt * speedfactor;

      outpt(p.x,p.y, 100);

      p.vy -= dt*speedfactor * 0.25;

      p.counter--;
      if (p.counter <= 0) {
        p.recycle = true;
      }
    }

    if (p.y > ymax) { p.init=false; }
  }



  std::vector<particle> pnew;
  for (auto p : particles) {
    if (!p.recycle) {
      pnew.push_back(p);
    }
  }

  particles = pnew;
}


void traceBorder()
{
  FT_Vector a; a.x=xmin; a.y=ymin;
  FT_Vector b; b.x=xmin; b.y=ymax;
  FT_Vector c; c.x=xmax; c.y=ymax;
  FT_Vector d; d.x=xmax; d.y=ymin;

  move_to(&a,NULL);
  line_to(&b,NULL);
  line_to(&c,NULL);
  line_to(&d,NULL);
  line_to(&a,NULL);
}


int main()
{
  FT_Error error = FT_Init_FreeType( &library );
  if (error) {
      exit(5);
  }


  error = FT_New_Face( library,
                       //"fonts/BebasNeue Bold.ttf",
                       "fonts/SourceSansPro-BoldIt.otf",
                       //"/usr/share/fonts/truetype/msttcorefonts/Arial.ttf",
                       0,
                       &face );
  if ( error == FT_Err_Unknown_File_Format )
    {
      printf("a\n");
      exit(5);
    }
  else if ( error )
    {
      printf("b\n");
      exit(5);
    }


  error = FT_Set_Char_Size(face,    /* handle to face object           */
                           0,       /* char_width in 1/64th of points  */
                           2000, //16*64,   /* char_height in 1/64th of points */
                           300,     /* horizontal device resolution    */
                           300 );   /* vertical device resolution      */


  SF_INFO sfinfo;
  sfinfo.samplerate = 96000;
  sfinfo.channels = 2;
  sfinfo.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;
  sndfile = sf_open("out.wav", SFM_WRITE, &sfinfo);


  particles.resize(3);

  double textshift = 32000;

  const char* text =
    "Happy New Year 2017 !!!  "
    //"Happy New Year 2017 !!!  "
    //"Happy New Year 2017 !!!  "
    ;

  int64_t total_t = 1024*1024*50;

  srand(1000);

  bool text_over = false;
  while (!text_over) {
    dt = t;
    t=0;
    total_t -= dt;
    printf("%ld\n",total_t);

    if (dt==0) dt=100;

    textshift -= dt*speedfactor * 25;
    text_over = traceText(text, textshift);

    equalize();
  }

  while (total_t >= 0)  {
    dt = t;
    t=0;
    total_t -= dt;
    printf("%ld\n",total_t);

    traceFireworks();
    equalize();
  }

    //traceBorder();
    //equalize();

  sf_close(sndfile);

  printf("success\n");
  return 0;
}
