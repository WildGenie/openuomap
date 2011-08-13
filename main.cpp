/*  OpenUOMap - Free mapping program for Ultima Online
    Copyright (C) 2011 Raptor85

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


/*
    ---Please Note the following before continuing---

    I am NOT a windows developer, this is hacked together quickly as an
    example of how to bind to UOAssist, how to parse the UO map files,
    and how to request coordanate updates.  This was put together in hopes
    of someone more intersted in this than me using this code as an example
    and doing a re-write with a proper interface.  I guess you can build
    from this code if you want....but sir (or maam) .. good luck to you on that
    one.

    Basically for the love of god re-write this to use OpenGL for rendering, so
    you can smoothly scale/rotate/etc...I used SDL as an example because it's quick
    and incredibly easy to dump code into

*/
#include <windows.h>
#include <stdio.h>
#include <string>
#include "SDL.h"
#include "SDL/SDL_ttf.h"
#include "SDL/SDL_rotozoom.h"
#include <sys/stat.h>
#include <time.h>
#include <sys/types.h>

using namespace std;

/*
UOARequestData = WM_USER+200;
	UOACountResources = WM_USER+201;
	UOAGetCoords = WM_USER+202;
	UOAGetSkill = WM_USER+203;
	UOAGetStat = WM_USER+204;
	UOASetActiveMacro = WM_USER+205;
	UOAPlayMacro = WM_USER+206;
	UOADisplayText = WM_USER+207;
	UOARequestHouseBoatInformation = WM_USER+208;
	UOAAddCommandLineCommand = WM_USER+209;
	UOAGetUserID = WM_USER+210;
	UOAGetShardName = WM_USER+211;
	UOAAddUserToParty = WM_USER+212;
	UOAGetUOWindowHandle = WM_USER+213;
	UOAGetPoisonStatus = WM_USER+214;
	UOASetSkillLock = WM_USER+215;
	UOAGetAccountIndentifier = WM_USER+216;
	*/

//eew....you got windows code in my linux!

//kids...dont try this at home...we're going all in ghetto
// "global variable" style
HINSTANCE hinst;
HWND hwndMain;
HWND map_hWnd;

unsigned long currentfacet;

bool rotate;

Uint32 rmask, gmask, bmask, amask;





struct color
{
    char r;
    char g;
    char b;
};

color colors[49152];

struct facetinfo
{
    char name[32];
    int width;
    int height;
    SDL_Surface *map;
    SDL_Surface *rotatedmap;
};

facetinfo facets[64];

void SetFacets()
{
    sprintf(facets[0].name,"%s","Felluca");
    facets[0].width = 896*8;
    facets[0].height = 512*8;

    sprintf(facets[1].name,"%s","Trammel");
    facets[1].width = 896*8;
    facets[1].height = 512*8;

    sprintf(facets[2].name,"%s","Ilshenar");
    facets[2].width = 288*8;
    facets[2].height = 200*8;

    sprintf(facets[3].name,"%s","Malas");
    facets[3].width = 320*8;
    facets[3].height = 256*8;

    sprintf(facets[4].name,"%s","Tokuno");
    facets[4].width = 181*8;
    facets[4].height = 181*8;

    sprintf(facets[5].name,"%s","Ter Mur");
    facets[5].width = 160*8;
    facets[5].height = 512*8;


}

//origionally based off some example code somewhere, this program was built off of drawing code
//ripped out of a really old test app of mine, so performance was never really a thought
// (but you read my warning earlier and knew that right?)
void putpixel(SDL_Surface *surface, int x, int y, Uint32 pixel)
{
    int bpp = surface->format->BytesPerPixel;
    /* Here p is the address to the pixel we want to set */
    Uint8 *p = (Uint8 *)surface->pixels + y * surface->pitch + x * bpp;

    switch(bpp) {
    case 1:
        *p = pixel;
        break;

    case 2:
        *(Uint16 *)p = pixel;
        break;

    case 3:
        if(SDL_BYTEORDER == SDL_BIG_ENDIAN) {
            p[0] = (pixel >> 16) & 0xff;
            p[1] = (pixel >> 8) & 0xff;
            p[2] = pixel & 0xff;
        } else {
            p[0] = pixel & 0xff;
            p[1] = (pixel >> 8) & 0xff;
            p[2] = (pixel >> 16) & 0xff;
        }
        break;

    case 4:
        *(Uint32 *)p = pixel;
        break;
    }
}

void LoadColors()
{
    FILE *pFile;

    //note to the next developer...you...uh....probably want to make it read the game base path from
    //a config file here, as i doubt you all use my username....on 64 bit linux
    //there actually already is a config file I just hadn't bothered to put this string in it
    //or prompt user on first run for hte UO directory yet, but that's simple enough to do.

    pFile = fopen("/home/raptor85/.wine/drive_c/Program Files/EA GAMES/Ultima Online 2D Client/radarcol.mul","rb");
    if(pFile == NULL)
    {
        //error here...be glad i even bothered to catch this.....
        MessageBox(0, "Could not load radarcol.mul", "Error",MB_OK);

    }

    //there's a page on stratics somewhere that explains the UO file formats, they're pretty easy to read
    //if you have any REAL questions or have troubles, PM me on stratics, i have map making programs
    //in both C and PHP
    for(int i=0; i < 0xC000; i++)
    {
        unsigned short col;

        fread(&col,sizeof(unsigned short),1,pFile);

        colors[i].r = (0xFF/0x1F) *((col >> 10)& 0x1F);
        colors[i].g = (0xFF/0x1F) *((col >> 5)& 0x1F);
        colors[i].b = (0xFF/0x1F) *((col)& 0x1F);
    }

    fclose(pFile);
}

struct configuration
{
    unsigned short lastfacet;
    bool rotated;
    float zoomlevel;
    time_t mapsmodified[64];
};

configuration appconfig;

void LoadConfig()
{
    FILE *pFile;
    pFile = fopen("maps.cfg", "rb");
    if(pFile == NULL)
    {
        //make a new one and save it quickly
        appconfig.lastfacet = 0;
        appconfig.rotated = true;
        appconfig.zoomlevel = 1.0;
        for(int i=0;i<64;i++)
            appconfig.mapsmodified[i] = 0;

        currentfacet = appconfig.lastfacet;
        rotate = appconfig.rotated;
        return;
    }

    fread(&appconfig,sizeof(configuration),1,pFile);


    fclose(pFile);

    currentfacet = appconfig.lastfacet;
    rotate = appconfig.rotated;
}

void SaveConfig()
{
    FILE *pFile;
    pFile = fopen("maps.cfg", "wb");
    if(pFile == NULL)
        return;

    fwrite(&appconfig,sizeof(configuration),1,pFile);

    fclose(pFile);
}

void LoadMaps()
{
    char bytes[4800];
    for(int i=0; i<64;i++)
    {

        FILE *pFile;
        char strfile[512];
        time_t filemodified;
        int result;
        struct stat buf;

        sprintf(strfile,"/home/raptor85/.wine/drive_c/Program Files/EA GAMES/Ultima Online 2D Client/map%d.mul",i);

        //if you don't understand what's going on here, you probably can't improve it further.....

        //eh...i'll explain it anyways, it basicly just checks the last updated time on hte map files against the last time
        //the program generated new bitmap versions of the maps, if the programs version is older than the ones in the UO dir
        //it loads in the new .mul files and makes new maps...if not it loads the bitmap files...crashing miserably if they
        //don't exist because that means the user fucked with the data files and needs to learn a harsh lesson.

        result = stat(strfile, &buf);
        if( result != 0 )
        {
            return;
        }

        if(appconfig.mapsmodified[i]<buf.st_mtime)
        {
            appconfig.mapsmodified[i] = buf.st_mtime;
            //map file updated since last load, give user a prompt and reload the map
            char message[512];
            sprintf(message,"The map for %s is out of date, caching new map (this may take some time)",facets[i].name);
            MessageBox(0, message, "Informative",MB_OK);


            pFile = fopen(strfile, "rb");
            if(pFile == NULL)
                return;

            facets[i].map = SDL_CreateRGBSurface(SDL_SWSURFACE,facets[i].width,facets[i].height,32,rmask,gmask,bmask,amask);
            if(facets[i].map == NULL)
            {
                MessageBox(0, "Could not create maps!", "Error",MB_OK);
            }

            for(int x=0; x<facets[i].width/8; x++)
            {
                for(int y=0; y<facets[i].height/8; y++)
                {
                    fseek(pFile,196*((x*(facets[i].height/8))+y),SEEK_SET);
                    fread(bytes,sizeof(char),4,pFile); //unknown data ...seriously, i don't know WTF this block is for, nothing interesting though

                    for(int cy=0; cy<8; cy++)
                    {
                        for(int cx=0;cx<8;cx++)
                        {
                            unsigned short colorid;
                            fread(&colorid,sizeof(unsigned short),1,pFile); //colorid[1] now holds the color index

                            fread(bytes,sizeof(char),1,pFile);//altitude byte..tells you how high from the ground plane this tile is
                            //just incase you ever wanted to make a 3d topo map of UO :D (or properly order the statics you read in later, either way is fine)

                            SDL_LockSurface(facets[i].map);

                            Uint32 col; //because ambiguous 3 letter variable names are awesome....

                            col = SDL_MapRGB(facets[i].map->format,colors[colorid].r,colors[colorid].g,colors[colorid].b);

                            putpixel(facets[i].map,(x*8)+cx,(y*8)+cy,col);

                            SDL_UnlockSurface(facets[i].map);

                        }
                    }

                }
            }

            // lazy dev note: this only loads terrain data here, if you want statics
            // you need to load up the statics index file and the statics file
            // please try to do that yourself but if you get REALLY stuck
            // I'll send you some code on how to do that.

            facets[i].rotatedmap = rotozoomSurface(facets[i].map, -45, 1,1); //tilts it to the direction it shows in game

            // sprintf....the lazy man's way of generating file names. I honestly think the keys i hit the most while writing this
            //were ctrl+c and ctrl+v (I spent more tiem on the comments then code)
            char bmpfilename1[1024];
            sprintf(bmpfilename1,"map%d.bmp",i);
            SDL_SaveBMP(facets[i].map, bmpfilename1);
            sprintf(bmpfilename1,"map%d_rot.bmp",i);
            SDL_SaveBMP(facets[i].rotatedmap, bmpfilename1);

            //congrats, you just saved 2 enormous bitmap files on the hard drive...better than rotating at runtime though

            fclose(pFile);
        }
        else
        {
            //load the maps from the bitmap files
            char bmpfilename1[1024];

            sprintf(bmpfilename1,"map%d.bmp",i);
            facets[i].map = SDL_LoadBMP(bmpfilename1);

            sprintf(bmpfilename1,"map%d_rot.bmp",i);
            facets[i].rotatedmap = SDL_LoadBMP(bmpfilename1);

            if(facets[i].map == NULL || facets[i].rotatedmap == NULL)
            {
                    MessageBox(0, "Could not load cached maps, delete maps.cfg from running directory and run again.", "Error",MB_OK);
                    exit(-1);
            }
        }
    }
}


//this is where callbacks to your program happen after it gets registered, for example I put in facet changes here
//so you can automatically swap maps when the user goes to a different facet

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch(msg)
    {
        case (WM_USER+313): //facet changed
            currentfacet = (unsigned long)wParam;
            break;
        case WM_CLOSE:
            DestroyWindow(hwnd);
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}


//if you need me to explain what this function is for...turn back now
void drawString(SDL_Surface* screen, char *text, int x, int y, TTF_Font *font, char r, char g, char b)
{
    SDL_Rect dest;
    SDL_Surface *surface;
    SDL_Color foregroundColor;

    foregroundColor.r = r;
    foregroundColor.g = g;
    foregroundColor.b = b;

    surface = TTF_RenderText_Blended(font, text, foregroundColor);

    if (surface == NULL)
        return;

    dest.x = x;
    dest.y = y;
    dest.w = surface->w;
    dest.h = surface->h;

    SDL_BlitSurface(surface, NULL, screen, &dest);

    SDL_FreeSurface(surface);
}

//quickly hacked together function to translate X and Y coordinates to UO's
//"Latitude and Longitude" system, takes facet/dungeons/lost lands into account
char* LatLon(char* outputstring,int x, int y, int facet)
{
    float centerx = 1323;
    float centery = 1624;
    float width = 1;
    float height = 1;

    //fel or tram
    if(facet ==0 || facet == 1)
    {
        if(x < 5120)
        {
            centerx = 1323;
            centery = 1624;
        }
        else
        {
            if(y < 2304)
            {
                sprintf(outputstring,"%s", "In a dungeon");
                return outputstring;
            }
            else
            {
                centerx = 5936;
                centery = 3112;
            }
        }
    }

    width = facets[facet].width;
    height = facets[facet].height;

    //malas 1323

    float degx = (x - centerx) * 360.0 / width;
    float degy = (y - centery) * 360.0 / height;

    bool east = true;
    bool south = true;

    char ew[2] = "";
    char ns[2] = "";

    int degreesx = (int)degx;
    int degreesy = (int)degy;
    int minutesx = (int)(((float)degx-(float)degreesx)*60.0);
    int minutesy = (int)(((float)degy-(float)degreesy)*60.0);

    while(degreesx > 180)
    {
        east = !east;
        degreesx -=180;
    }

    while(degreesx < 0)
    {
        east = !east;
        degreesx += 180;
    }

    while(degreesy > 180)
    {
        south = !south;
        degreesy -=180;
    }

    while(degreesy < 0)
    {
        south = !south;
        degreesy += 180;
    }

    if(east && !(minutesx<0))
        ew[0]='E';
    else
    {
        minutesx = -minutesx;
        degreesx = 180-degreesx;
        if(degreesx == 180)
            degreesx = 0;
        ew[0]='W';
    }

    if(south && !(minutesy<0))
        ns[0]='S';
    else
    {
        degreesy = 180-degreesy;
        minutesy = -minutesy;
        if(degreesy == 180)
            degreesy == 0;
        ns[0]='N';
    }

    if(minutesx < 0)
    {
        degreesx -=1;
        minutesx = 60+minutesx;
    }

    if(minutesy < 0)
    {
        degreesy -=1;
        minutesy = 60+minutesy;
    }

    sprintf(outputstring,"%d,%d %s : %d,%d %s",degreesy,minutesy,ns,degreesx,minutesx,ew);

    return outputstring;


}


//because everone likes pi...

#define PI 3.14159265

//you know...fuck it...this is already a bastardization of windows and linux code
//using WineLib.....screw form and function, i'm making a giant "main" function

//something seems unholy about this.....
int WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    MSG msg;
    BOOL bRet;
    WNDCLASS wc;

    TTF_Font *font;

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
    rmask = 0xff000000;
    gmask = 0x00ff0000;
    bmask = 0x0000ff00;
    amask = 0x000000ff;
#else       //////////ff   HERE THERE BE DRAGONS!!!
    rmask = 0x000000ff;
    gmask = 0x0000ff00;
    bmask = 0x00ff0000;
    amask = 0xff000000;
#endif

    SetFacets();
    LoadColors();
    LoadConfig();
    LoadMaps();

    // Register the window class for the main window.

    if (!hPrevInstance)
    {
        wc.style = 0;
        wc.lpfnWndProc = (WNDPROC)WndProc;
        wc.cbClsExtra = 0;
        wc.cbWndExtra = 0;
        wc.hInstance = hInstance;
        wc.hIcon = LoadIcon((HINSTANCE) NULL,
            IDI_APPLICATION);
        wc.hCursor = LoadCursor((HINSTANCE) NULL,
            IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
        wc.lpszMenuName =  "MainMenu";
        wc.lpszClassName = "MainWndClass";

        if (!RegisterClass(&wc))
        {
            MessageBox(0, "Could not register class", "Oh Noes!",MB_OK);
            return FALSE;
        }
    }

    hinst = hInstance;  // save instance handle

    // Create the main window.

    hwndMain = CreateWindow("MainWndClass", "Your Mom",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        CW_USEDEFAULT, CW_USEDEFAULT, (HWND) NULL,
        (HMENU) NULL, hinst, (LPVOID) NULL);

    // If the main window cannot be created, terminate
    // the application.

    if (!hwndMain)
    {
        MessageBox(0, "Could not create window.", "Error",MB_OK);
        return FALSE;
    }

    // Show the window and paint its contents.

    //ShowWindow(hwndMain, nCmdShow);
    //UpdateWindow(hwndMain);

    //...or not....mwa ha ha ha ha

    //other init
    /* initialize SDL */
    SDL_Init(SDL_INIT_VIDEO);

    atexit(SDL_Quit);

    /* set the title bar */
    SDL_WM_SetCaption("OpenUOMap", "OpenUOMap");

    /* create window */
    SDL_Surface* screen = SDL_SetVideoMode(640, 480, 0, SDL_DOUBLEBUF|SDL_HWSURFACE|SDL_RESIZABLE);

    if (TTF_Init() < 0)
    {
        printf("Couldn't initialize SDL TTF: %s\n", SDL_GetError());
        exit(1);
    }

    //todo: pack font with program, right now this points to a font i used in another application
    font = TTF_OpenFont("/home/raptor85/projects/game2010/fonts/FreeSansY.ttf",14);

    if (font == NULL)
    {
        printf("Failed to open Font: %s\n", TTF_GetError());
        exit(1);
    }

    map_hWnd = FindWindow("UOASSIST-TP-MSG-WND", NULL);
    if (map_hWnd==NULL)
    {
        MessageBox(0, "UOAssist is not running ", "OH FU!!~~~",MB_OK); return 0;
    }

    if(SendMessage(map_hWnd,WM_USER+200,(WPARAM)hwndMain,1) != 1)
    {
        MessageBox(0, "Could not hook with UOAssist", "Error",MB_OK); return 0;
    }


    // Start the message loop.

    int oldx=0;
    int oldy=0;

    bool fDone = false;
    while (!fDone)
    {
        SDL_Event event;
        while(SDL_PollEvent(&event))
        {
            if(event.type == SDL_QUIT)
            {
                SendMessage(map_hWnd,WM_USER+200,(WPARAM)hwndMain,0);
                //todo: save off facet here
                appconfig.lastfacet = currentfacet;
                appconfig.rotated = rotate;
                SaveConfig();
                exit(0);
            }
            if(event.type == SDL_VIDEORESIZE)
            {
                SDL_SetVideoMode(event.resize.w, event.resize.h, 0, SDL_DOUBLEBUF|SDL_HWSURFACE|SDL_RESIZABLE);
            }
        }

        if (PeekMessage(&msg, hwndMain,  0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {

        }
        SDL_FillRect( SDL_GetVideoSurface(), NULL, 0 );
        //stream coordinates from UOAssist
            unsigned long res = SendMessage(map_hWnd, WM_USER + 202, 0, 0 );
            int x = LOWORD(res);
            int y = HIWORD(res);

            if(x==oldx && y==oldy)
                continue;

            oldx = x;
            oldy = y;

            //horrible way to do this, but fuck it, this was written in a few hours
            char locationtext[32];
            char locationtext2[32];
            sprintf(locationtext,"%d : %d",x,y);

            SDL_Rect srcrect,destrect;

            appconfig.zoomlevel = 0.5;

            if(rotate)
            {
                int newx,newy;
                int cx, cy;
                float scalex,scaley;

                cx = facets[currentfacet].map->w/2;
                cy = facets[currentfacet].map->h/2;

                //BEHOLD - THE POWER OF PI!!!

                newx = ((x-cx)*cos(45.0*PI/180.0))-((y-cy)*sin(45.0*PI/180.0));
                newy = ((x-cx)*sin(45.0*PI/180.0))+((y-cy)*cos(45.0*PI/180.0));

                newx += (facets[currentfacet].rotatedmap->w/2);
                newy += (facets[currentfacet].rotatedmap->h/2);

                //anyways....


                srcrect.x = ((float)newx)-((SDL_GetVideoSurface()->w/2)*appconfig.zoomlevel);
                srcrect.y = ((float)newy)-((SDL_GetVideoSurface()->h/2)*appconfig.zoomlevel);
                srcrect.w = SDL_GetVideoSurface()->w*appconfig.zoomlevel;
                srcrect.h = SDL_GetVideoSurface()->h*appconfig.zoomlevel;

                destrect.x =0;
                destrect.y =0;
                destrect.w = SDL_GetVideoSurface()->w;
                destrect.h = SDL_GetVideoSurface()->h;

                //if i had written a blit function, i'd call it like this
               //SDL_StretchBlit(facets[currentfacet].rotatedmap, &srcrect, SDL_GetVideoSurface(),&destrect);

                SDL_BlitSurface(facets[currentfacet].rotatedmap, &srcrect, SDL_GetVideoSurface(),NULL);
            }
            else
            {
                srcrect.x = ((float)x)-((SDL_GetVideoSurface()->w/2)*appconfig.zoomlevel);
                srcrect.y = ((float)y)-((SDL_GetVideoSurface()->h/2)*appconfig.zoomlevel);
                srcrect.w = SDL_GetVideoSurface()->w*appconfig.zoomlevel;
                srcrect.h = SDL_GetVideoSurface()->h*appconfig.zoomlevel;


                SDL_BlitSurface(facets[currentfacet].map, &srcrect, SDL_GetVideoSurface(),NULL);
            }

            drawString(screen,locationtext,0,0,font,255,255,255);
            drawString(screen,LatLon(locationtext2,x,y,currentfacet),0,16,font,255,255,255);
            drawString(screen,facets[currentfacet].name,0,32,font,255,255,255);


            SDL_Flip(screen);
           sleep(0);

    }

    // Return the exit code to the system.

    //screw cleanup, this is for windows, people expect stuff to crash and keep memory in use

    //....ok...maybe i'll clean up SOME of it
    TTF_Quit();
    SDL_Quit();

    return msg.wParam;


}
