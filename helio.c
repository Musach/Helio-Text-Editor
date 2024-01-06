/**************************************************************************************************
 * File: helio.c
 * Description:
 *  - A text editor program developed in C99, designed to interface with the Linux terminal. It
 *    utilizes escape sequence commands; for reference, consult the VT100 user guide:
 *    http://vt100.net/docs/vt100-ug/chapter3.html
 *
 * Sources:
 *  - This project draws inspiration and references from an online tutorial while maintaining
 *    distinct implementation: [Tutorial Link](https://viewsourcecode.org/snaptoken/kilo/04.
 *    aTextViewer.html)
 *************************************************************************************************/

//====================Includes====================//
#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

//====================Global Declarations====================//
#define HELIO_VERSION "0.0.1"
#define TAB_STOP 8
#define ABUFF_INIT \
    {              \
        NULL, 0    \
    }

// gives control key equivalent of input character by masking highest 3 bits of 8 bits
#define CTRL_KEY(c) ((c) & 0x1f) // control keys range from 0 to 31 (lowest 5 bits)

enum key
{
    UP_ARROW = 500,
    DOWN_ARROW,
    RIGHT_ARROW,
    LEFT_ARROW,
    PAGE_UP,
    PAGE_DOWN,
    HOME_KEY,
    END_KEY,
    DEL_KEY
};

typedef struct
{
    int size;
    char *text;

    int rendSize;
    char *rendStr; // render string
} TerminalRow;     // contains information for a row of text

typedef struct
{
    // termios is a data structure that defines the attributes of the terminal
    struct termios originalState;

    TerminalRow *tRow; // row of text array
    int tRowsTot;      // number of rows with text

    int cursorX; // cursor x postion
    int cursorY; // cursor y position

    int numRows; // number of rows on screen
    int numCols; // number of columns on screen

    int rowOffset; // keeps track of rows scrolled
    int colOffset; // keeps track of columns scrolled

    int maxRowOffset; // keeps track of max permissable vertical scrolling
    int maxColOffset; // keeps track of max permissable horizontal scrolling

    char statusMsg[80];
    time_t statusMsgTime; // from <time.h>

    char *fileName; // stores the name of the file opened

} TerminalAttr; // used for storing terminal/window related variables

typedef struct
{
    char *buff;
    int length;
} AppendBuffer; // used for creating dynamic strings; can change/add content to the same buffer

//====================Function Prototypes====================//
void AppendRow(TerminalAttr *attr, char *str, size_t rowSize);
void AppendString(AppendBuffer *abuff, const char *str, int length);
void ErrorHandler(const char *str);
int FetchWindowSize(int *numRows, int *numCols);
void FreeAbuff(AppendBuffer *abuff);
void InitTerminalAttr(TerminalAttr *attr);
void MoveCursor(TerminalAttr *attr, int key);
void OpenFile(TerminalAttr *attr, char *fileName);
int ProcessKeypress(TerminalAttr *attr);
void RawModeOff(struct termios originalState);
void RawModeOn(struct termios rawState);
int ReadKeypress();
void RefreshScreen(TerminalAttr *attr);
void RenderRow(TerminalRow *tRow);
void Scroll(TerminalAttr *attr, int key);
void WriteRows(TerminalAttr *attr, AppendBuffer *abuff);
void WriteStatusBar(TerminalAttr *attr, AppendBuffer *abuff);

//=============================================================//
//====================Function Declarations====================//
//=============================================================//

//------------------------------------------------//
//---------------Reading Keypresses---------------//
//------------------------------------------------//

/**************************************************************************************************
    Description:
    Monitors and captures key presses until a key event is registered. Translates registered
    keypresses into appropriate enum constants.

    Dependencies:
    - ErrorHandler
**************************************************************************************************/
int ReadKeypress()
{
    int readStatus = 0;
    char c;

    while (readStatus != 1)
    {
        readStatus = read(STDIN_FILENO, &c, 1);
        // reads a character input; ignore EAGAIN as an error (for cygwin)
        if ((readStatus == -1) && (errno != EAGAIN))
            ErrorHandler("read"); // gives error description
    }

    if (c == '\x1b')
    {
        char escSeq[3];
        int retSeq;

        if (read(STDIN_FILENO, &escSeq[0], 2) != 2) // checks if two chars were read
            retSeq = '\x1b';                        // if it timed out, retSeq = esc char

        else if (escSeq[0] == '[') // checks for an esc sequence starting with '['
        {
            if ((escSeq[1] >= '0') && (escSeq[1] <= '9')) // for three char esc seq
            {
                if (read(STDIN_FILENO, &escSeq[2], 1) != 1) // check for a third char
                    retSeq = '\x1b';

                else if (escSeq[2] == '~')
                {
                    switch (escSeq[1])
                    {
                    case '1':
                        retSeq = HOME_KEY; // many diff esc seq for Home key across diff OS's
                        break;
                    case '3':
                        retSeq = DEL_KEY;
                        break;
                    case '4':
                        retSeq = END_KEY; // many diff esc seq for End key across diff OS's
                        break;
                    case '5':
                        retSeq = PAGE_UP;
                        break;
                    case '6':
                        retSeq = PAGE_DOWN;
                        break;
                    case '7':
                        retSeq = HOME_KEY; // many diff esc seq for Home key across diff OS's
                        break;
                    case '8':
                        retSeq = END_KEY; // many diff esc seq for End key across diff OS's
                        break;
                    default:
                        retSeq = '\x1b';
                    }
                }
            }
            else // this case is for two character esc seq that follow the '['
            {
                switch (escSeq[1])
                {
                case 'A':
                    retSeq = UP_ARROW;
                    break;
                case 'B':
                    retSeq = DOWN_ARROW;
                    break;
                case 'C':
                    retSeq = RIGHT_ARROW;
                    break;
                case 'D':
                    retSeq = LEFT_ARROW;
                    break;
                case 'F':
                    retSeq = END_KEY; // many diff esc seq for End key across diff OS's
                    break;
                case 'H':
                    retSeq = HOME_KEY; // many diff esc seq for Home key across diff OS's
                    break;
                default:
                    retSeq = '\x1b'; // sequence is not valid, assume esc char
                }
            }
        }
        else if (escSeq[0] == 'O') // checks for esc seq starting with 'O'
        {
            switch (escSeq[1])
            {
            case 'F':
                retSeq = END_KEY; // many diff esc seq for End key across diff OS's
                break;
            case 'H':
                retSeq = HOME_KEY; // many diff esc seq for Home key across diff OS's
                break;
            default:
                retSeq = '\x1b';
            }
        }
        else
            retSeq = '\x1b'; // not an esc seq, assume esc char

        return retSeq;
    }

    return c;
}

/**************************************************************************************************
    Description:
    Utilizes ReadKeyPress() to handle registered keypresses. Returns 0 if 'Ctrl + Q' is pressed,
    typically indicating a program termination. Directs other keypresses to specific functions for
    further processing (e.g., arrow keys to MoveCursor() function).

    PAGE_UP and PAGE_DOWN go up or down a page respectively by calling MoveCursor iteratively. HOME
    sends cursorX to the beginning of a line while END sends it to the end of the current line.

    Dependencies:
    - ReadKeyPress
**************************************************************************************************/
int ProcessKeypress(TerminalAttr *attr)
{
    int key = ReadKeypress();

    switch (key)
    {
    case CTRL_KEY('q'):
        return 0; // returns 0 to end a while loop in main
        break;

    case UP_ARROW:
    case DOWN_ARROW:
    case RIGHT_ARROW:
    case LEFT_ARROW:
        MoveCursor(attr, key);
        break;

    case PAGE_UP:                                   // moves a whole page up
    case PAGE_DOWN:                                 // moves a whole page down
                                                    // goes to begnning of or end of next page by calling MoveCursor iteratively
        for (int i = 0; i < attr->numRows - 1; i++) // iterates one whole page length
            // delegates scrolling to MoveCursor by simulating an up or down arrow key
            MoveCursor(attr, key == PAGE_UP ? UP_ARROW : DOWN_ARROW); // if page up -> UP_ARROW else - > DOWN_ARROW
        break;
    case HOME_KEY: // moves cursorX to beggining of the sline
        attr->cursorX = 0;
        break;
    case END_KEY: // moves cursorX to end of the line
        attr->cursorX = attr->tRow[attr->cursorY + attr->rowOffset].rendSize;
        break;
    }

    return 1;
}

//-------------------------------------------------------------//
//---------------Moving the Cursor and Scrolling---------------//
//-------------------------------------------------------------//

/**************************************************************************************************
    Description:
    Two int variables, cursorX and cursorY, track cursor position within the TerminalAttr struct.
    Both initialize to 0. A switch-case detects key presses, updating cursorX or cursorY based on
    arrow or HOME key input. If cursor exceeds screen boundary, scrolling occurs.

    Horizontal movement beyond a line jumps to the next via MoveCursor with a DOWN_ARROW input,
    adjusting row lengths/offsets and resetting cursorX and colOffset if scrolling occurs.
    Similarly, moving before a line calls MoveCursor with UP_ARROW, placing the cursor at the end
    of the line in the row above as well as handling any necessary scrolling.

    Dependencies:
    - Scroll
**************************************************************************************************/
void MoveCursor(TerminalAttr *attr, int key)
{
    int txtLen;

    if (attr->cursorY < attr->tRowsTot)                                // checks if current row has text
        txtLen = attr->tRow[attr->cursorY + attr->rowOffset].rendSize; // calc size of curr row
    else                                                               // used for rows with no text (tilde rows) and is also a default size value for a file with no text
        txtLen = 0;

    switch (key)
    {
    case UP_ARROW:
        if (attr->cursorY == 0)     // if cursor is at topmost of screen
            Scroll(attr, UP_ARROW); // scroll up
        else
            attr->cursorY--;
        break;

    case DOWN_ARROW:
        if (attr->cursorY == attr->numRows - 1) // if cursor is at bottommost of screen
            Scroll(attr, DOWN_ARROW);           // scroll down
        else
            attr->cursorY++;
        break;

    case RIGHT_ARROW:
        if ((attr->cursorX < attr->numCols - 1) && (attr->cursorX < txtLen)) // if cursorX is less than screen or end of line
            attr->cursorX++;                                                 // move cursor right
        else if (attr->colOffset < attr->maxColOffset)
            Scroll(attr, RIGHT_ARROW); // scrolls right if less than maxColOffset
        else                           // means cursorX is on right side of last char
        {
            MoveCursor(attr, DOWN_ARROW); // recursive call to move cursorY down one line and update scroll/offset values
            attr->cursorX = 0;            // set cursorX to end of screen
            attr->colOffset = 0;          // set offset to max to ensure it reaches end of line
        }
        break;

    case LEFT_ARROW:
        if ((attr->cursorX == 0) && (attr->colOffset > 0)) // if cursorX is at leftmost of screen and if screen has scrolled right
            Scroll(attr, LEFT_ARROW);
        else if (attr->cursorX == 0)
        {
            MoveCursor(attr, UP_ARROW);           // recursive call to move cursorY up one line and update scroll/offset values
            attr->cursorX = attr->numCols - 1;    // set cursorX to end of screen
            attr->colOffset = attr->maxColOffset; // set offset to max to ensure it reaches end of line
        }
        else
            attr->cursorX--;
        break;
    }

    if (attr->cursorY < attr->tRowsTot)                                // checks if current row has text
        txtLen = attr->tRow[attr->cursorY + attr->rowOffset].rendSize; // calc size of curr row
    else                                                               // used for rows with no text (tilde rows) and is also a default size value for a file with no text
        txtLen = 0;

    attr->maxColOffset = txtLen - attr->numCols + 1; // calculate max col offset
    if (attr->maxColOffset < 0)                      // make sure its not a negative value
    {
        attr->maxColOffset = 0;
        if (attr->cursorX > txtLen)
            attr->cursorX = txtLen; // since row is smaller than screen width, set cursorX to end of text line
    }
    if (attr->colOffset > attr->maxColOffset) // make sure offset is not greater than max
        attr->colOffset = attr->maxColOffset;
}

/**************************************************************************************************
    Description:
    The Scroll function manages horizontal scrolling when the cursor exceeds the current row's
    screen length and vertical scrolling when the number of rows exceeds the screen length.
    MoveCursor calls Scroll to handle scrolling behavior.

    When scrolling horizontally, the screen scrolls until the last character is reached. Vertical
    scrolling continues until the last row is reached, handling scrolling in both directions until
    the first character or row is reached.

    Dependencies:
    None
**************************************************************************************************/
void Scroll(TerminalAttr *attr, int key)
{
    switch (key)
    {
    case UP_ARROW:
        if (attr->rowOffset > 0) // makes sure rowOffset doesn't go below min offset
            attr->rowOffset -= 1;
        break;
    case DOWN_ARROW:
        if (attr->rowOffset < attr->maxRowOffset) // makes sure rowOffset doesn't exceed the max offset
            attr->rowOffset += 1;
        break;
    case RIGHT_ARROW:
        // if (attr->colOffset < attr->maxColOffset) // makes sure colOffset doesn't exceed the max offset (redundant)
        attr->colOffset += 1;
        break;
    case LEFT_ARROW:
        // if (attr->colOffset > 0) // makes sure colOffset doesn't go below min offset (redundant)
        attr->colOffset -= 1;
        break;
    }
}

//--------------------------------------------------------//
//---------------Processing Text from Files---------------//
//--------------------------------------------------------//

/**************************************************************************************************
    Description:
    OpenFile takes the file name pointer as a parameter. It opens the file and copies each line
    without including '/r' and '/n' characters. The size is also updated accordingly. The string of
    each row or line is then put into AppendRow which handles storing the text for each row.

    Dependencies:
    - ErrorHandler
    - AppendRow
**************************************************************************************************/
void OpenFile(TerminalAttr *attr, char *fileName)
{
    // free(attr->fileName);              // frees any memory so we can use strdup
    attr->fileName = strdup(fileName); // copies fileName into attr struct's fileName string

    FILE *fp = fopen(fileName, "r");
    if (!fp)
        ErrorHandler("fopen"); // manages errors

    char *lineTxt = NULL;
    size_t capacity = 0;
    ssize_t lineSize;

    while ((lineSize = getline(&lineTxt, &capacity, fp)) != -1)
    { // skips through all newline and return char in the row

        while ((lineSize > 0) &&
               ((lineTxt[lineSize - 1] == '\n') || (lineTxt[lineSize - 1] == '\r')))
            lineSize--; // the size is updated and excludes '\n' & '\r' chars

        // the line is then copied without '\n' or '\r' chars
        AppendRow(attr, lineTxt, lineSize);
    }
    attr->maxRowOffset = attr->tRowsTot - attr->numRows; // define once the max rows that can be scrolled
    free(lineTxt);
    fclose(fp);
}

/**************************************************************************************************
    Description:
    This function copies text from the string given to it into a string within tRow (TerminalRow
    struct). tRow is an array where each index contains a string and the size of that string for a
    single row of text from the file. Before copying a row into the corresponding tRow, the amount
    of tRows is incremented by 1 for each new row copied. Tabs are accounted for in RenderRow.

    Dependencies:
    - RenderRow
**************************************************************************************************/
void AppendRow(TerminalAttr *attr, char *str, size_t rowSize)
{
    attr->tRowsTot++; // increment number of rows since a new row is needed
    attr->tRow = realloc(attr->tRow, sizeof(TerminalRow) * (attr->tRowsTot));

    int i = attr->tRowsTot - 1; // last index is one less than size

    attr->tRow[i].size = rowSize;
    attr->tRow[i].text = malloc(rowSize + 1); //+1 for null char
    memcpy(attr->tRow[i].text, str, rowSize); // copy string into allocated slot
    attr->tRow[i].text[rowSize] = '\0';       // manually terminate string

    attr->tRow[i].rendSize = 0; // initialize render string and its size
    attr->tRow[i].rendStr = NULL;

    RenderRow(&attr->tRow[i]); // send to RenderRow to account for tabs
}

/**************************************************************************************************
    Description:
    This function takes one tRow index as it's parameter. It first checks the string in tRow for
    tab characters. It then allocates memory for a new string, rendStr. If it found tab characters
    in text (tRow string), it allocates 7 additional spaces in memory for each tab characrer found.
    It then adds spaces for each tab in rendStr until it reaches a tab stop.

    Dependencies:
    None
**************************************************************************************************/
void RenderRow(TerminalRow *tRow)
{
    int numTabs = 0;
    int i;

    for (i = 0; i < tRow->size; i++)
        if (tRow->text[i] == '\t')
            numTabs++;

    free(tRow->rendStr); // make sure no memory is reserved for rendStr
    // each tab is a maximum of 8 characters, 1 character has already been accounted for each tab
    tRow->rendStr = malloc(tRow->size + 1 + numTabs * 7); // reserves the appropiate amount of memory

    int j = 0; // used to keep track of rendStr indices seperately of text indices

    for (i = 0; i < tRow->size; i++)
    {
        if (tRow->text[i] != '\t')
            tRow->rendStr[j++] = tRow->text[i]; // copy character from text string
        else
        {
            tRow->rendStr[j++] = ' ';     // each tab must increment by at least one space
            while (j % TAB_STOP != 0)     // checks for a tab stop
                tRow->rendStr[j++] = ' '; // keep adding tabs until we reach a tab stop
        }
    }

    tRow->rendStr[j] = '\0'; // manually terminate string
    tRow->rendSize = j;      // set rendStr size to number of characters copied
}

//-------------------------------------------------------//
//---------------Displaying Text on Screen---------------//
//-------------------------------------------------------//

/**************************************************************************************************
    Description:
    Taking a string, the string's length and an AppendBuffer as its parameters, this function
    reallocates memory for a pointer that points to the buffer string within the abuff struct. The
    buffer string's job is to store all the text for a file. So when a new line of text is sent to
    AppendString, AppendString must make sure there is enough memory in the buffer string to append
    the new string on top of the text that is already inside the buffer.

    Dependencies:
    None
**************************************************************************************************/
void AppendString(AppendBuffer *abuff, const char *str, int length)
{
    // creates new buffer pointer with appropiate memory size
    char *newBuff = realloc(abuff->buff, abuff->length + length); // length of new string is accounted for

    if (newBuff == NULL) // in case of failure of trying to allocate memory
        return;

    memcpy(&newBuff[abuff->length], str, length); // appends new string to end of old buffer
    abuff->buff = newBuff;                        // sets pointer to the new buffer pointer that has additional memory
    abuff->length += length;                      // adjusts the lenght of the buffer string
}

/**************************************************************************************************
    Description:
    Deallocates memory from the append buffer.

    Dependencies:
    None
**************************************************************************************************/
void FreeAbuff(AppendBuffer *abuff)
{
    free(abuff->buff);
}

/**************************************************************************************************
    Description:
    This function takes in the append buffer as a parameter. If no file is loaded, it prints a
    welcome message. If a file was opened, WriteRows prints as many rows that fit onto the screen.
    It does this by appending rows of the file (using the rendStr array) into the append buffer.
    If vertical scrolling has occured, we start copying rows from the rendStr array with an index
    offset by the amount of vertical scrolling that occured. If horizontal scrolling has occured,
    we copy the text from from each row with the index of the string offset by the amount of
    horizontal scrolling that occured. RefreshScreen handles printing to the terminal.

    Dependencies:
    - AppendString
**************************************************************************************************/
void WriteRows(TerminalAttr *attr, AppendBuffer *abuff)
{
    int rows = attr->numRows;
    int columns = attr->numCols;
    int scrollRows = attr->rowOffset;
    int scrollCols = attr->colOffset;
    int fileRows = attr->tRowsTot;
    char welcome[40];

    int length = snprintf(welcome, sizeof(welcome), "Helio Editor -- version %s", HELIO_VERSION);
    if (length > columns)
        length = columns;                     // makes sure message fits screen
    int padding = (columns - length - 1) / 2; // minus 1 to account for the tilde

    for (int i = 0; i < rows; i++)
    { // only prints as many rows that fit on screen

        // makes sure all rows of text are written (matters only when text file is smaller than screen)
        if (i < fileRows)
        {
            int txtLen = attr->tRow[i + scrollRows].rendSize - scrollCols; // accounts for scrolled rows
            if (txtLen > columns)                                          // if txtLen is greater than window width
                txtLen = columns;                                          // makes txtLen same legnth of window width
            if (txtLen > 0)                                                // doesn't let string be printed if no there is no text
                AppendString(abuff, &attr->tRow[i + scrollRows].rendStr[scrollCols], txtLen);
        }
        else // inserts padding and welcome message
        {
            AppendString(abuff, "~", 1); // prints tilde on left most column of screen
            // prints welcome message a fourth down the screen
            if ((i == rows / 4) && (fileRows == 0)) // only prints wlc msg if no file loaded
            {
                for (int j = 0; j < padding; j++) // centers message by adding spaces
                    AppendString(abuff, " ", 1);
                AppendString(abuff, welcome, length);
            }
        }

        AppendString(abuff, "\x1b[K", 3); // command that clears everything right of the cursor
        AppendString(abuff, "\r\n", 2);   // adds newline for every line
    }
}

/**************************************************************************************************
    Description:
    Prints the statusBar (last bar on screen) to display information about the file (file name and
    row number). If no file is opened and therefore no file name is given, the default is set to
    "[No Name]" in InitTerminalAttr. Up to 20 characters of the file name will be shown.

    For the m command, refer to selecting graphic rendition in the VT100 user guide.

    Dependencies:
    - AppendString
**************************************************************************************************/
void WriteStatusBar(TerminalAttr *attr, AppendBuffer *abuff)
{
    AppendString(abuff, "\x1b[7m", 4);   // command to display inverted colors (switches black with white)
    char statusBar1[80], statusBar2[80]; // left side and right side string of the status bar respectively
    // sets length as well as prints the file name and the number of rows in the file
    int length1 = snprintf(statusBar1, sizeof(statusBar1), "%.20s - %d Lines", attr->fileName, attr->tRowsTot);
    // sets length as well as prints the current row the cursor is on as well as the number of rows in the file
    int length2 = snprintf(statusBar2, sizeof(statusBar2), "%d/%d", attr->cursorY + attr->rowOffset + 1, attr->tRowsTot);

    if (length1 > attr->numCols)
        length1 = attr->numCols; // makes sure length of statusBar doesn't exceed screen width
    AppendString(abuff, statusBar1, length1);

    // makes sure it prints spaces until there is exactly enough space left for statusBar2
    for (int i = length1 + length2; i < attr->numCols; i++)
        AppendString(abuff, " ", 1); // adds spaces

    AppendString(abuff, statusBar2, length2); // prints right side of statusBar (statusBar2)

    AppendString(abuff, "\x1b[m", 3); // sets display colors back to default
    AppendString(abuff, "\r\n", 2);   // makes room for status message to be displayed
}

/**************************************************************************************************
    Description:
    This function takes in a variable amount of arguments (like printf), so it is a variadic
    function. To use such functions, we must use a va_list type. We then input the last argument
    before "..." into va_start so the next arguments' addresses are known. Then we call vsnprintf to
    take care of reading and formatting the string. Lastly we call va_end.

    Dependencies:
    None
**************************************************************************************************/
void SetStatusMessage(TerminalAttr *attr, const char *frmt, ...)
{
    va_list arguments;
    va_start(arguments, frmt);                                            // call va_start to provide address of arguments
    vsnprintf(attr->statusMsg, sizeof(attr->statusMsg), frmt, arguments); // reads and formats the string
    va_end(arguments);

    attr->statusMsgTime = time(NULL);
}

/**************************************************************************************************
    Description:
    Writes the status message that was set in SetStatusMessage to the append buffer, abuff. It also
    makes sure that it only display status messages that are less than 5 seconds after a keypress.

    Dependencies:
    - AppendString
**************************************************************************************************/
void WriteStatusMessage(TerminalAttr *attr, AppendBuffer *abuff)
{
    AppendString(abuff, "\x1b[k", 3); // clears the current row
    int length = strlen(attr->statusMsg);

    if (length > attr->numCols) // makes sure string length doesn't exceed screen width
        length = attr->numCols;

    if (length && (time(NULL) - attr->statusMsgTime < 5)) // checks if there is a message and
        AppendString(abuff, attr->statusMsg, length);     // if it happened less than 5 seconds ago
}

/**************************************************************************************************
    Description:
    First the append buffer is declared. The first commands appended into the append buffer hide
    and reposition the cursor to the top left of the screen. After that, WriteRows is called to
    append all the rows into the append buffer. Lastly, it appends commands that reposition the
    cursor back to its original location as well as show the cursor. Then all the buffer is written
    at once to avoid flickering (including the commmands).

    Dependencies:
    - AppendString
    - WriteRows
    - FreeAbuff
**************************************************************************************************/
void RefreshScreen(TerminalAttr *attr)
{
    AppendBuffer abuff = ABUFF_INIT;
    char buff[32];

    // refer to VT100 user guide for descriptions of commands (\x1b = 27 in decimal)
    AppendString(&abuff, "\x1b[?25l", 6); // command to hide the cursor
    AppendString(&abuff, "\x1b[H", 3);    // command to reposition cursor to top-left of screen

    WriteRows(attr, &abuff);      // appends rows from file into the append buffer that are supposed to be visible
    WriteStatusBar(attr, &abuff); // adds status bar to the bottom of the display
    WriteStatusMessage(attr, &abuff); // adds a status message below the status bar (i.e., bottommost line)

    // moves cursor to specified cursorY and cursorX position (+1 to convert 0-indexed to 1-indexed)
    snprintf(buff, sizeof(buff), "\x1b[%d;%dH", attr->cursorY + 1, attr->cursorX + 1);
    AppendString(&abuff, buff, strlen(buff));

    AppendString(&abuff, "\x1b[?25h", 6); // command to show the cursor

    write(STDOUT_FILENO, abuff.buff, abuff.length); // writes the whole buffer at once to avoid flickering
    FreeAbuff(&abuff);
}

//-----------------------------------------------//
//---------------Utility Functions---------------//
//-----------------------------------------------//

/**************************************************************************************************
    Description:
    Displays an error description (given as a parameter) and forcefully exits program.

    Dependencies:
    None
**************************************************************************************************/
void ErrorHandler(const char *str)
{
    write(STDOUT_FILENO, "\x1b[2J", 4); // refreshes screen
    write(STDOUT_FILENO, "\x1b[H", 3);  // repositions cursor to top-left of screen

    perror(str); // prints out error description
    exit(1);
}

/**************************************************************************************************
    Description:
    Turns on raw mode; a terminal mode in which input characters are read immediately and output
    characters are sent directly to the screen. It does so by turning on and off certain flags.
    The termios struct given as a prameter is not modified since we passed it by value.

    Dependencies:
    None
**************************************************************************************************/
void RawModeOn(struct termios rawState)
{
    // turns off canonical flag, echo flag, ctrl c/z (ISIG) cmnds and ctrl v cmnd (IEXTEN)
    // BRKINT, INPCK and ISTRIP flags were added for older systems as a precaution
    rawState.c_lflag &= ~(IEXTEN | ISIG | ICANON | ECHO | BRKINT | INPCK | ISTRIP);

    // turns off ctrl s/q commands (IXON) and carriage return feature (ICRNL)
    rawState.c_iflag &= ~(IXON | ICRNL);

    // turn off output processing flag
    rawState.c_oflag &= ~(OPOST);

    // adding this in case (probably won't matter for newer systems)
    rawState.c_cflag |= (CS8);

    // sets minimum amount of bytes read function must read before it returns
    rawState.c_cc[VMIN] = 0;
    // sets max time to wait until read can return (100 ms in this case)
    rawState.c_cc[VTIME] = 1;

    // sets attributes to raw state; TCSAFLUSH means wait for output to finish
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &rawState);
}

/**************************************************************************************************
    Description:
    Turns onff rawMode by setting all the flags to the original state taht they were before being
    modified. We are able to do this by saving a copy of the original termios struct as a pointer
    (through the InitTerminalAttr function).

    Dependencies:
    - ErrorHandler
**************************************************************************************************/
void RawModeOff(struct termios originalState)
{
    // sets attributes back to the orignal terminal state
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &originalState) == -1)
        ErrorHandler("tcsetattr"); // gives error description
}

/**************************************************************************************************
    Description:
    Gets the window width and length (row and column sizes) by using the winsize struct. To access
    this struct, we use the ioctl function (both winsize and iocatl are located in <sys/ioctl.h>).

    Dependencies:
    None
**************************************************************************************************/
int FetchWindowSize(int *numRows, int *numCols)
{
    struct winsize size;
    // ioctl stands for input output control; it is able to fetch the window size on most systems
    if ((ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) == -1) || size.ws_col == 0) // checks for erroneous behaviour
        return -1;                                                           // reports failure to get sizes
    else
    {
        *numRows = size.ws_row - 2; // subtract 2 to account for status bar and status message
        *numCols = size.ws_col;
        return 0; // reports success in getting sizes
    }
}

/**************************************************************************************************
    Description:
    Initiliazes all variables that are a part of the attr (TerminalAttr) struct. attr is defined in
    main and is passed by reference to this function. It also initializes the orignalState termios
    struct and calls FetchWindowSize.

    Dependencies:
    - ErrorHandler
**************************************************************************************************/
void InitTerminalAttr(TerminalAttr *attr)
{
    attr->cursorX = 0; // set x and y cursor positions to top left of screen
    attr->cursorY = 0;
    attr->rowOffset = 0;
    attr->colOffset = 0;
    attr->maxRowOffset = 0;
    attr->maxColOffset = 0;
    attr->tRowsTot = 0;
    attr->tRow = NULL;
    attr->statusMsg[0] = '\0';
    attr->statusMsgTime = 0;
    attr->fileName = "[fileName]"; // in case no file is opened, set default name to no name

    // stores original state attributes; STDIN_FILENO means standard input stream
    if (tcgetattr(STDIN_FILENO, &(attr->originalState)) == -1)
        ErrorHandler("tcgetattr"); // gives error description
    // provides pointers of row member and column member to function FetchWindowSize
    if (FetchWindowSize(&(attr->numRows), &(attr->numCols)) == -1)
        ErrorHandler("fetch_window_size"); // gives error description
}

//====================main Program====================//

int main(int argc, char *argv[])
{
    TerminalAttr attr;

    InitTerminalAttr(&attr); // initialzes the TerminalAttr struct
    RawModeOn(attr.originalState);
    if (argc >= 2)
    {
        OpenFile(&attr, argv[1]);
    }

    SetStatusMessage(&attr, "HELP: Press CTRL-Q to quit"); // first status message when booting up program

    while (ProcessKeypress(&attr)) // ProcessKeypress returns either 0 or 1
    {
        // providing pointers of row member and column member to function FetchWindowSize
        if (FetchWindowSize(&(attr.numRows), &(attr.numCols)) == -1)
            ErrorHandler("fetch_window_size"); // gives error description

        RefreshScreen(&attr); // screen is only refreshed after every keypress
    }

    RawModeOff(attr.originalState);
    return 0;
}