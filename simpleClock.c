/******************************************************************************\
*  simpleClock.c                                                               *
*                                                                              * 
*  A simple ASCII clock built with no dependecies.                             *
*                                                                              *
*              Written by A.N                                 October 2022     *
*                                                                              *
\******************************************************************************/
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <wchar.h>
#include <locale.h>
#include <stdlib.h>
#include <memory.h>
#include <math.h>
#include <signal.h>
#include <setjmp.h>
#include <time.h>

/* Space between segments.                                                    */
#define SPACE 4

/* ASCII codes definitions borrowed straight from LibASCII                    */
#define ASCII_CLEAR_SCREEN "\033[2J"
#define ASCII_RESET_TERM "\033c"
#define ASCII_TURN_CURSOR_OFF "\033[?25l"
#define ASCII_TURN_CURSOR_ON "\033[?25h"
#define ASCII_ESCAPE "\033["
/******************************************************************************/

#define MOVE_CURSOR(r, c) printf(ASCII_ESCAPE "%d;%dH", (r), (c))
#define GET_INDEX(r, c) ((c) + (r) * (Columns))

/* ASCII block data.                                                          */
wchar_t 
	*Blocks[] = {L" ", L"░", L"▒", L"▓", L"█"};
			
typedef enum Block {
	BLOCK_WHITE = 0,
	BLOCK_LIGHT_GREY,
	BLOCK_GREY,
	BLOCK_DARK_GREY,
	BLOCK_BLACK,
	BLOCK_TOTAL_TONES
} Block;

/* Draws the block b at row r and column c.                                   */
static inline void SetBlock(Block b, int r, int c);

/* Clears the screen.                                                         */
static inline void ClearScreen();

/* Screen definitions.                                                        */
static Block *Screen;
static size_t Rows;
static size_t Columns;
static int Size;
static int Space;

/* Draw the current screen buffer to the terminal.                            */
static void DrawFrame();

/* Init and cleanup code + signal handler.                                    */
static void Init();
static void Resize();
static void Cleanup();
static void Handle(int Signal);

/* For long goto after resizing the window.                                   */
static jmp_buf jmp;

/* Number drawing functions:                                                  */
static void DrawSegment(int Digit, int r, int c, int Size, Block b);
static void DrawNumber(int Num, int r, int c, Block b);

int main()
{
	Init();	
	sigsetjmp(jmp, 1);
	struct tm *CurrentTime;
	time_t RawTime;
	while (1) {
		ClearScreen();

/* Fetch the time.                                                            */
		time(&RawTime);
		CurrentTime = localtime(&RawTime);

/* Fit the clock on the screen.                                               */
		int TotalX = (Space + Size) * 5 + Size,
		    OffsetX = (Columns - TotalX) / 2,
		    TotalY = Size,
		    OffsetY = (Rows - TotalY) / 2;


		DrawNumber(CurrentTime->tm_hour,
				   OffsetY,
				   OffsetX, BLOCK_DARK_GREY);	

		SetBlock(BLOCK_BLACK, OffsetY + 3,
				              OffsetX + (Space + Size) * 2 - Space/2);

		SetBlock(BLOCK_BLACK, OffsetY + Size - 3,
				              OffsetX + (Space + Size) * 2 - Space/2);

		DrawNumber(CurrentTime->tm_min,
				   OffsetY,
				   OffsetX + (Space + Size) * 2, BLOCK_GREY);	

		SetBlock(BLOCK_BLACK, OffsetY + 3,
				              OffsetX + (Space + Size) * 4 - Space/2);

		SetBlock(BLOCK_BLACK, OffsetY + Size - 3,
				              OffsetX + (Space + Size) * 4 - Space/2);

		DrawNumber(CurrentTime->tm_sec,
				   OffsetY,
				   OffsetX + (Space + Size) * 4, BLOCK_LIGHT_GREY);	

		DrawFrame();

/* We only need to run once per second.                                       */
		sleep(1);
	}

}

static void Init()
{

/* Enable UTF-8 printing so we can output the blocks.                         */
	setlocale(LC_ALL, "");

/* Turn off the cursor.                                                       */
	printf(ASCII_TURN_CURSOR_OFF);	

/* Fetch the window size.                                                     */
	Resize();

/* Bind the signal handler.                                                   */
	signal(SIGKILL, Handle);
	signal(SIGSTOP, Handle);
	signal(SIGINT, Handle);
	signal(SIGSEGV, Handle);
	signal(SIGWINCH, Handle);
}

static void Cleanup() 
{

/* Free buffer memory and turn the cursor back on.                            */
	free(Screen);
	printf(ASCII_TURN_CURSOR_ON);
}

static inline void SetBlock(Block b, int r, int c)
{

/* Bounds check.                                                              */
	if (r < 0 || r >= Rows ||
		c < 0 || c >= Columns)
		return;

/* Fetch the index and set the Screen.                                        */
	int 
		Index = GET_INDEX(r, c);

	Screen[Index] = b;
}

static inline void ClearScreen()
{
	memset(Screen, 0, Rows * Columns * sizeof(*Screen));
}

static void DrawFrame()
{

/* Dump the screen buffer on the screen.                                      */
	for (int r = 0; r < Rows; r++) {
		for (int c = 0; c < Columns; c++) {
			int 
				Index = GET_INDEX(r, c);
			
			MOVE_CURSOR(r, c);
			printf("%ls", Blocks[Screen[Index]]);
		}
	}

/* Flush stdout to make sure everything printed correctly.                    */
	fflush(stdout);
}

static void Resize() 
{

/* Query the terminal width and height first.                                 */
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
	Rows = w.ws_row + 1;
	Columns = w.ws_col + 1;

/* Allocate the screen buffer and clear it.                                   */
	Screen = malloc(sizeof(*Screen) * Rows * Columns);
	if (!Screen)
		perror("malloc");

	memset(Screen, 0, Rows * Columns * sizeof(*Screen));

/* Make sure size fits.                                                       */
	Size = (Columns - Space * 5 - 2) / 6;
	Size = Size - (Size % 2);
	Size = fmax(Size, 4);

/* Make sure Space fits.                                                      */
	Space = fmin(SPACE, (Columns - Size*6 - 2) / 5);
	Space = Space - (Space % 2);
	Space = fmax(Space, 2);
}

static void Handle(int Signal) 
{
	if (Signal == SIGWINCH) {
		free(Screen);
		Screen = NULL;
		Resize();
		longjmp(jmp, 1);
	}

	Cleanup();
	exit(Signal == SIGSEGV);
}

char 
	Numbers[] =  {0x7d, 0x18, 0x37,0x67, 0x6a, 0x4f, 0x5f, 0x61, 0xff, 0x6b};

static void DrawSegment(int Num, int r, int c, int Size, Block b)
{
	int 
		Segments[7][2] = {{0, 0},
						  {0, Size / 2},
						  {0, Size},
						  {0, 0},
						  {0, Size / 2},
						  {Size, 0},
						  {Size, Size / 2}};

	/* Make sure we're in range.                                                  */
	Num = fmax(fmin(Num, 9.f), 0.f);
	char 
		Segment = Numbers[Num];

	if (!Segment)
		return;

	for (int i = 0; i < 7; i++) {

/* Draw this segment?                                                         */
		if (!(Segment & (1 << i)))
			continue;

/* Get the coordinates and dircetion and draw the segment.                    */
		int 
			Horiz = i < 3;

		for(int j = 0; j <= Size * Horiz + (Size/2) * !Horiz; j++) {
			int 
				x = Segments[i][0] + j * Horiz,
			    y = Segments[i][1] + j * !Horiz;
			SetBlock(b, r + y, c + x);
		}
	}

}

static void DrawNumber(int Num, int r, int c, Block b)
{
	int 
		Ones = Num % 10,
		Tens = Num / 10;

	DrawSegment(Tens, r, c, Size, b);
	DrawSegment(Ones, r, c + Size + Space, Size, b);
}
