// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
// Small example how to use the library.
// For more examples, look at demo-main.cc
//
// This code is public domain
// (but note, that the led-matrix library this depends on is GPL v2)

#include "led-matrix.h"
#include "graphics.h"
#include "threaded-canvas-manipulator.h"

#include <unistd.h>
#include <math.h>
#include <stdio.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <sys/time.h>

#define MATRIX_ROWS   				32 // A 32x32 display. Use 16 when this is a 16x32 display.
#define MATRIX_CHAIN  				4  // Number of boards chained together.
#define MATRIX_MAIN_FONT_FILE 		"fonts/10x20.bdf"
#define MATRIX_TEMPHUM_FONT_FILE 	"fonts/6x10.bdf"
#define MATRIX_DATE_FONT_FILE 		"fonts/5x8.bdf"

using namespace rgb_matrix;

using rgb_matrix::GPIO;
using rgb_matrix::RGBMatrix;
using rgb_matrix::Canvas;
using rgb_matrix::ThreadedCanvasManipulator;

volatile bool interrupt_received = false;
static void InterruptHandler(int signo) {  
  interrupt_received = true;
}

typedef struct												// Create A Structure
{
	char	*imageData;										// Image Data (Up To 32 Bits)
	int		bpp;											// Image Color Depth In Bits Per Pixel.
	int		width;											// Image Width
	int		height;											// Image Height	
} TextureImage;												// Structure Name

rgb_matrix::Font main_font;
rgb_matrix::Font temphum_font;
rgb_matrix::Font date_font;

TextureImage imgYoutube;

void getClockTime()
{
	static int seconds_last = 99;
	char TimeString[128];

	timeval curTime;
	gettimeofday(&curTime, NULL);
	if (seconds_last == curTime.tv_sec)
		return;
	
	seconds_last = curTime.tv_sec;
	
	strftime(TimeString, 80, "%Y-%m-%d %H:%M:%S", localtime(&curTime.tv_sec));	
}

bool LoadTGA(TextureImage *texture, const char *filename)			// Loads A TGA File Into Memory
{    
	char	TGAheader[12]={0,0,2,0,0,0,0,0,0,0,0,0};	// Uncompressed TGA Header
	char	TGAcompare[12];								// Used To Compare TGA Header
	char	header[6];									// First 6 Useful Bytes From The Header
	int		bytesPerPixel;								// Holds Number Of Bytes Per Pixel Used In The TGA File
	int		imageSize;									// Used To Store The Image Size When Setting Aside Ram
	int		temp;										// Temporary Variable	
	char 	text[FILENAME_MAX];	

	printf("load %s ...", filename);

	FILE *file = fopen(filename, "rb");						// Open The TGA File


	if(	file==NULL ||										// Does File Even Exist?
		fread(TGAcompare,1,sizeof(TGAcompare),file)!=sizeof(TGAcompare) ||	// Are There 12 Bytes To Read?
		memcmp(TGAheader,TGAcompare,sizeof(TGAheader))!=0				||	// Does The Header Match What We Want?
		fread(header,1,sizeof(header),file)!=sizeof(header))				// If So Read Next 6 Header Bytes
	{
		if (file == NULL)									// Did The File Even Exist? *Added Jim Strong*
		{
			printf(text, "Error loading %s\nUnable to open (NULL)", filename);			
			return false;									// Return False
		}
		else
		{
			fclose(file);									// If Anything Failed, Close The File
			printf(text, "Error loading %s\nBad header or not tga file", filename);			
			return false;									// Return False
		}
	}

	texture->width  = header[1] * 256 + header[0];			// Determine The TGA Width	(highbyte*256+lowbyte)
	texture->height = header[3] * 256 + header[2];			// Determine The TGA Height	(highbyte*256+lowbyte)

	fprintf(stderr, "TGA: %dx%d\n", texture->width, texture->height);	
    
 	if(	texture->width	<=0	||								// Is The Width Less Than Or Equal To Zero
		texture->height	<=0	||								// Is The Height Less Than Or Equal To Zero
		(header[4]!=24 && header[4]!=32))					// Is The TGA 24 or 32 Bit?
	{
		printf(text, "Error loading %s\nFile must be 24 bpp (is 32 bpp)", filename);		
		fclose(file);										// If Anything Failed, Close The File
		return false;										// Return False
	}

	texture->bpp	= header[4];							// Grab The TGA's Bits Per Pixel (24 or 32)
	bytesPerPixel	= texture->bpp/8;						// Divide By 8 To Get The Bytes Per Pixel
	imageSize		= texture->width*texture->height*bytesPerPixel;	// Calculate The Memory Required For The TGA Data

	texture->imageData=(char *)malloc(imageSize);		// Reserve Memory To Hold The TGA Data

	if(	texture->imageData==NULL ||							// Does The Storage Memory Exist?
		fread(texture->imageData, 1, imageSize, file)!=imageSize)	// Does The Image Size Match The Memory Reserved?
	{
		if(texture->imageData!=NULL)						// Was Image Data Loaded
			free(texture->imageData);						// If So, Release The Image Data

		printf(text, "Error loading %s\nNo memory avaliable or data read error", filename);		

		fclose(file);										// Close The File
		return false;										// Return False
	}

	for(int i=0; i<int(imageSize); i+=bytesPerPixel)		// Loop Through The Image Data
	{														// Swaps The 1st And 3rd Bytes ('R'ed and 'B'lue)
		temp=texture->imageData[i];							// Temporarily Store The Value At Image Data 'i'
		texture->imageData[i] = texture->imageData[i + 2];	// Set The 1st Byte To The Value Of The 3rd Byte
		texture->imageData[i + 2] = temp;					// Set The 3rd Byte To The Value In 'temp' (1st Byte Value)
	}

	fclose (file);											// Close The File
	return true;											// Texture Building Went Ok, Return True
}

void drawPicture(Canvas *canvas, TextureImage *texture, int x, int y)
{	
	int cx;
	int cy;
	long c=0;

	for (cy=texture->height ; cy>0 ; cy--) // Y is reversed in TGA
	{
		for (cx=0 ; cx<texture->width ; cx++)
		{		
							
			float r = texture->imageData[c];
			float g = texture->imageData[c+1];
			float b = texture->imageData[c+2];
			float a = texture->imageData[c+3] * 254;
			
			r = (r * a) + (0 * (1.0 - a));
			g = (g * a) + (0 * (1.0 - a));
			b = (b * a) + (0 * (1.0 - a));
			

			canvas->SetPixel(cx, cy, r, g, b);
			c+=4;
		}
	}	
}

class piClockUpdater : public ThreadedCanvasManipulator {
public:
  piClockUpdater(Canvas *canvas) : ThreadedCanvasManipulator(canvas) { }
  virtual void Run() {
    unsigned char c;
    rgb_matrix::Color fcolor(255, 255, 255);
    rgb_matrix::Color fbg_color(0, 0, 0);        
     
    printf("Canvas is %dx%d\n", canvas()->width(), canvas()->height());
    while (running()) {

		const int width = canvas()->width() - 1;
		const int height = canvas()->height() - 1;	      

		// Main system time
		//DrawText(canvas(), main_font, 1, 1 + main_font.baseline(), fcolor, &fbg_color, getTimeString());

		// Temperarure and humidity
		DrawText(canvas(), temphum_font, 85, 1 + temphum_font.baseline(), Color(0, 0, 254), &fbg_color, "23C");    
		DrawText(canvas(), temphum_font, 87+20, 1 + temphum_font.baseline(), Color(0, 254, 0), &fbg_color, "57%");        

		DrawText(canvas(), date_font, 85, 17, Color(195, 30, 30), &fbg_color, getDateString());        	
		/*
		DrawLine(canvas(), 0, 0,      width, 0,      Color(255, 0, 0));
		DrawLine(canvas(), 0, height, width, height, Color(255, 255, 0));
		DrawLine(canvas(), 0, 0,      0,     height, Color(0, 0, 255));
		DrawLine(canvas(), width, 0,  width, height, Color(0, 255, 0));      
		*/

		drawPicture(canvas(), &imgYoutube, 1, 1);

		usleep(15 * 1000);
    }
  }

  virtual char *getTimeString()
  {

    char strTime[20];
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);    

    sprintf(strTime,"%02d:%02d:%02d",tm.tm_hour, tm.tm_min, tm.tm_sec);

  	return strTime;
  }

  virtual char *getDateString()
  {

    char strDate[50];    

    time_t t = time(NULL);
    struct tm tm = *localtime(&t);    
    
    strftime (strDate,50,"%d-%m-%g", &tm);

  	return strDate;
  }  

private:  
  FrameCanvas *off_screen_canvas_;

};

int main(int argc, char *argv[]) {

  /*
   * Set up GPIO pins. This fails when not running as root.
   */

  fprintf(stderr, "piClock Init\n");

  if (!main_font.LoadFont(MATRIX_MAIN_FONT_FILE)) {
    fprintf(stderr, "ERROR: Couldn't load font '%s'\n", MATRIX_MAIN_FONT_FILE);
    return 1;
  }    

  if (!temphum_font.LoadFont(MATRIX_TEMPHUM_FONT_FILE)) {
    fprintf(stderr, "ERROR: Couldn't load font '%s'\n", MATRIX_TEMPHUM_FONT_FILE);
    return 1;
  }      

  if (!date_font.LoadFont(MATRIX_DATE_FONT_FILE)) {
    fprintf(stderr, "ERROR: Couldn't load font '%s'\n", MATRIX_DATE_FONT_FILE);
    return 1;
  }  
  
  if (!LoadTGA(&imgYoutube, "img/youtubelogo.tga"))        
  {
  	fprintf(stderr, "Error loading youtubelogo.tga");
  	return 1;
  }

  GPIO io;
  if (!io.Init())
  {
    printf("Can't init GPIO. Please use this as root!\n");
    return 1;
  }  


  Canvas *canvas = new RGBMatrix(&io, MATRIX_ROWS, MATRIX_CHAIN);

  if (canvas == NULL)
    return 1;

  // It is always good to set up a signal handler to cleanly exit when we
  // receive a CTRL-C for instance. The DrawOnCanvas() routine is looking
  // for that.
  signal(SIGTERM, InterruptHandler);
  signal(SIGINT, InterruptHandler);

  //RGBMatrix matrix(&io);
  piClockUpdater *piClock = new piClockUpdater(canvas);
  piClock->Start();   // Start doing things.
  // This now runs in the background, you can do other things here,
  // e.g. aquiring new data or simply wait. But for waiting, you wouldn't
  // need a thread in the first place.  

  // Main stuff loop!
    rgb_matrix::Color fcolor(255, 255, 0);
    rgb_matrix::Color fbg_color(0, 0, 0);          
  while (!interrupt_received) {  
  	//rgb_matrix::DrawText(canvas, main_font, 1, 1 + main_font.baseline(), fcolor, &fbg_color, "Hola mundo!");
    usleep(1 * 100000);
  }

  piClock->Stop();  

  delete piClock;

  // Animation finished. Shut down the RGB matrix.
  canvas->Clear();
  delete canvas;

  return 0;
}
