//3350
//program: walk.cpp
//author:  Gordon Griesel
//date:    summer 2017 - 2018
//modified by: Alberto Acevedo, Sergio Benavides, Guadalupe Nunez
//
//Walk cycle using a sprite sheet.
//images courtesy: http://games.ucla.edu/resource/walk-cycles/
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <GL/glx.h>
#include "fonts.h"
#include <string>

//defined types
typedef double Flt;
typedef double Vec[3];
typedef Flt	Matrix[4][4];

//macros
#define rnd() (((double)rand())/(double)RAND_MAX)
#define random(a) (rand()%a)
#define MakeVector(x, y, z, v) (v)[0]=(x),(v)[1]=(y),(v)[2]=(z)
#define VecCopy(a,b) (b)[0]=(a)[0];(b)[1]=(a)[1];(b)[2]=(a)[2]
#define VecDot(a,b)	((a)[0]*(b)[0]+(a)[1]*(b)[1]+(a)[2]*(b)[2])
#define VecSub(a,b,c) (c)[0]=(a)[0]-(b)[0]; \
                      (c)[1]=(a)[1]-(b)[1]; \
                      (c)[2]=(a)[2]-(b)[2]
//constants
const float timeslice = 1.0f;
const float gravity = -0.2f;
#define ALPHA 1


class Image {
public:
	int width, height;
	unsigned char *data;
	~Image() { delete [] data; }
	Image(const char *fname) {
		if (fname[0] == '\0')
			return;
		//printf("fname **%s**\n", fname);
		char name[40];
		strcpy(name, fname);
		int slen = strlen(name);
		name[slen-4] = '\0';
		//printf("name **%s**\n", name);
		char ppmname[80];
		sprintf(ppmname,"%s.ppm", name);
		//printf("ppmname **%s**\n", ppmname);
		char ts[100];
		//system("convert eball.jpg eball.ppm");
		sprintf(ts, "convert %s %s", fname, ppmname);
		system(ts);
		//sprintf(ts, "%s", name);
		FILE *fpi = fopen(ppmname, "r");
		if (fpi) {
			char line[200];
			fgets(line, 200, fpi);
			fgets(line, 200, fpi);
			while (line[0] == '#')
				fgets(line, 200, fpi);
			sscanf(line, "%i %i", &width, &height);
			fgets(line, 200, fpi);
			//get pixel data
			int n = width * height * 3;			
			data = new unsigned char[n];			
			for (int i=0; i<n; i++)
				data[i] = fgetc(fpi);
			fclose(fpi);
		} else {
			printf("ERROR opening image: %s\n",ppmname);
			exit(0);
		}
		unlink(ppmname);
	}
};
Image img[3] = {
    "./images/Kang-Walk.gif",
    "./images/dog.jpg",
    "./images/bunny.png",
};

//-----------------------------------------------------------------------------
//Setup timers
class Timers {
public:
	double physicsRate;
	double oobillion;
	struct timespec timeStart, timeEnd, timeCurrent;
	struct timespec walkTime;
	Timers() {
		physicsRate = 1.0 / 30.0;
		oobillion = 1.0 / 1e9;
	}
	double timeDiff(struct timespec *start, struct timespec *end) {
		return (double)(end->tv_sec - start->tv_sec ) +
				(double)(end->tv_nsec - start->tv_nsec) * oobillion;
	}
	void timeCopy(struct timespec *dest, struct timespec *source) {
		memcpy(dest, source, sizeof(struct timespec));
	}
	void recordTime(struct timespec *t) {
		clock_gettime(CLOCK_REALTIME, t);
	}
} timers;
//-----------------------------------------------------------------------------


class Global {
public:
	int done;
	int xres, yres;
	int walk;
	bool credits = false;
	int walkFrame;
	double delay;
	//Booleans
	bool inMainMenu = true;
	bool inGame = false;
	GLuint walkTexture;
	GLuint sergioTexture;
	GLuint guadalupeTexture;
	Vec box[20];
	Global() {
		done=0;
		xres=800;
		yres=600;
		walk=0;
		walkFrame=0;
		delay = 0.1;
		for (int i=0; i<20; i++) {
			box[i][0] = rnd() * xres;
			box[i][1] = rnd() * (yres-220) + 220.0;
			box[i][2] = 0.0;
		}
	}
} g;

int selected = 1;

class Player {
    public:
	Vec pos;
	Vec vel;
	Vec lastpos;
}player;

class X11_wrapper {
private:
	Display *dpy;
	Window win;
public:
	X11_wrapper() {
		GLint att[] = { GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None };
		//GLint att[] = { GLX_RGBA, GLX_DEPTH_SIZE, 24, None };
		XSetWindowAttributes swa;
		setupScreenRes(g.xres, g.yres);
		dpy = XOpenDisplay(NULL);
		if (dpy == NULL) {
			printf("\n\tcannot connect to X server\n\n");
			exit(EXIT_FAILURE);
		}
		Window root = DefaultRootWindow(dpy);
		XVisualInfo *vi = glXChooseVisual(dpy, 0, att);
		if (vi == NULL) {
			printf("\n\tno appropriate visual found\n\n");
			exit(EXIT_FAILURE);
		} 
		Colormap cmap = XCreateColormap(dpy, root, vi->visual, AllocNone);
		swa.colormap = cmap;
		swa.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask |
							StructureNotifyMask | SubstructureNotifyMask;
		win = XCreateWindow(dpy, root, 0, 0, g.xres, g.yres, 0,
								vi->depth, InputOutput, vi->visual,
								CWColormap | CWEventMask, &swa);
		GLXContext glc = glXCreateContext(dpy, vi, NULL, GL_TRUE);
		glXMakeCurrent(dpy, win, glc);
		setTitle();
	}
	~X11_wrapper() {
		XDestroyWindow(dpy, win);
		XCloseDisplay(dpy);
	}
	void setTitle() {
		//Set the window title bar.
		XMapWindow(dpy, win);
		XStoreName(dpy, win, "Walk Cycle");
	}
	void setupScreenRes(const int w, const int h) {
		g.xres = w;
		g.yres = h;
	}
	void reshapeWindow(int width, int height) {
		//window has been resized.
		setupScreenRes(width, height);
		glViewport(0, 0, (GLint)width, (GLint)height);
		glMatrixMode(GL_PROJECTION); glLoadIdentity();
		glMatrixMode(GL_MODELVIEW); glLoadIdentity();
		glOrtho(0, g.xres, 0, g.yres, -1, 1);
		setTitle();
	}
	void checkResize(XEvent *e) {
		//The ConfigureNotify is sent by the
		//server if the window is resized.
		if (e->type != ConfigureNotify)
			return;
		XConfigureEvent xce = e->xconfigure;
		if (xce.width != g.xres || xce.height != g.yres) {
			//Window size did change.
			reshapeWindow(xce.width, xce.height);
		}
	}
	bool getXPending() {
		return XPending(dpy);
	}
	XEvent getXNextEvent() {
		XEvent e;
		XNextEvent(dpy, &e);
		return e;
	}
	void swapBuffers() {
		glXSwapBuffers(dpy, win);
	}

} x11;

//external function prototypes
extern void showMainMenu(int,int);

//function prototypes
void initOpengl(void);
void checkMouse(XEvent *e);
int checkKeys(XEvent *e);
void init();
void physics(void);
void render(void);
void show_credits();


int main(void)
{
	initOpengl();
	init();
	int done = 0;
	while (!done) {
		while (x11.getXPending()) {
			XEvent e = x11.getXNextEvent();
			x11.checkResize(&e);
			checkMouse(&e);
			done = checkKeys(&e);
		}
		physics();
		//render();
		showMainMenu(g.xres,g.yres);
		x11.swapBuffers();
	}
	cleanup_fonts();
	return 0;
}

void show_credits()
{
    Rect r;
    unsigned int c = 0x00ff3333;
    r.bot = g.yres - 20;
    r.left = g.xres/2 - 30;
    r.center = 0;
    ggprint8b(&r, 20, c, "CREDITS");


}
unsigned char *buildAlphaData(Image *img)
{
	//add 4th component to RGB stream...
	int i;
	unsigned char *newdata, *ptr;
	unsigned char *data = (unsigned char *)img->data;
	newdata = (unsigned char *)malloc(img->width * img->height * 4);
	ptr = newdata;
	unsigned char a,b,c;
	//use the first pixel in the image as the transparent color.
	unsigned char t0 = *(data+0);
	unsigned char t1 = *(data+1);
	unsigned char t2 = *(data+2);
	for (i=0; i<img->width * img->height * 3; i+=3) {
		a = *(data+0);
		b = *(data+1);
		c = *(data+2);
		*(ptr+0) = a;
		*(ptr+1) = b;
		*(ptr+2) = c;
		*(ptr+3) = 1;
		if (a==t0 && b==t1 && c==t2)
			*(ptr+3) = 0;
		//-----------------------------------------------
		ptr += 4;
		data += 3;
	}
	return newdata;
}

void initOpengl(void)
{
	//OpenGL initialization
	int w = img[0].width;
	int h = img[0].height;
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, 3, w, h, 0,
		GL_RGB, GL_UNSIGNED_BYTE, img[0].data);



	glViewport(0, 0, g.xres, g.yres);
	//Initialize matrices
	glMatrixMode(GL_PROJECTION); glLoadIdentity();
	glMatrixMode(GL_MODELVIEW); glLoadIdentity();
	//This sets 2D mode (no perspective)
	glOrtho(0, g.xres, 0, g.yres, -1, 1);
	//
	glDisable(GL_LIGHTING);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_FOG);
	glDisable(GL_CULL_FACE);
	//
	//Clear the screen
	glClearColor(1.0, 1.0, 1.0, 1.0);
	//glClear(GL_COLOR_BUFFER_BIT);
	//Do this to allow fonts
	glEnable(GL_TEXTURE_2D);
	initialize_fonts();
	//
	//load the images file into a ppm structure.
	//
	int w2 = img[1].width;
	int h2 = img[1].height;
	int w3 = img[2].width;
	int h3 = img[2].height;
	//
	//create opengl texture elements
	glGenTextures(1, &g.walkTexture);
	glGenTextures(1, &g.sergioTexture);
	glGenTextures(1, &g.guadalupeTexture);


	//-------------------------------------------------------------------------
	//silhouette
	//this is similar to a sprite graphic
	//
	glBindTexture(GL_TEXTURE_2D, g.walkTexture);
	//
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
	//
	//must build a new set of data...
	unsigned char *walkData = buildAlphaData(&img[0]);	
	printf("h = %d \n w = %d",h,w);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0,
		GL_RGBA, GL_UNSIGNED_BYTE, walkData);
	//free(walkData);
	//unlink("./images/walk.ppm");
	//-------------------------------------------------------------------------
	//Dog picture
	glBindTexture(GL_TEXTURE_2D, g.sergioTexture);
	
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, 3, w2, h2, 0,
		GL_RGB, GL_UNSIGNED_BYTE, img[1].data);
	unsigned char *sergioData = buildAlphaData(&img[1]);	
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w2, h2, 0,
		GL_RGBA, GL_UNSIGNED_BYTE, sergioData);
	//-------------------------------------------------------------------------
	//Bunny picture
	glBindTexture(GL_TEXTURE_2D, g.guadalupeTexture);
	
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, 3, w3, h3, 0,
		GL_RGB, GL_UNSIGNED_BYTE, img[2].data);
	unsigned char *guadalupeData = buildAlphaData(&img[2]);	
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w3, h3, 0,
		GL_RGBA, GL_UNSIGNED_BYTE, guadalupeData);
}

void init() {
    player.pos[0] = 100.0;
    player.pos[1] = (double)(g.yres-200);
    MakeVector(6.0,0.0,0.0, player.vel);
    VecCopy(player.pos, player.lastpos);
    
}

void checkMouse(XEvent *e)
{
	//Did the mouse move?
	//Was a mouse button clicked?
	static int savex = 0;
	static int savey = 0;
	//
	if (e->type == ButtonRelease) {
		return;
	}
	if (e->type == ButtonPress) {
		if (e->xbutton.button==1) {
			//Left button is down
		}
		if (e->xbutton.button==3) {
			//Right button is down
		}
	}
	if (savex != e->xbutton.x || savey != e->xbutton.y) {
		//Mouse moved
		savex = e->xbutton.x;
		savey = e->xbutton.y;
	}
}

int checkKeys(XEvent *e)
{
	//keyboard input?
	static int shift=0;
	if (e->type != KeyRelease && e->type != KeyPress)
		return 0;
	int key = (XLookupKeysym(&e->xkey, 0) & 0x0000ffff);
	if (e->type == KeyRelease) {
		if (key == XK_Shift_L || key == XK_Shift_R)
			shift = 0;
		return 0;
	}
	if (key == XK_Shift_L || key == XK_Shift_R) {
		shift=1;
		return 0;
	}
	(void)shift;

	if (g.inMainMenu) {
		switch (key) {
			case XK_Up:
				selected++;
				break;
			case XK_Down:
				selected--;
				break;
		}
	}
	if (g.inGame) {	
		switch (key) {
			case XK_w:
				timers.recordTime(&timers.walkTime);
				g.walk ^= 1;
			break;
		case XK_c:
			if (g.credits == false)
			    g.credits = true;
			else
			    g.credits = false;
			break;
		case XK_Left:
			VecCopy(player.pos, player.lastpos);
			player.pos[0] -= 10.0;
			g.walk^=1;
			break;
		case XK_Right:
			VecCopy(player.pos, player.lastpos);
			player.pos[0] += 10.0;
			g.walk^=1;
			break;
		case XK_Up:
			break;
		case XK_Down:
			break;
		case XK_equal:
			g.delay -= 0.005;
			if (g.delay < 0.005)
				g.delay = 0.005;
			break;
		case XK_minus:
			g.delay += 0.005;
			break;
		case XK_Escape:
			return 1;
			break;
	}
	}
	return 0;
}

Flt VecNormalize(Vec vec)
{
	Flt len, tlen;
	Flt xlen = vec[0];
	Flt ylen = vec[1];
	Flt zlen = vec[2];
	len = xlen*xlen + ylen*ylen + zlen*zlen;
	if (len == 0.0) {
		MakeVector(0.0,0.0,1.0,vec);
		return 1.0;
	}
	len = sqrt(len);
	tlen = 1.0 / len;
	vec[0] = xlen * tlen;
	vec[1] = ylen * tlen;
	vec[2] = zlen * tlen;
	return(len);
}

void physics(void)
{
	if (g.walk) {
		//man is walking...
		//when time is up, advance the frame.
		timers.recordTime(&timers.timeCurrent);
		double timeSpan = timers.timeDiff(&timers.walkTime, &timers.timeCurrent);
		if (timeSpan > g.delay) {
			//advance
			++g.walkFrame;
			if (g.walkFrame >= 16)
				g.walkFrame -= 16;
			timers.recordTime(&timers.walkTime);
		}
		for (int i=0; i<20; i++) {
			g.box[i][0] -= 2.0 * (0.05 / g.delay);
			if (g.box[i][0] < -10.0)
				g.box[i][0] += g.xres + 10.0;
		}
	}
}

void render(void)
{
	Rect r;
	//Clear the screen
	glClearColor(0.1, 0.1, 0.1, 1.0);
	glClear(GL_COLOR_BUFFER_BIT);
	float cx = g.xres/2.0;
	float cy = g.yres/2.0;
	//
	//show ground
	glBegin(GL_QUADS);
		glColor3f(0.2, 0.2, 0.2);
		glVertex2i(0,       220);
		glVertex2i(g.xres, 220);
		glColor3f(0.4, 0.4, 0.4);
		glVertex2i(g.xres,   0);
		glVertex2i(0,         0);
	glEnd();
	//
	//fake shadow
	//glColor3f(0.25, 0.25, 0.25);
	//glBegin(GL_QUADS);
	//	glVertex2i(cx-60, 150);
	//	glVertex2i(cx+50, 150);
	//	glVertex2i(cx+50, 130);
	//	glVertex2i(cx-60, 130);
	//glEnd();
	//
	//show boxes as background
	for (int i=0; i<20; i++) {
		glPushMatrix();
		glTranslated(g.box[i][0],g.box[i][1],g.box[i][2]);
		glColor3f(0.2, 0.2, 0.2);
		glBegin(GL_QUADS);
			glVertex2i( 0,  0);
			glVertex2i( 0, 30);
			glVertex2i(20, 30);
			glVertex2i(20,  0);
		glEnd();
		glPopMatrix();
	}
	float h = 200.0;
	float w = h * 0.5;
	glPushMatrix();
	glTranslatef(player.pos[0],0,player.pos[2]);
	glColor3f(1.0, 1.0, 1.0);
	glBindTexture(GL_TEXTURE_2D, g.walkTexture);
	//
	glEnable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GREATER, 0.0f);
	glColor4ub(255,255,255,255);
	int ix = g.walkFrame % 7;
	int iy = 0;
	if (g.walkFrame >= 7)
		iy = 1;
	float tx = (float)ix / 7.0;
	float ty = (float)iy / 1.0;
	glBegin(GL_QUADS);
		glTexCoord2f(tx,      ty+1); glVertex2i(cx-w, cy-h);
		glTexCoord2f(tx,      ty);    glVertex2i(cx-w, cy+h);
		glTexCoord2f(tx+.1, ty);    glVertex2i(cx+w, cy+h);
		glTexCoord2f(tx+.1, ty+1); glVertex2i(cx+w, cy-h);
	glEnd();
	glPopMatrix();
	glBindTexture(GL_TEXTURE_2D, 0);
	glDisable(GL_ALPHA_TEST);
	//
	unsigned int c = 0x00ffff44;

	//Show Credits
	if (g.credits) {
	    int x = g.xres/2 - 30;
	    int y = g.yres - 20;
	    extern void show_credits_Alberto(int x,int y);
	    extern void show_credits_Sergio(int x,int y);
	    extern void showLupeName(int x, int y);
	    extern void showSergioPicture(int x, int y, GLuint texid);
	    extern void showAlbertoPicture(int x, int y, float tx, float ty, GLuint texid);
	    extern void showLupePicture(int x, int y, GLuint texid);

	    showSergioPicture (x-100, y-40, g.sergioTexture);
	    showAlbertoPicture(x+200,  y-40,  tx, ty, g.walkTexture);
	    showLupePicture(x-100, y-150, g.guadalupeTexture);
	    show_credits();
	    show_credits_Alberto(x,y-20);
	    show_credits_Sergio(x,y-40);
	    showLupeName(x,y-60);
	    

	  //show dog pic
	 /* 
	  int wid=40;
	  glPushMatrix();
	  glTranslatef(200, 200, 0);
	  glBindTexture(GL_TEXTURE_2D, g.sergioTexture);
	  glBegin(GL_QUADS);
	  	glTexCoord2f(0.0f, 1.0f); glVertex2i(-wid, -wid);
		glTexCoord2f(0.0f, 0.0f); glVertex2i(-wid, wid);
		glTexCoord2f(1.0f, 0.0f); glVertex2i(wid, wid);
		glTexCoord2f(1.0f, 1.0f); glVertex2i(wid, -wid);
	  glEnd();
	  glPopMatrix();
	    return;
	    */
	}
	r.bot = g.yres - 20;
	r.left = 10;
	r.center = 0;
	ggprint8b(&r, 16, c, "C   Credits");
	ggprint8b(&r, 16, c, "W   Walk cycle");
	ggprint8b(&r, 16, c, "+   faster");
	ggprint8b(&r, 16, c, "-   slower");
	ggprint8b(&r, 16, c, "right arrow -> walk right");
	ggprint8b(&r, 16, c, "left arrow  <- walk left");
	ggprint8b(&r, 16, c, "frame: %i", g.walkFrame);
}







