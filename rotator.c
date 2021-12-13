/******************************************************************************
 *
 * Copyright:
 *    (C) 2006 Sulaiman Vali
 *
 * File:
 *    rotator.c
 *
 * Description:
 *    Implementation of the Rotator puzzle.
 *
 *****************************************************************************/

/******************************************************************************
 * Includes
 *****************************************************************************/
#include "../pre_emptive_os/api/osapi.h"
#include "../pre_emptive_os/api/general.h"
#include <printf_P.h>
#include <ea_init.h>
#include <stdlib.h>
#include "lcd.h"
#include "key.h"
#include "select.h"
#include "hw.h"
#include "rotator.h"
#include "rotator_images/arrows.h"

/******************************************************************************
 * Typedefs and defines
 *****************************************************************************/
#define DEBUG_BOARD

#define COLOR_RED ((tU8) 0xe0)
#define COLOR_YELLOW ((tU8) 0xfc)
#define COLOR_BLUE ((tU8) 0x03)
#define COLOR_GREEN ((tU8) 0x1c)
#define COLOR_PURPLE ((tU8) 0xe3)
#define COLOR_BROWN ((tU8) 0xec)

#define GRID_COLS ((tU8) 3)
#define GRID_ROWS ((tU8) 3)
#define MAX_TILES ((tU8) (GRID_COLS*GRID_ROWS))

#define MAX_COL_MOVES ((tU8) (GRID_COLS-2))
#define MAX_ROW_MOVES ((tU8) (GRID_ROWS-2))
/* For grids greater than 3 x 3, you will need to redesign the way of
 * rotating blocks if the cursor is on an edge but not on a corner */

#define BLOCK_WIDTH ((tU8) 2)
#define BLOCK_HEIGHT ((tU8) 2)

// pixel #defines
#define SCREEN_WIDTH  ((tU8) 130)
#define SCREEN_HEIGHT ((tU8) 130)
#define CHAR_HEIGHT ((tU8) 14)
#define CHAR_WIDTH ((tU8) 8)

#define BORDER_WIDTH ((tU8) 1)
#define ARROW_LENGTH ((tU8) 35)
#define ARROW_BREADTH ((tU8) 8)
#define ARROW_BRD_GAP ((tU8) 1)
#define ARROW_OFFSET ((tU8) 3)

// target preview window
#define TARGET_TILEW ((tU8) 12)
#define TARGET_TILEH ((tU8) 12)
#define TARGET_OFFSET_X ((tU8) 0)
#define TARGET_OFFSET_Y ((tU8) 0)
#define TARGET_BOARD_WIDTH ((tU8) (GRID_COLS*TARGET_TILEW))
#define TARGET_BOARD_HEIGHT ((tU8) (GRID_ROWS*TARGET_TILEH))

// main board
#define TILEW ((tU8) 25)
#define TILEH ((tU8) 25)
#define OFFSET_X ((tU8) (TARGET_BOARD_WIDTH + ARROW_BREADTH + ARROW_BRD_GAP))
#define OFFSET_Y ((tU8) (ARROW_BREADTH + CHAR_HEIGHT + ARROW_BRD_GAP))
#define BOARD_WIDTH ((tU8) (GRID_COLS*TILEW))
#define BOARD_HEIGHT ((tU8) (GRID_ROWS*TILEH))

#define CENTERED_START(WINDOW_SIZE, OBJECT_SIZE) \
        (((WINDOW_SIZE) - (OBJECT_SIZE)) / 2)

typedef enum {
  GAME_NOT_STARTED = 0,
  GAME_RUNNING,
  GAME_OVER,
  GAME_END
} tGameState;

#define MAX_GAME_MODES (6)
typedef enum {
  MODE_2_COLORS = 2,
  MODE_3_COLORS = 3,
  MODE_4_COLORS = 4,
  MODE_5_COLORS = 5,
  MODE_6_COLORS = 6,
  MODE_9_NUMBERS = 9
} tGameMode;

/*****************************************************************************
 * Local variables
 ****************************************************************************/
static tGameState gameStatus;
static tGameMode gameMode;

static tU16 moves = 0;
static tU8 colIndex; // the topleft column position of the current block
static tU8 rowIndex; // the topleft row position of the current block
static tU8 gameModeIndex;
static tBool tileModeUseNumbers;
static char movesStr[] = "Moves:    ";
static unsigned char tileColors[6] = {COLOR_RED, COLOR_YELLOW, COLOR_BLUE,
                                      COLOR_GREEN, COLOR_PURPLE, COLOR_BROWN};
static tU8 board[GRID_ROWS][GRID_COLS];
static tU8 targetBoard[MAX_GAME_MODES-1][GRID_ROWS][GRID_COLS] = {
       {{0, 1, 0},
        {1, 0, 1},
        {0, 1, 0}},

       {{0, 0, 0},
        {1, 1, 1},
        {2, 2, 2}},

       {{0, 0, 2},
        {0, 3, 1},
        {2, 1, 1}},

       {{2, 1, 0},
        {3, 2, 1},
        {4, 3, 2}},

       {{5, 0, 0},
        {1, 4, 2},
        {1, 2, 3}}
        };

/*****************************************************************************
 * External variables
 ****************************************************************************/
extern volatile tU32 ms;

/*****************************************************************************
 *
 * Description:
 *    Draw the score or whatever represents it.
 *
 ****************************************************************************/
static void drawMoves()
{
   movesStr[6]  = moves<1000 ? ' ' : moves/1000 + '0';
   movesStr[7]  = moves<100 ? ' ' : (moves%1000)/100 + '0';
   movesStr[8]  = moves<10 ? ' ' : (moves%100)/10 + '0';
   movesStr[9] = moves%10 + '0';

   lcdColor(0, 0xff);
   lcdGotoxy(CENTERED_START(SCREEN_WIDTH,sizeof(movesStr)*CHAR_WIDTH),
             SCREEN_HEIGHT-CHAR_HEIGHT);
   lcdPuts(movesStr);
}


/***************************************************************************/
static void redrawArrows(void)
{
  // clear any previous arrows
  lcdRect(OFFSET_X+ARROW_OFFSET, OFFSET_Y-ARROW_BRD_GAP-ARROW_BREADTH,
          BOARD_WIDTH-(2*ARROW_OFFSET), ARROW_BREADTH, 0);
  lcdRect(OFFSET_X+ARROW_OFFSET, OFFSET_Y+BOARD_HEIGHT+ARROW_BRD_GAP,
          BOARD_WIDTH-(2*ARROW_OFFSET), ARROW_BREADTH, 0);
  lcdRect(OFFSET_X-ARROW_BRD_GAP-ARROW_BREADTH, OFFSET_Y+ARROW_OFFSET,
          ARROW_BREADTH, BOARD_WIDTH-(2*ARROW_OFFSET), 0);
  lcdRect(OFFSET_X+BOARD_WIDTH+ARROW_BRD_GAP, OFFSET_Y+ARROW_OFFSET,
          ARROW_BREADTH, BOARD_WIDTH-(2*ARROW_OFFSET), 0);

  if (colIndex == 0)
  {
    if (rowIndex == 0)
    {
      lcdIcon(OFFSET_X+ARROW_OFFSET, OFFSET_Y-ARROW_BREADTH-ARROW_BRD_GAP,
              ARROW_LENGTH, ARROW_BREADTH,
              _right_arrow[2], _right_arrow[3], &_right_arrow[4]);
      lcdIcon(OFFSET_X-ARROW_BREADTH-ARROW_BRD_GAP, OFFSET_Y+ARROW_OFFSET,
              ARROW_BREADTH, ARROW_LENGTH,
              _down_arrow[2], _down_arrow[3], &_down_arrow[4]);
    }
    else /* if (rowIndex == MAX_ROW_MOVES) */
    {
      lcdIcon(OFFSET_X-ARROW_BREADTH-ARROW_BRD_GAP,
              OFFSET_Y+BOARD_HEIGHT-ARROW_LENGTH-ARROW_OFFSET,
              ARROW_BREADTH, ARROW_LENGTH,
              _up_arrow[2], _up_arrow[3], &_up_arrow[4]);
      lcdIcon(OFFSET_X+ARROW_OFFSET, OFFSET_Y+BOARD_HEIGHT+ARROW_BRD_GAP,
              ARROW_LENGTH, ARROW_BREADTH,
              _right_arrow[2], _right_arrow[3], &_right_arrow[4]);
    }
  }
  else /* if(colIndex == MAX_COL_MOVES) */
  {
    if (rowIndex == 0)
    {
      lcdIcon(OFFSET_X+BOARD_WIDTH-ARROW_LENGTH-ARROW_OFFSET,
              OFFSET_Y-ARROW_BREADTH-ARROW_BRD_GAP,
              ARROW_LENGTH, ARROW_BREADTH,
              _left_arrow[2], _left_arrow[3], &_left_arrow[4]);
      lcdIcon(OFFSET_X+BOARD_WIDTH+ARROW_BRD_GAP, OFFSET_Y+ARROW_OFFSET,
              ARROW_BREADTH, ARROW_LENGTH,
              _down_arrow[2], _down_arrow[3], &_down_arrow[4]);
    }
    else /* if (rowIndex == MAX_ROW_MOVES) */
    {
      lcdIcon(OFFSET_X+BOARD_WIDTH+ARROW_BRD_GAP,
              OFFSET_Y+BOARD_HEIGHT-ARROW_LENGTH-ARROW_OFFSET,
              ARROW_BREADTH, ARROW_LENGTH,
              _up_arrow[2], _up_arrow[3], &_up_arrow[4]);
      lcdIcon(OFFSET_X+BOARD_WIDTH-ARROW_OFFSET-ARROW_LENGTH,
              OFFSET_Y+BOARD_HEIGHT+ARROW_BRD_GAP,
              ARROW_LENGTH, ARROW_BREADTH,
              _left_arrow[2], _left_arrow[3], &_left_arrow[4]);
    }
  }
}

/***************************************************************************/
static void redrawCursorAndArrows(void)
{
  tU8 i;

  // Draw all grid borders
  for (i=0; i<=GRID_COLS; i++)
    lcdRect(OFFSET_X+(TILEW*i), OFFSET_Y, 1, BORDER_WIDTH+BOARD_HEIGHT, 0x6d);

  for (i=0; i<=GRID_ROWS; i++)
    lcdRect(OFFSET_X, OFFSET_Y+(TILEH*i), BORDER_WIDTH+BOARD_WIDTH, 1, 0x6d);

  // Draw all cursor block borders
  for (i=0; i<=2; i+=2)
    lcdRect(OFFSET_X+(TILEW*(colIndex+i)), OFFSET_Y+(TILEH*rowIndex),
            1, BORDER_WIDTH+BLOCK_HEIGHT*TILEH, 0xff);

  for (i=0; i<=2; i+=2)
    lcdRect(OFFSET_X+(TILEW*colIndex), OFFSET_Y+(TILEH*(rowIndex+i)),
            BORDER_WIDTH+BLOCK_WIDTH*TILEW, 1, 0xff);

  redrawArrows();
}

/*****************************************************************************
 *
 * Description:
 *    Draw the tile for this unique number.
 * Pre-condition
 *    tileId must be less than sizeof(tileColors) unless tileModeUseNumbers is TRUE
 ****************************************************************************/
static void drawTile(tU8 x, tU8 y, tU8 xLen, tU8 yLen, tU8 tileId)
{
  char str[] = " ";
  str[0] = tileId+1 + '0';

  if (tileModeUseNumbers)
  {
    lcdColor(0, 0xfc);
    lcdGotoxy(x+CENTERED_START(TILEW,CHAR_WIDTH), y+CENTERED_START(TILEH,CHAR_HEIGHT));
    lcdPuts(str);
  }
  else
  {
    lcdRect(x, y, xLen, yLen, tileColors[tileId]);
  }
}

/***************************************************************************/
static void redrawMainBoard()
{
  tU8 i, j;
  for (i=0; i<GRID_ROWS; i++)
    for (j=0; j<GRID_COLS; j++)
      drawTile(OFFSET_X+(TILEW*j), OFFSET_Y+(TILEH*i), TILEW, TILEH, board[i][j]);
}

/***************************************************************************/
static void redrawTargetBoard()
{
  tU8 i, j;

  if (tileModeUseNumbers)
  {
    char str[] = "Sort from 1 - 9";
    lcdColor(0, 0xfc);
    lcdGotoxy(CENTERED_START(SCREEN_WIDTH,(sizeof(str)-1)*CHAR_WIDTH), 0);
    lcdPuts(str);
  }
  else
  {
    for (i=0; i<GRID_ROWS; i++)
      for (j=0; j<GRID_COLS; j++)
        drawTile(TARGET_OFFSET_X+(TARGET_TILEW*j), TARGET_OFFSET_Y+(TARGET_TILEH*i),
                 TARGET_TILEW, TARGET_TILEH, targetBoard[gameModeIndex][i][j]);
  }
}

/***************************************************************************/
static void redrawGameScreen()
{
  //draw background
  lcdColor(0, 0xff);
  lcdClrscr();

  redrawMainBoard();
  redrawTargetBoard();
  redrawCursorAndArrows();
  drawMoves();
}

/*****************************************************************************
 *
 * Description:
 *    Return a random unpicked tile
 *
 ****************************************************************************/
static tU8 getRandomUnpicked (tU16 *bitMask, tU8 max)
{
  tU8 i, randNo, tmpUnpicked = 0;

  randNo = rand() % max;

  // iterate through the unpicked tiles (marked by zeros in the bitMask)
  // and select the randNo'th unpicked tile.
  for (i=0; i<MAX_TILES; i++)
    if ((*bitMask & (1<<i)) == 0)
    {
      if (tmpUnpicked++ == randNo)
      {
        *bitMask |= 1<<i;  /* reserve this bit */
        break;
      }
    }

  return i;
}

/*****************************************************************************
 *
 * Description:
 *    Draw game background and game board, initialize all variables
 *
 ****************************************************************************/
static void setupGame (void)
{
  tU8 i, j, randTile, index, tileId;
  tU16 pickedTilesBitMask = 0;

  moves = 0;
  colIndex  = 0;
  rowIndex  = 0;

  //initialize random generator
  srand(ms);

  //initialise game board with random numbers
  for (i=0; i<GRID_ROWS; i++)
  {
    for (j=0; j<GRID_COLS; j++)
    {
      index = i*GRID_COLS + j;

      randTile = getRandomUnpicked(&pickedTilesBitMask, MAX_TILES-index);
      if (tileModeUseNumbers)
        tileId = randTile;
      else
        tileId = targetBoard[gameModeIndex][randTile/GRID_ROWS][randTile%GRID_COLS];
      board[i][j] = tileId;
    }
  }

  redrawGameScreen();
}

/***************************************************************************/
static void redrawCurrentBlock()
{
  tU8 i, j;

  for (i=rowIndex; i<rowIndex+BLOCK_WIDTH; i++)
    for (j=colIndex; j<colIndex+BLOCK_HEIGHT; j++)
      drawTile(OFFSET_X+(TILEW*j), OFFSET_Y+(TILEH*i),
               TILEW, TILEH, board[i][j]);
}

/***************************************************************************/
static void rotateBlock(tBool clockwise)
{
  tU8 topLeft;
  topLeft = board[rowIndex][colIndex];

  if (clockwise)
  {
    board[rowIndex][colIndex] = board[rowIndex+1][colIndex];
    board[rowIndex+1][colIndex] = board[rowIndex+1][colIndex+1];
    board[rowIndex+1][colIndex+1] = board[rowIndex][colIndex+1];
    board[rowIndex][colIndex+1] = topLeft;
  }
  else
  {
    board[rowIndex][colIndex] = board[rowIndex][colIndex+1];
    board[rowIndex][colIndex+1] = board[rowIndex+1][colIndex+1];
    board[rowIndex+1][colIndex+1] = board[rowIndex+1][colIndex];
    board[rowIndex+1][colIndex] = topLeft;
  }

  redrawCurrentBlock();

  moves++;
  drawMoves();
}

/***************************************************************************/
static void moveCursorOrRotateBlock(tU8 key)
{
  switch (key)
  {
    case KEY_LEFT:
      if (colIndex == 0)
        rotateBlock(rowIndex == MAX_ROW_MOVES);
      else
        colIndex--;
      break;
    case KEY_RIGHT:
      if (colIndex == MAX_COL_MOVES)
        rotateBlock(rowIndex == 0);
      else
        colIndex++;
      break;
    case KEY_UP:
      if (rowIndex == 0)
        rotateBlock(colIndex == 0);
      else
        rowIndex--;
      break;
    case KEY_DOWN:
      if (rowIndex == MAX_ROW_MOVES)
        rotateBlock(colIndex == MAX_COL_MOVES);
      else
        rowIndex++;
      break;
  }

  redrawCursorAndArrows();
}

/*****************************************************************************
 *
 * Description:
 *    Check whether the board is sorted.
 *
 ****************************************************************************/
static tBool hasWon()
{
  tU8 i, j, num=0;

  for (i=0; i<GRID_ROWS; i++)
    for (j=0; j<GRID_COLS; j++)
    {
      if (tileModeUseNumbers)
      {
        if (board[i][j] != num++)
          return FALSE;
      }
      else
      {
        if (board[i][j] != targetBoard[gameModeIndex][i][j])
          return FALSE;
      }
    }

  return TRUE;
}

/*****************************************************************************
 *
 * Description:
 *    Cheer and jump with joy.
 *
 ****************************************************************************/
static void celebrate()
{
  tU8 i, j;

  lcdColor(COLOR_RED, COLOR_YELLOW);
  lcdGotoxy(5,25);  lcdPuts("~*~ Hip Hip ~*~");
  lcdGotoxy(5,40);  lcdPuts("~*~ Horray! ~*~");
  lcdGotoxy(5,55);  lcdPuts("    You won!    ");

  for(i=0; i<3; i++)
  {
    for(j=0; j<3; j++)
    {
      setBuzzer(TRUE);
      osSleep(1);

      setBuzzer(FALSE);
      osSleep(1);
    }
    osSleep(10);
  }

  osSleep(100);
  lcdColor(0,0xcd);  lcdGotoxy(0,75);  lcdPuts("Press to restart");
}

/*****************************************************************************
 *
 * Description:
 *    Displays menu to select which game to play and setup that game
 *
 ****************************************************************************/
static tBool selectAndSetupGame(void)
{
  tMenu menu;
  tU8 menuResp;

  // clear screen
  lcdColor(0x0c, 0xff);
  lcdClrscr();

#define NO_OF_CHOICES 7

  menu.xPos = 20;
  menu.yPos = 4;
  menu.xLen = 6+(10*8);
  menu.noOfChoices = NO_OF_CHOICES;
  menu.yLen = (1+NO_OF_CHOICES)*14 + 6;
  menu.initialChoice = 3;
  menu.pHeaderText = "Difficulty";
  menu.headerTextXpos = 20;
  menu.pChoice[0] = "2 colours";
  menu.pChoice[1] = "3 colours";
  menu.pChoice[2] = "4 colours";
  menu.pChoice[3] = "5 colours";
  menu.pChoice[4] = "6 colours";
  menu.pChoice[5] = "9 numbers";
  menu.pChoice[6] = "Quit";
  menu.bgColor       = 0;
  menu.borderColor   = 0x6d;
  menu.headerColor   = 0;
  menu.choicesColor  = 0xfd;
  menu.selectedColor = 0xe0;

  switch(menuResp = drawMenu(menu))
  {
    case 0: gameMode = MODE_2_COLORS; break;
    case 1: gameMode = MODE_3_COLORS; break;
    case 2: gameMode = MODE_4_COLORS; break;
    case 3: gameMode = MODE_5_COLORS; break;
    case 4: gameMode = MODE_6_COLORS; break;
    case 5: gameMode = MODE_9_NUMBERS; break;
    case 6:
    default: gameStatus = GAME_END; return FALSE; //End game
  }

  tileModeUseNumbers  = (gameMode == MODE_9_NUMBERS);
  gameModeIndex = menuResp;
  gameStatus = GAME_RUNNING;
  setupGame();

  return TRUE; // start the game
}

/*****************************************************************************
 *
 * Description:
 *    Start game of Rotator
 *
 ****************************************************************************/
void playRotator(void)
{
  selectAndSetupGame();

  while(gameStatus != GAME_END)
  {
    tU8 key, i, j;

    key = checkKey();
    switch(gameStatus)
    {
      case GAME_NOT_STARTED:
        if (key != KEY_NOTHING)
        {
          selectAndSetupGame();
        }
        break;
      case GAME_RUNNING:
        if (key != KEY_NOTHING)
        {
          if (key == KEY_CENTER)
            gameStatus = GAME_OVER;
          else
          {
            moveCursorOrRotateBlock(key);
            if (hasWon())
            {
              celebrate();
              gameStatus = GAME_NOT_STARTED;
            }
#ifdef DEBUG_BOARD
            for (i=0; i<GRID_ROWS; i++)
            {
              for (j=0; j<GRID_COLS; j++)
                printf("%d ", board[i][j]);
              printf("\n");
            }
            printf("Row: %d, Col: %d \n\n", colIndex, rowIndex);
#endif
          }
        }
        break;

      case GAME_OVER:
      {
        tMenu menu;

        menu.xPos = 10;
        menu.yPos = 25;
        menu.xLen = 6+(13*8);
        menu.yLen = 5*14;
        menu.noOfChoices = 3;
        menu.initialChoice = 0;
        menu.pHeaderText = "End game?";
        menu.headerTextXpos = 20;
        menu.pChoice[0] = "Continue game";
        menu.pChoice[1] = "Restart game";
        menu.pChoice[2] = "End game";
        menu.bgColor       = 0;
        menu.borderColor   = 0x6d;
        menu.headerColor   = 0;
        menu.choicesColor  = 0xfd;
        menu.selectedColor = 0xe0;

        switch(drawMenu(menu))
        {
          case 0: gameStatus = GAME_RUNNING; redrawGameScreen(); break; //Continue game
          case 1: selectAndSetupGame(); break;  //Restart game
          case 2: gameStatus = GAME_END; break; //End game
          default: break;
        }
        break;
      }

      default:
        gameStatus = GAME_END;
        break;
    }
    osSleep(1);
  }
}

