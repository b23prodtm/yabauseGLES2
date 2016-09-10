/*  Copyright 2006 Guillaume Duhamel
    Copyright 2006 Fabien Coulon
    Copyright 2005 Joost Peters

    This file is part of Yabause.

    Yabause is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Yabause is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Yabause; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <SDL2/SDL.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "../yabause.h"
#include "../gameinfo.h"
#include "../yui.h"
#include "../peripheral.h"
#include "../sh2core.h"
#include "../sh2int.h"
#include "../vidogl.h"
#include "../vidsoftnogl.h"
#include "../cs0.h"
#include "../cs2.h"
#include "../cdbase.h"
#include "../scsp.h"
#include "../sndsdl.h"
#include "../sndal.h"
#include "../persdljoy.h"
#ifdef ARCH_IS_LINUX
#include "../perlinuxjoy.h"
#endif
#include "../debug.h"
#include "../m68kcore.h"
#include "../m68kc68k.h"
#include "../vdp1.h"
#include "../vdp2.h"
#include "../cdbase.h"
#include "../peripheral.h"

M68K_struct * M68KCoreList[] = {
&M68KDummy,
#ifdef HAVE_MUSASHI
&M68KMusashi,
#endif
#ifdef HAVE_C68K
&M68KC68K,
#endif
#ifdef HAVE_Q68
&M68KQ68,
#endif
NULL
};

SH2Interface_struct *SH2CoreList[] = {
&SH2Interpreter,
&SH2DebugInterpreter,
#ifdef TEST_PSP_SH2
&SH2PSP,
#endif
#ifdef SH2_DYNAREC
&SH2Dynarec,
#endif
NULL
};

PerInterface_struct *PERCoreList[] = {
&PERDummy,
#ifdef HAVE_LIBSDL
&PERSDLJoy,
#endif
#ifdef ARCH_IS_LINUX
&PERLinuxJoy,
#endif
NULL
};

CDInterface *CDCoreList[] = {
&DummyCD,
&ISOCD,
#ifndef UNKNOWN_ARCH
&ArchCD,
#endif
NULL
};

SoundInterface_struct *SNDCoreList[] = {
&SNDDummy,
#ifdef HAVE_LIBSDL
&SNDSDL,
#endif
#ifdef HAVE_LIBAL
&SNDAL,
#endif
NULL
};

VideoInterface_struct *VIDCoreList[] = {
&VIDDummy,
&VIDOGLES,
&VIDSoftNoGL,
NULL
};

GLuint g_FrameBuffer = 0;
GLuint g_VertexBuffer = 0;
GLuint programObject  = 0;
GLuint positionLoc    = 0;
GLuint texCoordLoc    = 0;
GLuint samplerLoc     = 0;

int g_buf_width = -1;
int g_buf_height = -1;

static int resizeFilter = GL_NEAREST;

static char biospath[256] = "\0";
static char cdpath[256] = "\0";

SDL_Window *window;

yabauseinit_struct yinit;

int error;

float vertices [] = {
   -1.0f, 1.0f, 0, 0,
   1.0f, 1.0f, 1.0f, 0,
   1.0f, -1.0f, 1.0f, 1.0f,
   -1.0f,-1.0f, 0, 1.0f
};

void YuiErrorMsg(const char * string) {
	fprintf(stderr, "%s\n\r", string);
}

void YuiDrawSoftwareBuffer() {

    int buf_width, buf_height;
    int screen_width, screen_height;
    int error;

    SDL_GetWindowSize(window, &screen_width, &screen_height);

glUseProgram(programObject);
glViewport(0, 0, screen_width, screen_height);

    glClearColor( 0.0f,0.0f,0.0f,1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    if( g_FrameBuffer == 0 )
    {
       glGenTextures(1,&g_FrameBuffer);
       glActiveTexture ( GL_TEXTURE0 );
       glBindTexture(GL_TEXTURE_2D, g_FrameBuffer);
       glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1024, 1024, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
       glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
       glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
       glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, resizeFilter );
       glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, resizeFilter );
       error = glGetError();
       if( error != GL_NO_ERROR )
       {
          fprintf(stderr,"g_FrameBuffer gl error %04X", error );
          return;
       }
    }else{
       glBindTexture(GL_TEXTURE_2D, g_FrameBuffer);
    }

    VIDCore->GetGlSize(&buf_width, &buf_height);
    glTexSubImage2D(GL_TEXTURE_2D, 0,0,0,buf_width,buf_height,GL_RGBA,GL_UNSIGNED_BYTE,dispbuffer);


    if( g_VertexBuffer == 0 )
    {
       glGenBuffers(1, &g_VertexBuffer);
       glBindBuffer(GL_ARRAY_BUFFER, g_VertexBuffer);
       glBufferData(GL_ARRAY_BUFFER, sizeof(vertices),vertices,GL_STATIC_DRAW);
       error = glGetError();
       if( error != GL_NO_ERROR )
       {
          fprintf(stderr,"g_VertexBuffer gl error %04X", error );
          return;
       }
    }else{
       glBindBuffer(GL_ARRAY_BUFFER, g_VertexBuffer);
    }

   if( buf_width != g_buf_width ||  buf_height != g_buf_height )
   {
      vertices[6]=vertices[10]=(float)buf_width/1024.f;
      vertices[11]=vertices[15]=(float)buf_height/1024.f;
      glBufferData(GL_ARRAY_BUFFER, sizeof(vertices),vertices,GL_STATIC_DRAW);
      glVertexAttribPointer ( positionLoc, 2, GL_FLOAT,  GL_FALSE, 4 * sizeof(GLfloat), 0 );
      glVertexAttribPointer ( texCoordLoc, 2, GL_FLOAT,  GL_FALSE, 4 * sizeof(GLfloat), (void*)(sizeof(float)*2) );
      glEnableVertexAttribArray ( positionLoc );
      glEnableVertexAttribArray ( texCoordLoc );
      g_buf_width  = buf_width;
      g_buf_height = buf_height;
      error = glGetError();
      if( error != GL_NO_ERROR )
      {
         fprintf(stderr, "gl error %d", error );
         return;
      }
   }else{
      glBindBuffer(GL_ARRAY_BUFFER, g_VertexBuffer);
   }

    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

void YuiSwapBuffers(void) {
   if( window == NULL ){
      return;
   }
   if( yinit.vidcoretype == VIDCORE_SOFT ){
       YuiDrawSoftwareBuffer();
   }
   SDL_GL_SwapWindow(window);
}

void YuiInit() {
	yinit.m68kcoretype = M68KCORE_MUSASHI;
	yinit.percoretype = PERCORE_SDLJOY;
	yinit.sh2coretype = SH2CORE_DEFAULT;
	yinit.vidcoretype = VIDCORE_SOFT; //VIDCORE_OGL
	yinit.sndcoretype = SNDCORE_SDL;
	yinit.cdcoretype = CDCORE_DEFAULT;
	yinit.carttype = CART_NONE;
	yinit.regionid = REGION_EUROPE;
	yinit.biospath = NULL;
	yinit.cdpath = NULL;
	yinit.buppath = NULL;
	yinit.mpegpath = NULL;
	yinit.cartpath = NULL;
        yinit.videoformattype = VIDEOFORMATTYPE_NTSC;
	yinit.osdcoretype = OSDCORE_DEFAULT;
	yinit.skip_load = 0;

	yinit.sh1coretype = SH2CORE_DEFAULT;
	yinit.use_cd_block_lle = 0;
	yinit.usethreads = 1;
	yinit.numthreads = 4;
}

void SDLInit(void) {
	SDL_GLContext context;
	SDL_Renderer* rdr;

	SDL_InitSubSystem(SDL_INIT_VIDEO);

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

	SDL_GL_SetSwapInterval(1);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);

	window = SDL_CreateWindow("OpenGL Window", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, SDL_WINDOW_OPENGL);
	if (!window) {
    		fprintf(stderr, "Couldn't create window: %s\n", SDL_GetError());
    		return;
	}

	context = SDL_GL_CreateContext(window);
	if (!context) {
    		fprintf(stderr, "Couldn't create context: %s\n", SDL_GetError());
    		return;
	}

	rdr = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
}

///
// Create a shader object, load the shader source, and
// compile the shader.
//
GLuint LoadShader ( GLenum type, const char *shaderSrc )
{
   GLuint shader;
   GLint compiled;
   
   // Create the shader object
   shader = glCreateShader ( type );

   if ( shader == 0 )
   	return 0;

   // Load the shader source
   glShaderSource ( shader, 1, &shaderSrc, NULL );
   
   // Compile the shader
   glCompileShader ( shader );

   // Check the compile status
   glGetShaderiv ( shader, GL_COMPILE_STATUS, &compiled );

   if ( !compiled ) 
   {
      GLint infoLen = 0;

      glGetShaderiv ( shader, GL_INFO_LOG_LENGTH, &infoLen );
      
      if ( infoLen > 1 )
      {
         char* infoLog = malloc (sizeof(char) * infoLen );

         glGetShaderInfoLog ( shader, infoLen, NULL, infoLog );
         printf ( "Error compiling shader:\n%s\n", infoLog );            
         
         free ( infoLog );
      }

      glDeleteShader ( shader );
      return 0;
   }

   return shader;

}

int YuiInitProgramForSoftwareRendering()
{
   GLbyte vShaderStr[] =
      "attribute vec4 a_position;   \n"
      "attribute vec2 a_texCoord;   \n"
      "varying vec2 v_texCoord;     \n"
      "void main()                  \n"
      "{                            \n"
      "   gl_Position = a_position; \n"
      "   v_texCoord = a_texCoord;  \n"
      "}                            \n";

   GLbyte fShaderStr[] =
      "varying vec2 v_texCoord;                            \n"
      "uniform sampler2D s_texture;                        \n"
      "void main()                                         \n"
      "{                                                   \n"
      "  gl_FragColor = texture2D( s_texture, v_texCoord );\n"    
      "}                                                   \n";

   GLuint vertexShader;
   GLuint fragmentShader;
   GLint linked;

   // Load the vertex/fragment shaders
   vertexShader = LoadShader ( GL_VERTEX_SHADER, vShaderStr );
   fragmentShader = LoadShader ( GL_FRAGMENT_SHADER, fShaderStr );

   // Create the program object
   programObject = glCreateProgram ( );

   if ( programObject == 0 )
      return 0;

   glAttachShader ( programObject, vertexShader );
   glAttachShader ( programObject, fragmentShader );

   // Link the program
   glLinkProgram ( programObject );

   // Check the link status
   glGetProgramiv ( programObject, GL_LINK_STATUS, &linked );

   if ( !linked )
   {
      GLint infoLen = 0;

      glGetProgramiv ( programObject, GL_INFO_LOG_LENGTH, &infoLen );

      if ( infoLen > 1 )
      {
        char* infoLog = malloc (sizeof(char) * infoLen );
        glGetProgramInfoLog ( programObject, infoLen, NULL, infoLog );
        fprintf (stderr, "Error linking program:\n%s\n", infoLog );
        free ( infoLog );
         return GL_FALSE;
      }

      glDeleteProgram ( programObject );
      return GL_FALSE;
   }


   // Get the attribute locations
   positionLoc = glGetAttribLocation ( programObject, "a_position" );
   texCoordLoc = glGetAttribLocation ( programObject, "a_texCoord" );

   // Get the sampler location
   samplerLoc = glGetUniformLocation ( programObject, "s_texture" );

   glUseProgram(programObject);


   return GL_TRUE;
}


void displayGameInfo(char *filename) {
    GameInfo info;
    if (! GameInfoFromPath(filename, &info))
    {
       return;
    }

    printf("Game Info:\n\tSystem: %s\n\tCompany: %s\n\tItemNum:%s\n\tVersion:%s\n\tDate:%s\n\tCDInfo:%s\n\tRegion:%s\n\tPeripheral:%s\n\tGamename:%s\n", info.system, info.company, info.itemnum, info.version, info.date, info.cdinfo, info.region, info.peripheral, info.gamename);
}

int main(int argc, char *argv[]) {
	int i;
	SDL_Event event;
	int quit = SDL_FALSE;
	SDL_GameController* ctrl;

	LogStart();
	LogChangeOutput( DEBUG_STDERR, NULL );

	YuiInit();

//handle command line arguments
   for (i = 1; i < argc; ++i) {
      if (argv[i]) {
         //show usage
         if (0 == strcmp(argv[i], "-h") || 0 == strcmp(argv[i], "-?") || 0 == strcmp(argv[i], "--help")) {
            print_usage(argv[0]);
            return 0;
         }
			
         //set bios
         if (0 == strcmp(argv[i], "-b") && argv[i + 1]) {
            strncpy(biospath, argv[i + 1], 256);
            yinit.biospath = biospath;
	 } else if (strstr(argv[i], "--bios=")) {
            strncpy(biospath, argv[i] + strlen("--bios="), 256);
            yinit.biospath = biospath;
	 }
         //set iso
         else if (0 == strcmp(argv[i], "-i") && argv[i + 1]) {
            strncpy(cdpath, argv[i + 1], 256);
	    yinit.cdcoretype = 1;
	    yinit.cdpath = cdpath;
	    displayGameInfo(cdpath);
	 } else if (strstr(argv[i], "--iso=")) {
            strncpy(cdpath, argv[i] + strlen("--iso="), 256);
	    yinit.cdcoretype = 1;
	    yinit.cdpath = cdpath;
	 }
         //set cdrom
	 else if (0 == strcmp(argv[i], "-c") && argv[i + 1]) {
            strncpy(cdpath, argv[i + 1], 256);
	    yinit.cdcoretype = 2;
	    yinit.cdpath = cdpath;
	 } else if (strstr(argv[i], "--cdrom=")) {
            strncpy(cdpath, argv[i] + strlen("--cdrom="), 256);
	    yinit.cdcoretype = 2;
	    yinit.cdpath = cdpath;
	 }
         // Set sound
         else if (strcmp(argv[i], "-ns") == 0 || strcmp(argv[i], "--nosound") == 0) {
	    yinit.sndcoretype = 0;
	 }
         // Set sound
         else if (strcmp(argv[i], "-rb") == 0 || strcmp(argv[i], "--resizebilinear") == 0) {
	    resizeFilter = GL_LINEAR;
	 }
         // Auto frame skip
         else if (strstr(argv[i], "--autoframeskip=")) {
            int fscount;
            int fsenable;
            fscount = sscanf(argv[i] + strlen("--autoframeskip="), "%d", &fsenable);
            //if (fscount > 0)
              // yui_window_set_frameskip(YUI_WINDOW(yui), fsenable);
         }
	 // Binary
	 else if (strstr(argv[i], "--binary=")) {
	    char binname[1024];
	    unsigned int binaddress;
	    int bincount;

	    bincount = sscanf(argv[i] + strlen("--binary="), "%[^:]:%x", binname, &binaddress);
	    if (bincount > 0) {
	       if (bincount < 2) binaddress = 0x06004000;

               //yui_window_run(YUI_WINDOW(yui));
	       MappedMemoryLoadExec(binname, binaddress);
	    }
	 }
      }
   }
        SDLInit();

        if (yinit.vidcoretype == VIDCORE_SOFT) {
            if( YuiInitProgramForSoftwareRendering() != GL_TRUE ){
                fprintf(stderr, "Fail to YuiInitProgramForSoftwareRendering\n");
                return -1;
            }
        }

	YabauseDeInit();

        if (YabauseInit(&yinit) != 0) printf("YabauseInit error \n\r");


	while (quit == SDL_FALSE)
	{
		if (SDL_PollEvent(&event))
		{
   			switch(event.type)
    			{
     				case SDL_QUIT:
      					quit = SDL_TRUE;
      				break;
				case SDL_KEYDOWN:
					printf("Key down!");
				break;
     			}
  		}

	        PERCore->HandleEvents();
	}

	YabauseDeInit();
	LogStop();
	SDL_Quit();

	return 0;
}
