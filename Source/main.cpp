///////////////////////////////////////////////////////////////////////////////
//
// SPREADSHEET
// ============
//
// "Crafted" from ssc and PhotoDataBase
//
///////////////////////////////////////////////////////////////////////////////

const char Revision [] = "1.42";
char *Title;

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <math.h>

#include "Widgets.hpp"
#include "WidgetsGrid.hpp"
#include "MessageBox.hpp"
#include "MessageResponseBox.hpp"
#include "FileSelect.hpp"

#include "Complex.c"

/*void DebugAdd (const char* Message)
  {
    static int Time0 = ClockMS ();
    static int Time1 = Time0;
    char *s = (char *) malloc (StrLen (Message) + 32);
    char *ps = s;
    int t;
    //
    t = ClockMS ();
    IntToStrDecimals (&ps, t - Time0, 3);
    StrCat (&ps, " [");
    IntToStrDecimals (&ps, t - Time1, 3);
    Time1 = t;
    StrCat (&ps, "] ");
    StrCat (&ps, Message);
    *ps = 0;
    //
    puts (s);
    //Log ("Log", s);
    free (s);
  }*/


//////////////////////////////////////////////////////////////////////////
//
// GLOBALS

class _EditContents: public _EditCommon
  {
    public:
      _MenuPopup *pmEdit;
      _EditContents (_Container *Parent, _Rect Rect, char *Text = NULL, _Action Action_ = NULL, bool Multi = false);
  };

void ActionEditContentsMenu (_MenuPopup *Menu)
  {
    _EditContents *Edit;
    char *Snippet;
    //
    Edit = (_EditContents *) Menu->Parent;
    switch (Menu->Selected)
      {
        case 0: Edit->Clipboard (coCut);   break;
        case 1: Edit->Clipboard (coCopy);  break;
        case 2: Edit->Clipboard (coPaste); break;
        case 3: Edit->TextSet (NULL);      break;
        default:   // Insert Function
          {
            Snippet = StrGetItem (Menu->TextGet (), Menu->Selected);
            Edit->Insert (Edit->IndexLine + Edit->Posn.x, StrLen (Snippet), Snippet);
            Edit->Posn.x += StrLen (Snippet) - 1;
            Edit->Invalidate ();
            free (Snippet);
          }
      }
    Edit->Form->EventLock = Edit;   // Restore lock (taken by popup
  }

_EditContents::_EditContents (_Container *Parent, _Rect Rect_, char *Text_, _Action Action, bool Multi)
  : _EditCommon (Parent, Rect_, Text_, Action, Multi)
  {
    pmEdit = new _MenuPopup (this, {0, 0, 0, 0},
      "Cut\tCopy\tPaste\t_Clear\t"
      "Sqrt ()\tSqr ()\tLn ()\tLog ()\tExp ()\tAbs ()\tIF ()\tRandom ()\t"
      "!\tC (,)\tP (,)\tB (,,)\tB (,,,)\t"
      "GCD (,)\tLCM (,)\t"
      "Pi\tSin ()\tCos ()\tTan ()\tASin ()\tACos ()\tATan\t"
      "Count (,)\tSum (,)\tSumSquare (,)\tAvg (,)\tStDev (,)\tStDevN (,)"
      "Date (,,)\tTime (,,)\tAddMonth (,)\t"
      "Int ()\tFrac ()\tReal ()\tImag ()\tMod ()\tArg ()\tComplex (,)\tComplexPolar (,)\t"
      "Solve (,)\tSolve (,,)\tIntegrate (,,,)\tDifferentiate (,)\tRational (,)\t"
      "Mortgage (,,)",
      (_Action) ActionEditContentsMenu);
  }

class _FormMain: public _Form
  {
    public:
      _Container *Toolbar;
        //_Button *bUp, *bHome;
        _Button *bCut, *bCopy, *bPaste;
        _Button *bDelete;
        _Button *bSort, *bSummary;
        _Button *bLoad, *bSave, *bGraph, *bHelp;
        _ButtonArrow *bPrev, *bNext;
        _EditPopup *eTarget;
        _ButtonArrow *bLarger, *bSmaller;
        _Edit *Target;
      _Label *lCellAddress, *lContents;
      _EditContents *eContent;
      _Container *cFormat;   // this will be filled with scripted controls
      _Tabs *tPages;
      _GridView *gSheet;
      _MenuPopup *mpSheet;
      //
      _Bitmap *Icon;
      //
      _FormMain (char *Title);
     ~_FormMain ();
  };

_FormMain *fMain;


bool Debug = false;
bool Quit = false;
bool Status = false;

const char *HelpMsg = NULL;

const char HelpMsgErrorRead [] = "\ab\a1** Read Error\ab\a0";
const char HelpMsgErrorWrite [] = "\ab\a1** Write Error\ab\a0";

bool CellsChanged;


////////////////////////////////////////////////////////////////////////////
//
// DYNAMIC CELL ARRAY

typedef struct
  {
    char *Content;
    char *Format;
    bool Exp;
    bool Error;
    Complex Value;
  } _Cell;

typedef struct
  {
    int SizeX;
    int SizeY;
    int XSel;
    int YSel;
    int XSelEnd;
    int YSelEnd;
    int *WidthColumns;   // dynamic array: Use Array[Get/Set]Int
    _Point PageLock;   // Top portion of page fixed in place
    _Cell *Cells;
  } _Page;

#define NumPages 26

_Page Pages [NumPages] = {0};

int Page = 0;
_Page *pPage = &Pages [0];

bool Radians;

void SetPage (int NewPage)
  {
    if (NewPage < SIZEARRAY (Pages))
      {
        Page = NewPage;
        pPage = &Pages [Page];
      }
  }

int CellsSizeCalc (int Index, int Size)
  {
    while (Index >= Size)
      if (Size == 0)
        Size = 1;
      else
        Size <<= 1;
    return Size;
  }

void CellsCheckSize (_Page *pPage, int ix, int iy)
  {
    int sx, sy;
    _Cell *CellsNew;
    int x, y, s;
    //
    sx = CellsSizeCalc (ix, pPage->SizeX);
    sy = CellsSizeCalc (iy, pPage->SizeY);
    if ((sx > pPage->SizeX) || (sy > pPage->SizeY))
      {
        s = sx * sy * sizeof (_Cell);
        CellsNew = (_Cell *) malloc (s);
        memset (CellsNew, 0, s);
        if (pPage->Cells)
          for (y = 0; y < pPage->SizeY; y++)
            for (x = 0; x < pPage->SizeX; x++)
              CellsNew [y * sx + x] = pPage->Cells [y * pPage->SizeX + x];
        pPage->SizeX = sx;
        pPage->SizeY = sy;
        free (pPage->Cells);
        pPage->Cells = CellsNew;
      }
  }

_Cell *CellFind (int Page, int x, int y, bool Force)
  {
    _Page *pPage;
    //
    if (Page >= NumPages)
      return NULL;
    pPage = &Pages [Page];
    if (!Force)
      {
        if ((x < 0) || (x >= pPage->SizeX))
          return NULL;
        if ((y < 0) || (y >= pPage->SizeY))
          return NULL;
      }
    else
      {
        CellsCheckSize (pPage, x, y);
        if (x > pPage->XSelEnd)
          pPage->XSelEnd = x;
        if (y > pPage->YSelEnd)
          pPage->YSelEnd = y;
      }
    return (&pPage->Cells [y * pPage->SizeX + x]);
  }

bool CellSwap (int Page, int x1, int y1, int x2, int y2)
  {
    _Page *pPage;
    _Cell temp;
    //
    if (Page >= NumPages)
      return false;
    pPage = &Pages [Page];
    if ((x1 < 0) || (x1 >= pPage->SizeX) || (x2 < 0) || (x2 >= pPage->SizeX))
      return false;
    if ((y1 < 0) || (y1 >= pPage->SizeY) || (y2 < 0) || (y2 >= pPage->SizeY))
      return false;
    memmove (&temp, &pPage->Cells [y1 * pPage->SizeX + x1], sizeof (_Cell));
    memmove (&pPage->Cells [y1 * pPage->SizeX + x1], &pPage->Cells [y2 * pPage->SizeX + x2], sizeof (_Cell));
    memmove (&pPage->Cells [y2 * pPage->SizeX + x2], &temp, sizeof (_Cell));
    return true;
  }

void CellsFree (void)
  {
    int i;
    //
    for (i = 0; i < pPage->SizeX * pPage->SizeY; i++)
      {
        free (pPage->Cells [i].Content);
        free (pPage->Cells [i].Format);
      }
    free (pPage->Cells);
    pPage->Cells = NULL;
    pPage->SizeX = 0;
    pPage->SizeY = 0;
    pPage->XSelEnd = 0;
    pPage->YSelEnd = 0;
  }

void PagesFree (void)
  {
    SetPage (0);
    while (true)
      {
        CellsFree ();
        free (pPage->WidthColumns);
        pPage->WidthColumns = NULL;
        if (Page + 1 >= NumPages)
          break;
        SetPage (Page + 1);
      }
    SetPage (0);
  }


#include "GetExpression.cpp"


////////////////////////////////////////////////////////////////////////////
//
// PROCESS CELL DATA

bool Recalculate = false;
bool Redraw = true;
unsigned int RandomSeed;

bool IsExpression (char **Content)
  {
    bool Res;
    char *c;
    //
    Res = false;
    c = *Content;
    StepSpace (&c);
    if (*c++ == '=')
      {
        Res = true;
        *Content = c;
      }
    return Res;
  }

#define AssumeFormat

char* StrPosWild (char *St, char *Target)
  {
    char *Res;
    //
    StrPosFlags = spfWilds;
    Res = StrPos (St, Target);
    StrPosFlags = 0x00;
    return Res;
  }

bool StrNumberPercent (char *St)
  {
    while (true)
      {
        if (!IsDigit (*St) && *St != '.' && *St != '-')
          return (St [0] == '%' && St [1] == '\0');
        St++;
      }
  }

bool StrNumberWithCommas (char *St)
  {
    int nDig, nComma, nPoint, nCrap;
    //
    nDig = nComma = nPoint = nCrap = 0;
    while (*St)
      {
        if (IsDigit (*St))
          nDig++;
        else if (*St == ',')
          nComma++;
        else if (*St == '.')
          nPoint++;
        else
          nCrap++;
        St++;
      }
    return (nDig > 3 && nComma > 0 && nPoint <= 1 && nCrap == 0);
  }

void ApplyFormatCell (_Cell *Cell, char FormatCode, bool Add);

void ProcessCell (_Cell *Cell)
  {
    bool CellsChanged_;
    Complex ValueOld;
    bool ErrorOld;
    char *c, *c_;
    int n;
    //
    if (Cell)
      {
        ValueOld = Cell->Value;
        ErrorOld = Cell->Error;
        CellsChanged_ = CellsChanged;
        Cell->Exp = false;
        ComplexSet (0.0, 0.0, &Cell->Value);
        Cell->Error = false;
        if (Cell->Content && Cell->Content [0])
          {
            c = Cell->Content;
            c_ = NULL;
            if (IsExpression (&c))
              Cell->Exp = true;
            // Evaluate
            StrError = false;
            StepSpace (&c);
            n = StrGetDate (&c, &Cell->Value.Real);   // is this a date
            if (n)   // date
              {
                #ifdef AssumeFormat
                ApplyFormatCell (Cell, 'd', true);
                ApplyFormatCell (Cell, n + '0', true);
                #endif
              }
            else
              {
                if (StrPosWild (c, "$#") == c) // Currency?
                  {
                    #ifdef AssumeFormat
                    ApplyFormatCell (Cell, 'c', true);
                    #endif
                    c++;
                  }
                else if (StrPos (c, "0x") == c) // Hex?
                  {
                    #ifdef AssumeFormat
                    ApplyFormatCell (Cell, 'h', true);
                    #endif
                  }
                else if (StrNumberPercent (c))
                  {
                    c = c_ = StrReplace (c, "%", "/100");
                    #ifdef AssumeFormat
                    ApplyFormatCell (Cell, 'N', true);
                    #endif
                  }
                else if (StrNumberWithCommas (c))
                  c = c_ = StrReplace (c, ",", NULL);
                Radians = StrPos (Cell->Format, 'X') != NULL;   // set Radians as per Format
                GetExpression (&c, &Cell->Value, NULL);
              }
            // Did we reach the end of the expression
            if (*c != 0)
              StrError = true;
            Cell->Error = StrError;
            if (Cell->Error != ErrorOld)
              Recalculate = true;
            else
              if (!StrError)
                if (IsDifferent (Cell->Value.Real, ValueOld.Real) || IsDifferent (Cell->Value.Imaginary, ValueOld.Imaginary))
                  Recalculate = true;
            free (c_);
          }
        CellsChanged = CellsChanged_;
      }
  }

void RecalculateCells (void)
  {
    int x, y;
    int x_, y_;
    int Count;
    bool RecalculateOld;
    _Cell *Cell;
    //
    Count = 0;
    while (Recalculate)
      {
        Recalculate = false;
        srand (RandomSeed ^ (Page * Page));   // Initialize random number generator
        x = y = 0;
        while (true)
          {
            if (x >= pPage->SizeX)
              {
                x = 0;
                y ++;
              }
            if (y >= pPage->SizeY)
              break;
            RecalculateOld = Recalculate;
            Cell = CellFind (Page, x, y, false);
            ProcessCell (Cell);
            if (Recalculate && !RecalculateOld)
              {
                Redraw = true;
                x_ = x;
                y_ = y;
              }
            x++;
          }
        if (++Count >= 1000)
          {
            Beep ();
            HelpMsg = "\ab\a1** Circular Reference\ab\a0";
            pPage->XSel = x_;
            pPage->YSel = y_;
            Recalculate= false;   // stop the endless cycle
            break;
          }
      }
  }

void WriteToCellContent (char *Content, int x, int y)
  {
    _Cell *Cell;
    //
    if (Content && (Content [0]))
      {
        Cell = CellFind (Page, x, y, true);
        if (StrCmp (Cell->Content, Content))   // Cell changed
          {
            Recalculate = true;
            StrAssignCopy (&Cell->Content, Content);   // Make our own copy
            ProcessCell (Cell);
          }
      }
    else
      {
        Cell = CellFind (Page, x, y, false);
        if (Cell && Cell->Content)
          {
            Recalculate = true;
            StrAssign (&Cell->Content, NULL);
            ProcessCell (Cell);
          }
      }
    CellsChanged = true;
  }

void WriteToCellFormat (char *Format, int x, int y)
  {
    _Cell *Cell;
    //
    if (Format && (Format [0]))
      {
        Cell = CellFind (Page, x, y, true);
        StrAssignCopy (&Cell->Format, Format);
      }
    else
      {
        Cell = CellFind (Page, x, y, false);
        if (Cell)
          StrAssign (&Cell->Format, NULL);
      }
    CellsChanged = true;
  }

void WriteToCell (char *Content, char *Format, int x, int y)
  {
    WriteToCellContent (Content, x, y);
    WriteToCellFormat (Format, x, y);
  }

void WriteToCellContentFormat (char *ContentFormat, int x, int y)
  {
    char *pc;
    //
    if ((ContentFormat == NULL) || (*ContentFormat == 0))
      WriteToCell (NULL, NULL, x, y);
    else
      {
        StrTrim (ContentFormat);
        pc = StrPos (ContentFormat, '~');
        if (!pc)
          pc = StrPos (ContentFormat, 0xFF);
        if (!pc)   // no format data (we tried)
          WriteToCell (ContentFormat, NULL, x, y);   // so just write content
        else
          {
            *pc++ = 0;   // split string and step past separator
            StrTrim (ContentFormat);
            WriteToCell (ContentFormat, pc, x, y);
          }
      }
  }


///////////////////////////////////////////////////////////////////////////////////////////////////
//
// OTHER MODULES

#include "Formatting.cpp"
#include "Clipboard.cpp"
#include "FormGraph.cpp"


///////////////////////////////////////////////////////////////////////////////////////////////////
//
// SAVE / LOAD

const char ff_ = '\f'; //0x0C

char *FileName = NULL;

bool SaveSheets (void) //(char *Filename)
  {
    bool OK;
    int File;
    int p;
    char *Buff1, *Buff2;
    unsigned int l1, l2;
    //
    OK = false;
    FileBackup (FileName);
    File = FileOpen (FileName, foWrite);
    if (File > 0)
      {
        OK = true;
        p = Page;
        SetPage (0);
        while (true)
          {
            Buff1 = CellsToText (0, 0, pPage->XSelEnd, pPage->YSelEnd, &l1, mSS);
            if (l1)
              {
                Buff2 = PageParametersToText (&l2);
                if (!FileWrite (File, (byte *) Buff2, l2))
                  OK = false;
                free (Buff2);
                if (!FileWrite (File, (byte *) Buff1, l1))
                  OK = false;
                if (!FileWrite (File, (byte *) &ff_, sizeof (ff_)))
                  OK = false;
              }
            free (Buff1);
            if (Page + 1 >= NumPages)
              break;
            SetPage (Page + 1);
          }
        FileClose (File);
        SetPage (p);
      }
    if (OK)
      CellsChanged = false;
    else
    if (!OK)
      {
        Beep ();
        HelpMsg = (char *) HelpMsgErrorWrite;
      }
    Redraw = true;
    return OK;
  }

bool LoadSheets (void) //(char *Filename)
  {
    int File;
    int Size;
    char *Buffer;
    bool OK;
    bool Comma;
    //
    OK = false;
    StrAddExtension (&FileName, ".ss");
    Comma = StrCmp (StrFindFileExtension (FileName), ".csv") == 0;
    File = FileOpen (FileName, foRead);
    if (File > 0)
      {
        Size = FileSize (File);
        Buffer = (char *) malloc (Size + 1);
        if (Buffer)
          {
            if (FileRead (File, (byte *) Buffer, Size) == Size)
              {
                Buffer [Size] = 0;   // Terminate block
                PagesFree ();
                TextToCells (0, 0, Buffer, Comma);
                OK = true;
                CellsChanged = false;
                Recalculate = true;
              }
            free (Buffer);
          }
        FileClose (File);
      }
    if (!OK)
      {
        Beep ();
        HelpMsg = (char *) HelpMsgErrorRead;
      }
    CellsChanged = false;
    return OK;
  }


//////////////////////////////////////////////////////////////////////////
//
// MAIN FORM

#define InputMax 100

int FontSize = 12;
char Input [InputMax + 1], *pInput = NULL, *pInputCoord = NULL;
_Point InputPos;

void StrAppend (char **St, char Ch)
  {
    char *Res, *r;
    int l;
    //
    l = StrLen (*St);
    r = Res = (char *) malloc (l + 2);
    StrCat (&r, *St);
    StrCat (&r, Ch);
    *r = 0;
    StrAssign (St, Res);
  }

void GridGetSelected (_Point *s0, _Point *s1)
  {
    s0->x = Min (fMain->gSheet->Selected0.x, fMain->gSheet->Selected1.x) - 1;
    s0->y = Min (fMain->gSheet->Selected0.y, fMain->gSheet->Selected1.y) - 1;
    s1->x = Max (fMain->gSheet->Selected0.x, fMain->gSheet->Selected1.x) - 1;
    s1->y = Max (fMain->gSheet->Selected0.y, fMain->gSheet->Selected1.y) - 1;
  }

void BlockReadFormat (char *Format)
  {
    int x, y;
    _Point s0, s1;
    _Cell *Cell;
    char *cf;
    char *f;   // List of all formats used in the Selection
    //
    f = Format;
    *f = 0;
    GridGetSelected (&s0, &s1);
    for (y = s0.y; y <= s1.y; y++)
      for (x = s0.x; x <= s1.x; x++)
        {
          Cell = CellFind (Page, x, y, false);
          if (Cell)
            if (Cell->Format)
              {
                cf = Cell->Format;
                while (*cf)
                  {
                    if (!StrPos (Format, *cf))
                      {
                        *f++ = *cf;
                        *f = 0;
                      }
                    cf++;
                  }
              }
        }
  }

void ApplyFormatCell (_Cell *Cell, char FormatCode, bool Add)
  {
    char *f;
    //
    if (Cell)
      {
        f = NULL;
        if (Cell->Format)
          if (IsDigit (FormatCode))
            {
              StrPosFlags = spfWilds;
              f = StrPos (Cell->Format, '#');
              StrPosFlags = 0x00;
            }
          else
            f = StrPos (Cell->Format, FormatCode);
        if (Add)   // Adding
          if (f)   // already there but might be wrong digit so overwrite
            *f = FormatCode;
          else   // not there so add it
            StrAppend (&Cell->Format, FormatCode);
        if (!Add && f)   // Removing and it's there
          StrDelete (f, 1);
        CellsChanged = true;
      }
  }

void ApplyFormatBlock (char FormatCode, bool Add)
  {
    _Point s0, s1;
    int x, y;
    //
    GridGetSelected (&s0, &s1);
    for (y = s0.y; y <= s1.y; y++)
      for (x = s0.x; x <= s1.x; x++)
        ApplyFormatCell (CellFind (Page, x, y, false), FormatCode, Add);
    Redraw = true;
  }

void ActionCellSelect (_GridView *Grid, int x, int y)
  {
    char Format [64];
    _Button *btn;
    //
    HelpMsg = NULL;   // Clear errors off the Status Line
    // Extend Sheet as required
    if (Grid)
      {
        if (x + 1 == Grid->Columns)
          {
            Grid->Columns++;
            Grid->Invalidate ();
          }
        if (y + 1 == Grid->Rows)
          {
            Grid->Rows++;
            Grid->Invalidate ();
          }
      }
    // Add Coords to Input
    if (pInput && pInputCoord)
      {
        pInput = pInputCoord;
        if (IsAlpha (pInput [-1]))
          StrCat (&pInput, ' ');
        StrColumn (&pInput, x - 1);
        StrRow (&pInput, y - 1);
        //StrCat (&pInput, ' ');
        *pInput = 0;
      }
    Status = true;
    // Update Format buttons
    BlockReadFormat (Format);
    btn = (_Button *) fMain->cFormat->Children;
    while (btn)
      {
        btn->Down = (StrPos (Format, (char) btn->DataInt) != NULL);   // This button has been chosen
        btn->Invalidate ();
        btn = (_Button *) btn->Next;
      }
  }

void WriteToPages (void)
  {
    int x;
    //
    pPage->XSel = fMain->gSheet->Selected0.x - 1;
    pPage->YSel = fMain->gSheet->Selected0.y - 1;
    pPage->PageLock = {fMain->gSheet->Locked.x - 1, fMain->gSheet->Locked.y - 1};
    for (x = 0; x <= pPage->XSelEnd; x++)
      ArraySetInt (&pPage->WidthColumns, x, fMain->gSheet->CellWidthGet (x + 1));
  }

void ReadFromPages (void)
  {
    int x;
    //
    free (fMain->gSheet->CellHeights);
    fMain->gSheet->CellHeights = NULL;
    fMain->gSheet->Columns = Max (8, pPage->XSelEnd + 2);
    fMain->gSheet->Rows = Max (16, pPage->YSelEnd + 2);
    fMain->gSheet->Selected0.x = pPage->XSel + 1;
    fMain->gSheet->Selected0.y = pPage->YSel + 1;
    fMain->gSheet->Selected1.x = pPage->XSel + 1;
    fMain->gSheet->Selected1.y = pPage->YSel + 1;
    for (x = 0; x <= pPage->XSelEnd; x++)
      fMain->gSheet->CellWidthSet (x + 1, ArrayGetInt (&pPage->WidthColumns, x, fMain->gSheet->CellSizeDefault.x));
    fMain->gSheet->Locked = {pPage->PageLock.x + 1, pPage->PageLock.y + 1};
    fMain->gSheet->FocusSet ();
    fMain->gSheet->ScrollToSelected ();
  }

void ActionPageSelect (_Tabs *Tabs)
  {
    if (Tabs)
      {
        WriteToPages ();
        SetPage (Tabs->Selected);
      }
    ReadFromPages ();
    ActionCellSelect ((_GridView *) NULL, fMain->gSheet->Selected0.x, fMain->gSheet->Selected0.y);
    Recalculate = true;
    Redraw = true;
  }
void ActionPageDrag (int From, int To)
  {
    //ReadFromPages ();
    SwapBytes (&Pages [From], &Pages [To], sizeof (Pages [0]));
    SetPage (To);
  }

void ActionCellDraw (_GridView *Grid, int x, int y, _Rect Rect)
  {
    _Cell *Cell;
    char *St, *s;
    int l, h;
    int Col;
    _Align al;
    bool ww;
    _Font *Font;
    int Size;
    //
    Col = cText;//Grid->ColourTextFind ();
    Font = Grid->FontFind ();
    Font->Style = 0x00;
    Size = Font->Size;
    al = aLeft;
    s = St = NULL;
    l = 0;
    ww = false;
    while (true)
      {
        if (y == 0)   // Title Column
          {
            Col = cBlueDark;
            al = aCenter;
            if (x > 0)
              l += StrColumn (&s, x - 1);
          }
        else if (x == 0)   // Title Row
          {
            Col = cBlueDark;
            al = aCenter;
            l += StrRow (&s, y - 1);
          }
        else   // Sheet Body
          {
            Cell = CellFind (Page, x - 1, y - 1, false);
            if (Cell)
              {
                if (St == NULL)   // just do this once
                  {
                    ww = StrPos (Cell->Format, 'W');
                    if (Cell->Exp)
                      //Grid->DrawRectangle (AddMargin (Rect, {2, 2}), -1, -1, ColourAdjust (cRed, 190));
                      Col = cGreenDark; // ColourAdjust (cRed, 80);
                    if (StrPos (Cell->Format, '>'))
                      al = aRight;
                    if (StrPos (Cell->Format, 'B'))
                      Font->Style |= fsBold;
                    if (StrPos (Cell->Format, 'I'))
                      Font->Style |= fsItalic;
                    if (StrPos (Cell->Format, 'U'))
                      Font->Style |= fsUnderline;
                    if (StrPos (Cell->Format, 'O'))
                      Font->Style |= fsOutline;
                    if (StrPos (Cell->Format, '+'))
                      FontSizeSet (Font, Size + 9);
                    if (StrPos (Cell->Format, '-'))
                      FontSizeSet (Font, Size - 5);
                  }
                l += StrCellFormatValue (&s, Cell, true);
              }
          }
        if (l == 0)
          break;
        if (St == NULL)
          s = St = (char *) malloc (l + 1);
        else
          {
            *s = 0;
            break;
          }
      }
    if (St)
      {
        Grid->FontFind ()->Colour = Col;
        if (ww)
          h = Grid->TextOutWrap (AddMargin (Rect, {4, 0}), St, al, aCenter);   // Get the content height
        else
          h = Grid->TextOutAligned (AddMargin (Rect, {4, 0}), St, al, aCenter);
        if (h + 8 > Grid->CellHeightGet (y))   // and make it fit
          Grid->CellHeightSet (y, h + 8);
        if (Font)
          Font->Style = 0x00;
        free (St);
      }
    if (Font->Size != Size)
      FontSizeSet (Font, Size);
  }

const bool EditInSheet = false; //true;

char *ActionCellRead (_GridView *Grid, int x, int y)
  {
    _Cell *Cell;
    char *Res;
    //
    Res = "";
    // read from the matrix and give to the grid
    Cell = CellFind (Page, x - 1, y - 1, false);
    if (Cell)
      if (Cell->Content)
        Res = Cell->Content;
    if (EditInSheet)
      return Res;
    // Edit in Form Header
    fMain->eContent->VisibleSet (true);
    fMain->eContent->TextSet (Res);
    fMain->eContent->FocusSet ();   // shuld not be vecessary
    fMain->EventLock = fMain->eContent;
    return NULL;
  }

void ActionCellWrite (_GridView *Grid, int x, int y, char *Data, bool Abort)
  {
    // write back to matrix
    if (!Abort)
      WriteToCellContent (Data, x - 1, y - 1);
  }

void ActionArrow (_ButtonArrow *Button)
  {
    if (Button->Direction == dUp)
      FontSize += 2;
    else
      FontSize = Max (6, FontSize - 2);
    FontSizeSet (fMain->gSheet->FontFind (), FontSize);
    fMain->gSheet->Invalidate ();
  }

void CellClear (_Cell *Cell)
  {
    if (Cell)
      {
        StrAssign (&Cell->Content, NULL);
        StrAssign (&Cell->Format, NULL);
        ProcessCell (Cell);
        CellsChanged = true;
      }
  }

void CellBlockClear (int x1, int y1, int x2, int y2)
  {
    int x, y;
    _Cell *Cell;
    //
    for (y = y1; y <= y2; y++)
      for (x = x1; x <= x2; x++)
        {
          Cell = CellFind (Page, x, y, false);
          if (Cell)
            CellClear (Cell);
        }
  }

void ActionCutCopy (_Button *Button)
  {
    unsigned int Size;
    _Mode Mode;
    _Point s0, s1;
    //
    Mode = mSS;
    if (Button == NULL)   // copy values
      Mode = mVal;
    GridGetSelected (&s0, &s1);
    Clipboard.Cut = Button == fMain->bCut;
    Clipboard.x = s0.x;
    Clipboard.y = s0.y;
    ClipboardSet (fMain->Window, CellsToText (Clipboard.x, Clipboard.y, s1.x, s1.y, &Size, Mode));
    if (Clipboard.Cut)
      {
        CellBlockClear (s0.x, s0.y, s1.x, s1.y);
        fMain->gSheet->Invalidate ();
      }
  }

void ActionPaste (_Button *Button)
  {
    _Point s0, s1;
    //
    GridGetSelected (&s0, &s1);
    BlockPaste (s0.x, s0.y, s1.x, s1.y);
  }

void ActionDelete_ (int Result, void *Parameter)
  {
    _Point s0, s1;
    int x, y;
    //
    if (Result == 0)   // Yes selected
      {
        GridGetSelected (&s0, &s1);
        for (y = s0.y; y <= s1.y; y++)
          for (x = s0.x; x <= s1.x; x++)
            WriteToCellContentFormat (NULL, x, y);
        Redraw = true;
        Recalculate = true;
      }
  }

void ActionDelete (_Button *Button)
  {
    MessageResponseBox ({200, 80}, "Delete Cells?", "Yes\tNo", ActionDelete_);
    fMessageResponseBox->Container->Colour = cRedLight;
  }

inline bool CellHasValue (_Cell *Cell)
  {
    //if (Cell)
      if (Cell->Content)
        if (!Cell->Error)
          return true;
    return false;
  }

int CellCompare (_Cell *A, _Cell *B)
  {
    if (A && B)   // both Cells defined
      {
        if (CellHasValue (A)  && CellHasValue (B))
          return ComplexCompare (&A->Value, &B->Value);
        if (CellHasValue (A))
          return -1;
        if (CellHasValue (B))
          return 1;
        return StrCmp (A->Content, B->Content);
      }
    if (A)
      return -1;
    return 1;
  }

void BlockSort (int SortColumn)
  {
    _Point s0, s1;
    int x, y;
    bool Sorted;
    _Cell *CellA, *CellB;
    //
    GridGetSelected (&s0, &s1);
    while (true)
      {
        Sorted = true;
        y = s0.y;
        while (y < s1.y)
          {
            CellA = CellFind (Page, SortColumn, y, false);
            CellB = CellFind (Page, SortColumn, y + 1, false);
            //if (CellA && CellB)
              if (CellCompare (CellA, CellB) > 0)   // swap needed
                {
                  AdjustContent (s0.x, y,     s1.x - s0.x + 1, 1, 0, +1, false, {s1.x - s0.x, s1.y - s0.y}); // %%%%
                  AdjustContent (s0.x, y + 1, s1.x - s0.x + 1, 1, 0, -1, false, {s1.x - s0.x, s1.y - s0.y});
                  for (x = s0.x; x <= s1.x; x++)
                    CellSwap (Page, x, y, x, y + 1);
                  CellsChanged = true;
                  Sorted = false;
                }
            y++;
          }
        if (Sorted)
          break;
      }
    Recalculate = true;
    Redraw = true;
  }

#define UniqueXMax 8

void BlockSummary (int SummaryColumn)
  {
    char *Unique;
    int UniqueCount;
    Complex *UniqueTotals;
    char *Table, *t;
    int n, len;
    int Column;
    char *s;
    _Point s0, s1;
    _Point p;
    _Cell *Cell;
    //
    GridGetSelected (&s0, &s1);
    // Build list of unique names from left selected column AND sum the totals
    Unique = NULL;
    UniqueCount = 0;
    UniqueTotals = NULL;
    while (true)
      {
        for (p.y = s0.y; p.y <= s1.y; p.y++)
          {
            Cell = CellFind (Page, SummaryColumn, p.y, false);   // Key field
            if (Cell && !Cell->Exp)
              {
                n = StrAddItemEnd (&Unique, Cell->Content);
                UniqueCount = Max (UniqueCount, n + 1);
                if (UniqueTotals)
                  for (p.x = s0.x; p.x <= s1.x; p.x++)
                    if (p.x != SummaryColumn)   // not the Key field
                      {
                        Column = p.x - s0.x;
                        if (Column < UniqueXMax)
                          {
                            Cell = CellFind (Page, p.x, p.y, false);
                            if (Cell && !Cell->Error)
                              ComplexAdd (&UniqueTotals [n * UniqueXMax + Column], &Cell->Value, &UniqueTotals [n * UniqueXMax + Column]);
                          }
                      }
              }
          }
        if (UniqueTotals)
          break;
        UniqueTotals = (Complex *) malloc (UniqueCount * UniqueXMax * sizeof (Complex));
        memset (UniqueTotals, 0, UniqueCount * UniqueXMax * sizeof (Complex));
      }
    // Make Format Resluts: Unique Names & Totals
    t = Table = NULL;
    len = 0;
    while (true)
      {
        for (n = 0; n < UniqueCount; n++)
          {
            for (p.x = s0.x; p.x <= s1.x; p.x++)
              {
                if (p.x == SummaryColumn)
                  {
                    s = StrGetItem (Unique, n);
                    len += StrCat (&t, s);
                    free (s);
                  }
                else
                  {
                    Column = p.x - s0.x;
                    if (Column < UniqueXMax)
                      {
                        Cell = CellFind (Page, p.x, s0.y, false);
                        if (Cell)
                          len += StrValueFormat (&t, &UniqueTotals [n * UniqueXMax + Column], Cell->Format, true, true);
                      }
                  }
                len += StrCat (&t, '\t');
              }
            len += StrCat (&t, '\n');
          }
        if (t)
          break;
        t = Table = (char *) malloc (len + 1);
      }
    //
    *t = 0;
    ClipboardSet (fMain->Window, Table);
    free (Table);
    free (Unique);
    free (UniqueTotals);
  }

void ActionSortSummary_ (_SelectionList *SelectionList)
  {
    int Col;
    //
    Col = SelectionList->Selected;
    //DebugAddInt ("ActionSort/Summary", Col);
    if (Col >= 0)
      if (SelectionList->DataContainer == fMain->bSort)
        BlockSort (Col + SelectionList->DataInt);
      else   // bSummary
        BlockSummary (Col + SelectionList->DataInt);
    SelectionList->Die ();
  }

void ActionSortSummary (_Button *Button)
  {
    _Point s0, s1;
    char *Cols, *c;
    int x, l;
    _SelectionList *sl;
    //
    // Get Range
    GridGetSelected (&s0, &s1);
    // Build Column Menu
    c = Cols = NULL;
    while (true)
      {
        l = 0;
        for (x = s0.x; x <= s1.x; x++)
          {
            l += StrColumn (&c, x);
            l += StrCat (&c, '\t');
          }
        if (Cols)
          break;
        c = Cols = (char *) malloc (l);
      }
    c--;   // lose last tab
    *c = '\0';
    sl = new _SelectionList (fMain->Container, {Button->Rect.x, Button->Rect.y + Button->Rect.Height, 0, 0}, Cols, (_Action) ActionSortSummary_);
    sl->DataInt = s0.x;   // Note where we start
    sl->DataContainer = Button;
    free (Cols);
  }

void MessageResponseBoxSave (_ActionResultInt Action)
  {
    MessageResponseBox ({400, 100}, "Alert\v\abSpreadsheet Changed:\ab", "Save File\tKeep Editing\t\a1Discard Changes", Action);
    fMessageResponseBox->Container->Colour = cOrange;  // these colours suck
    fMessageResponseBox->Container->ColourGrad = cYellow;
  }

void ActionSave_ (char *Result, void *Parameter)
  {
    if (Result)
      {
        StrAssignCopy (&FileName, Result);
        WriteToPages ();   // flush values back inti pPage
        SaveSheets ();
      }
  }

void ActionSave (_Button *Button)
  {
    FileSelect ("Save Spreadsheet", ".ss", true, ActionSave_);
    FileSelectSetFile (FileName);
  }

void ActionLoad__ (char *Result, void *Parameter)
  {
    char *t;
    //
    if (Result)
      {
        StrAssignCopy (&FileName, Result);
        if (LoadSheets ())
          {
            SetPage (0);
            fMain->tPages->Selected = 0;
            fMain->tPages->Invalidate ();
            t = StrMake (StrFindFilename (Result), " - ");
            StrAdd (&t, Title);
            WindowSetTitle (fMain->Window, t);
            free (t);
            ReadFromPages ();
            //ActionPageSelect ();
          }
      }
  }

void ActionLoad_ (int Result, void *Parameter)
  {
    if (Result == 0)   // Save
      ActionSave (NULL);
    else if (Result == 2)   // Don't save
      FileSelect ("Load Spreadsheet", ".ss", false, ActionLoad__);
  }

void ActionLoad (_Button *Button)
  {
    if (CellsChanged)
      MessageResponseBoxSave (ActionLoad_);
    else
      ActionLoad_ (2, NULL);
  }

void ActionGraph (_Button *Button)
  {
    new _FormGraph ("Graph", MousePos ());
  }

void ActionFormat (_Button *Button)
  {
    if (UpCase (Button->DataInt) == 'X')   // Radians mode change
      Recalculate = true;
    ApplyFormatBlock ((char) Button->DataInt, Button->Down);
  }

void ActionSaveRequest (int Result, void *Parameter)
  {
    if (Result == 0)   // Save
      {
        WriteToPages ();
        if (SaveSheets ())
          Quit = true;
        else
          ActionSave (fMain->bSave);
      }
    else if (Result == 2)   // Discard
      Quit = true;
  }

void ActionFind (_ButtonArrow *Button)
  {
    int Page0, x0, y0;;
    int x, y;
    _Cell *Cell;
    //
    pInput = NULL;   // cancel direct input
    WriteToPages ();   // save page state
    Page0 = Page;
    x0 = x = Min (pPage->XSel, pPage->XSelEnd);
    y0 = y = Min (pPage->YSel, pPage->YSelEnd);
    while (true)
      {
        if (Button->Direction == dRight)   // Next
          if (++x > pPage->XSelEnd)
            {
              x = 0;
              if (++y > pPage->YSelEnd)
                {
                  y = 0;
                  if (++Page >= NumPages)
                    Page = 0;
                  SetPage (Page);
                }
            }
        if (Button->Direction == dLeft)   // Previous
          if (--x < 0)
            {
              if (--y < 0)
                {
                  if (--Page < 0)
                    Page = NumPages - 1;
                  SetPage (Page);
                  y = pPage->YSelEnd;
                }
              x = pPage->XSelEnd;
            }
        if (Page == Page0 && x == x0 && y == y0)   // back to start page
          {
            Beep ();
            break;
          }
        Cell = CellFind (Page, x, y, false);
        if (Cell)
          if (StrPos (Cell->Content, fMain->eTarget->TextGet ()))   // found
            {
              pPage->XSel = x;
              pPage->YSel = y;
              ReadFromPages ();
              fMain->tPages->Selected = Page;
              fMain->tPages->Invalidate ();
              ActionCellSelect (fMain->gSheet, x, y);
              break;
            }
      }
  }


///////////////////////////////////////////////////////////////////////////////
//
// HELP

const char *Help =
  "Help\v"
  "\ab\a+\auSTEWY'S MATHS SPREADSHEET\au:\ab\a-\n"
  "\n"
  "Simple yet 'full featured' Spread Sheet with a Maths emphasis and based on complex numbers. "
  "A counter example of how bloated software has become.\n"
  "\n"
  "\au\abToolbar\au:\ab The basic edit, data and file functions, Graph, Help and Search. "
  "The left / right arrows are search Previous / Next. "
  "The small up / down arrows change the display scale.\n"
  //"Sort and Brief require a block selection first and then a Column will be requested. "
  //"Sort changes the selected Block, Brief produces a summary of the Block in the Clipboard.\n"
  "\n"
  "\e6 \au\abSort\au:\ab Select a block to sort, click Sort and select a sorting Column.\n"
  "\ee\au\abBrief\au:\ab Select a block to summarise, click Brief and select a Key field column. "
  "This will produce a table (in the Clipboard) listing every unique Key value and a corresponding total for each other column. "
  "Paste this somewhere useful.\n"
  "\e9 \au\abGraph\au:\ab Creates a new window displaying a graph of the current selection.\n"
  "Right click for Graph options: "
  "\edChoose between Bar and XY graphs. "
  "\edLeft Column Labels: Tick to specify the left column as X labels rather than data (Bar only). "
  "\edTop Row Labels: Tick to specify the top row as Data Tags rather than data. "
  "\edInclude 0.0: Tick to force inclusion of Data point 0. "
  "\edAdd Lines: Tick to include lines between points (XY only). "
  "\edToggle Columns: Pull up a list to enable / disable data columns. "
  "\edExport: Save the Graph image as a File.\n"
  "\n"
  "\au\abFormatting\au:\ab\n"
  "\edToggle the buttons to change the layout, format and precision of Cells.\n"
  "\edThis includes \eA for right justify, \eB for Word Wrap, \eC to Blank Zero values.\n"
  "\ed\a1Float Sci Eng\a0 control the exponent. "
  "\a1$\a0 for currency, \a10x\a0 for Hexadecimal, \a1a/b\a0 to display numbers as fractions (if possible), \a1%\a0 for percentage.\n"
  "\ed\a1Date Time\a0 for dates, time and time intervals, \a1Pol\a0 for Polar coordinates (complex numbers only).\n"
  "\ed\a10\a0-\a19\a0 specify precision (select none for floating) AND variances of Date, Time and a/b formats.\n"
  "\ed\a1B\a0old \a1I\a0talic, \a1U\a0nderline and \a1O\a0utline, \a1+ -\a0 to change size.\n"
  "\ed\a1rad\a0ians mode (trig functions and Polar display) and \e8 for Commas.\n"
  "\n"
  "\ab\auCurrent Cell(s)\au:\ab Displays Current Selected Cell, raw contents and result OR The selected Block with the Sum of all selected Cells.\n"
  "\n"
  "\ab\auPage Tabs\au:\ab Select the current Page: \a1\abA..Z\ab\a0.\n"
  "\n"
  "\ab\auCell Contents\au:\ab\n"
  "\edDouble-click (or press \abEnter\ab) to edit a Cell's contents. "
  "OR, move to a Cell and just type:\n"
  "\edIn this case, use \abarrow keys\ab to insert a reference to another cell. \abBack Space\ab to delete. "
  "\abEsc\ab to quit. \abEnter\ab when done.\n"
  "\n"
  "Cells can contain Text, Values or Expressions. "
  "Eg \a1Distance\a0, \a113.1415\a0, \a122/7\a0, \a1=A7+B5^B4\a0.\n"
  "\n"
  "\ab\auKeyboard Shortcuts\au:\ab\n"
  "\ab^X\ab for Cut. \ab^C\abopy, \ab^V\ab for Paste.\n"
  "\ab^F\abind - Search for text, \ab^N\abext, \ab^P\abrevious.\n"
  "\ab^S\abave spread sheet.\n"
  "\abDel\abete Cell(s).\n"
  "\ab^Q\abuit - Save and Exit.\n"
  "\n"
  "\ab\auRight Click\au:\ab\n"
  "\edCut, Copy and Paste as usual.\n"
  "\edCopy Values: Same as Copy EXCEPT the displayed values will copied instead of the Raw Cell Contents.\n"
  "\edFreeze Above / Left: Locks all Cells above and to the left of this Cell. "
  "Good for large spreadsheets when you need to see the top or left cells at all times.\n"
  //"If it starts with an \a1=\a0 the calculated value will be shown in the specified Layout.\n"
  "\n"
  "\au\abSharing Data\au:\ab\n"
  "Data from other apps can be Pasted into a Spreadsheet and Cells can be Copied from here and Pasted into other apps. "
  "Use the Copy Values command in this case.\n"
  "\n"
  "\ab\auValues and Expressions\au:\ab\n"
  "Values can be: Scientific Notation eg \a16.02e23\a0, "
  "Complex eg \a13.5i\a0, \a1(6+2i)\a0, "
  "Hexadecimal eg \a10xC000\a0, "
  "Currency eg \a1$40.50\a0, "
  "Percentage eg \a155%\a0 and "
  "Date format eg \a11959-07-6\a0, \a16/7/1959\a0, \a11 Mar 2000\a0 or \a1Mar 1 2000\a0.\n"
  "\n"
  "\edAny Cell's value can be referenced by ColumnRow. eg \a1K9\a0 optionally specifying a Page. eg \a1B.E16\a0. "
  "The Column and/or Row can be preceded by a \a1$\a0. This locks the reference for subsequent Copy / Paste operations. "
  "eg \a1$H12\a0, \a1$B$52\a0.\n"
  "\edCell references can be Offset by Expressions for Column and Row. eg \a1K9 [K1+2, 3]\a0 "
  "These offsets can be either numerical OR a search string. eg \a1E4 [\"target\", 2]\a0 or \a1C.A1 [3, \"Ork\"]\a0 "
  "This allows lookup tables referenced by Name, then offset to extract the needed parameter associated with the Name.\n"
  "\n"
  "\edArithmetic operations: \a1+ - * /\a0  Raise to power with \a1^\a0  Modulo with \a1%\a0.\n"
  "\edLogical Operations: \a1|\a0 for OR, \a1&\a0 for AND, \a1||\a0 for EXCLUSIVE OR, \a1<< >>\a0 for shift.\n"
  "\edAlgebraic hierarchy: \a1()\a0 then \a1^ << >>\a0 then \a1* / % &\a0 then \a1+ - |\a0. "
  "Eg \a1= 3 + 2 ^ 3\a0 will result in \a111\a0\n"
  "\n"
  "\ab\auFunctions\au:\ab\n"
  "\abSqrt\ab - Square Root  eg \a1sqrt 7\a0\n"
  "\abSqr\ab - Square  eg \a1sqr 7\a0\n"
  "\abLn\ab - Natural Log (base e)  eg \a1ln (3 + E4)\a0\n"
  "\abLog\ab - Log (base 10)  eg \a1log 100\a0\n"
  "\abExp\ab - Natural antilog  eg \a1EXP 4.7\a0\n"
  "\abAbs\ab - Absolute value\n"
  "\n"
  "\abIF\ab - Select between 2 expressions depending on a Boolean Expression. "
  "Format: if (<BoolExp>, <ExpIfTrue>, <ExpIfFalse>). BoolExp has 2 forms: Either it is an expression, followed by a <, >, =, <=, >=, <>, then another expression. "
  "OR it is <string>/Cell IN <string>/Cell. This is true if the first string is found in the second string. "
  "Examples: \a1if (D3 / C1 < 7, A1 * 5, A1 * 6)\a0, \a1if (\"tax\" in $A3, A2, 0)\a0.\n"
  "\n"
  "\abRandom\ab - Random integer from 0 to (parameter-1)  eg \a1Random 100\a0 gives 0..99\n"
  "\ab!\ab - Factorial: How many ways to arrange n items  eg \a180!\a0\n"
  "\abC\ab - Combination (n, r): How many ways to SELECT r things from n\n"
  "\abP\ab - Permutation (n, r): How many ways to ARRANGE r things from n\n"
  "\abB\ab - Binomial Probability: 2 uses: "
  "\a1B (p, n, r)\a0  Probability of exactly \a1r\a0 events (of probability \a1p\a0) happening from \a1n\a0 tries. "
  "\a1B (p, n, r1, r2)\a0  As above but r can be between \a1r1\a0 and \a1r2\a0 inclusive. "
  "eg \a1C (80, 8)\a0, \a1P (80, 1)\a0, \a1B (0.5, 10, 5)\a0, \a1B (0.1, 100, 40, 60)\a0\n"
  "\abGCD\ab - Greatest Common Divisor  eg \a1GCD (128, B4+6)\a0\n"
  "\abLCM\ab - Least Common Multiple  eg \a1LCM (128, B4+6)\a0\n"
  "\n"
  "\auTrig\au: Angular parameters/results are either degrees or radians (depending on Rad button)\n"
  "\abPi\ab - Pi\n"
  "\abSin\ab - Sine  eg \a1sin (pi / 4)\a0\n"
  "\abCos\ab - Cosine  eg \a1cos (0.2)\a0\n"
  "\abTan\ab - Tangent  eg \a1tan (pi/4)\a0\n"
  "\abASin\ab - Arc sine  eg \a1asin (E12/E11)\a0\n"
  "\abACos\ab - Arc cosine  eg \a1acos 0.9\a0\n"
  "\abATan\ab - Acr tangent  eg \a1atan AA2\a0\n"
  "\n"
  "\auFunctions over a region\au:\n"
  "\abCount\ab - Count of non empty cells  eg \a1Count (A1,C6)\a0\n"
  "\abSum\ab - Total values  eg \a1Sum (A1, G9)\a0\n"
  "\abSumSquare\ab - Total squared values  eg \a1SumSquare (A1, G9)\a0\n"
  "\abAvg\ab - Average = Sum() / Count()  \a1Avg (A3, A9)\a0\n"
  "\abStDev\ab - Standard Deviation {n-1}  eg \a1StDev (D4, e99)\a0\n"
  "\abStDevN\ab - Standard Deviation {n}  eg \a1StDevN (D4, e99)\a0."
  "\n"
  "** Optionally, the above can also contain a function applied to each cell: "
  "Eg \a1Sum (a1, b20, sqrt x)\a0, \a1Avg (a1, b20, exp (x/7))\a0\n"
  "\n"
  "\auSpecial\au:\n"
  "\abDate\ab - Convert a date (year, month, day) into its numerical representation  eg \a1Date (2020,6,7)\a0\n"
  "\abTime\ab - Convert a time (hour, minute, second) into its numerical representation  eg \a1Time (16,30,0)\a0\n"
  "\abAddMonth\ab - Add/Subtract a number of months to a date (Date, MonthsToAdd)  eg \a1AddMonth (A3,-1)\a0. "
  "To add/subtract days, just use \a1+\a0 and \a1-\a0. Every day equals 1. Eg \a1A3+7\a0\n"
  //"	Useful in conjunction with the date Layouts |d| and |t|\n"
  "\abInt\ab - Integer portion\n"
  "\abFrac\ab - Fractional portion\n"
  "\abReal\ab - Real portion\n"
  "\abImag\ab - Imaginary portion\n"
  "\abMod\ab - Modulus (magnitude)\n"
  "\abArg\ab - Argument (direction)\n"
  "\abComplex\ab - Make a complex from 2 reals  eg \a1Complex (A3+2, sqrt (B1))\a0\n"
  "\abComplexPolar\ab - Make a complex from the Modulus and the Argument  eg \a1ComplexPolar (1, pi / 2)\a0\n"
  "\abSolve\ab - Solve an equation for a root, 2 modes: "
  "\a1Solve (<fnX>, x0)\a0  Newtons Method. Start at x0  eg \a1Solve (x * ln (x) - B3, 2.0)\a0. "
  "\a1Solve (<fnX>, x0, x1)\a0  Binary Chop Method. Find solution between x0 and x1. Eg \a1Solve (x*sin x - 1, 2, 3)\a0\n"
  "\abIntegrate\ab - Integrate a function between 2 points using Simpsons Rule\n"
  "Parameters: Function (of x), x1, x2, Intervals (must be even). "
  "Eg \a1Integrate (x * ln x, 1, 2, 8)\a0\n"
  "\abDifferentiate\ab - Differentiate an equation at point x. Eg \a1Differentiate (x * ln x, 1)\a0\n"
  "\abRational\ab - Closest Rational value within Error. Eg \a1Rational (pi, 0.01)\a0\n"
  "\n"
  "\auFinancial\au:\n"
  "\abMortgage\ab - Calculate Mortgage Repayments: "
  "Parameters: Principle, Rate, Number of Instalments. "
  "Eg \a1Mortgage (300000, 0.03/12, 20*12)\a0 (and yes, this is how banks \"calculate\" monthly interest)";

void ActionHelp (_Container *Container)
  {
    MessageBox ({600, 600}, Help, 14);
    fMessageBox->lMessage->AlignVert = aTop;
    fMessageBox->lMessage->Align = aLeft;
    fMessageBox->lMessage->Margin = 4;
    fMessageBox->Container->Colour = cBlueLight;
    fMessageBox->Container->ColourGrad = cWhite;
    fMessageBox->Container->Bitmap = BitmapLoad (fMessageBox->Window, "SpreadSheet.bmp");
  }


///////////////////////////////////////////////////////////////////////////////
//
// MAIN FORM

void ActionPopup (_MenuPopup *mp)
  {
    switch (mp->Selected)
      {
        case 0: ActionCutCopy (fMain->bCut); break;
        case 1: ActionCutCopy (fMain->bCopy); break;
        case 2: ActionPaste (fMain->bPaste); break;
        case 3: ActionCutCopy (NULL); break; // copy values
        case 4: fMain->gSheet->Locked = fMain->gSheet->Selected1;
                CellsChanged = true;
                Redraw = true;
                break;
      }
  }

void TryToQuit (void)
  {
    if (CellsChanged)
      MessageResponseBoxSave (ActionSaveRequest);
    else
      Quit = true;
  }

bool FormMainEventPreview (_Form *Form, _Event *Event)
  {
    bool Res;
    //
    if (FileSelectActive)   // make Form FileSelect modal
      return true;
    Res = true;
    if (Event->Type == etWindowClose)
      TryToQuit ();
    else if (Event->Type == etKeyDown)
      if (fMain->Focused == fMain->gSheet)
        {
          if (Event->Key == Cntrl ('X'))
            ActionCutCopy (fMain->bCut);
          else if (Event->Key == Cntrl ('C'))
            ActionCutCopy (fMain->bCopy);
          else if (Event->Key == Cntrl ('V'))
            ActionPaste (fMain->bPaste);
          else if (Event->Key == KeyDel)
            ActionDelete (NULL);
          else if (Event->Key == Cntrl ('F'))
            {
              pInput = NULL;
              fMain->eTarget->FocusSet ();
              fMain->eTarget->Clipboard (coSelectAll);
            }
          else if (Event->Key == Cntrl ('N'))
            ActionFind (fMain->bNext);
          else if (Event->Key == Cntrl ('P'))
            ActionFind (fMain->bPrev);
          else if (Event->Key == Cntrl ('S') && CellsChanged)
            {
              WriteToPages ();
              SaveSheets ();
            }
          else if (Event->Key == Cntrl ('Q'))
            TryToQuit ();
          else if (Event->Key == '\r' && pInput)
            {
              pInput = NULL;
              WriteToCellContent (Input, InputPos.x - 1, InputPos.y - 1);
              fMain->gSheet->Selected0 = fMain->gSheet->Selected1 = InputPos;
              ActionCellSelect (fMain->gSheet, InputPos.x, InputPos.y);   // Set Format buttons
              fMain->gSheet->Invalidate ();
            }
          else if (Event->Key == '\e')
            if (pInput)
              pInput = NULL;
            else
              {
                fMain->gSheet->Selected0 = fMain->gSheet->Selected1;
                fMain->gSheet->Invalidate ();
              }
          else if (Event->Key == '\b' && pInput)
            if (--pInput == Input)
              pInput = NULL;
            else
              *(pInput) = 0;
          else if ((Event->Key & KeyMax) >= ' ' && (Event->Key & KeyMax) < 0x80)
            {
              if (pInput == NULL)
                {
                  InputPos = fMain->gSheet->Selected1;
                  pInput = Input;
                }
              *pInput = Event->Key & KeyMax;
              if (pInput - Input + 1 < InputMax)
                pInput++;
              *pInput = 0;
              pInputCoord = pInput;
            }
          else
            Res = false;
        }
      else if (fMain->Focused == fMain->eTarget)
        if (Event->Key == '\r')
          ActionFind (fMain->bNext);
        else if (Event->Key == '\e')
          fMain->gSheet->FocusSet ();
        else
          Res = false;
      else if (fMain->Focused == fMain->eContent)
        {
          if (Event->Key == '\r')
            {
              WriteToCellContent (fMain->eContent->TextGet (), fMain->gSheet->Selected1.x - 1, fMain->gSheet->Selected1.y - 1);   // Write cell data
              fMain->eContent->VisibleSet (false);
              fMain->gSheet->FocusSet ();
            }
          else if (Event->Key == '\e')
            {
              fMain->eContent->VisibleSet (false);
              fMain->gSheet->FocusSet ();
            }
          else
            Res = false;
        }
      else
        Res = false;
    else
      Res = false;
    if (Res)
      Status = true;
    return Res;
  }

#define bw  40
#define bh  38
#define bwf 24
#define lh  26
#define th  28
#define caw 84

const char *FormatsBtns [] =
  {
    "\eA", "", "\eB", "", "\eC"/*[0]*/, "",
    "Float", "Sci", "Eng", "$", "0x", "a/b", "%", "Date", "Time", "",
    "Pol", "",
    "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "",
    "\abB\ab", "", "\aiI\ai", "", "\auU\au", "", "O", "", "\ab+\ab", "\ab-\ab", "",
    "rad", "", "\e8", NULL};

const char *FormatCodes = "> W Z FSECHRNDT P 0123456789 B I U O +- X ,";

_FormMain::_FormMain (char *Title): _Form (Title, {100, 100, 800, 600}, waResizable)
  {
    int x, y, x2;
    int i, grp;
    _Button *btn;
    _Point sz;
    char TabSt [26*2+1], *t;
    //
    EventPreview = FormMainEventPreview;
    Icon = BitmapLoad (Window, "icon.bmp");
    WindowSetIcon (Window, Icon);
    Container->FontSet ("ARI.ttf");//, 14);
    Container->Bitmap = BitmapLoad (Window, "SpreadSheet.bmp");
    //
    y = 0;
    Toolbar = new _Container (Container, {0, y, 0, bh});
    Toolbar->Colour = ColourAdjust (cForm1, 80);
    x = 0;
    bCut = new _Button (Toolbar, {x, 0, bw, 0}, "\e2\nCut", (_Action) ActionCutCopy);  x += bw;
    bCopy = new _Button (Toolbar, {x, 0, bw, 0}, "\e3\nCopy", (_Action) ActionCutCopy);  x += bw;
    bPaste = new _Button (Toolbar, {x, 0, bw, 0}, "\e4\nPaste", (_Action) ActionPaste);  x += bw;
    bDelete = new _Button (Toolbar, {x, 0, bw, 0}, "\e5\nDelete", (_Action) ActionDelete);  x += bw;
    x += 4;
    bSort = new _Button (Toolbar, {x, 0, bw, 0}, "\e6\nSort", (_Action) ActionSortSummary);  x += bw;
    bSummary = new _Button (Toolbar, {x, 0, bw, 0}, "\ee\nBrief", (_Action) ActionSortSummary);  x += bw;
    x += 4;
    bLoad = new _Button (Toolbar, {x, 0, bw, 0}, "\e0\nLoad", (_Action) ActionLoad);  x += bw;
    bSave = new _Button (Toolbar, {x, 0, bw, 0}, "\e1\nSave", (_Action) ActionSave);  x += bw;
    x += 4;
    bGraph = new _Button (Toolbar, {x, 0, bw, 0}, "\e9\nGraph", (_Action) ActionGraph);  x += bw;
    x += 4;
    bHelp = new _Button (Toolbar, {x, 0, bw, 0}, "\e7\nHelp", (_Action) ActionHelp);  x += bw;
    x += 4;
    bPrev = new _ButtonArrow (Toolbar, {x, 0, bwf, 0}, NULL, (_Action) ActionFind, dLeft); x += bwf;
    bNext = new _ButtonArrow (Toolbar, {x, 0, bwf, 0}, NULL, (_Action) ActionFind, dRight); x += bwf;
    //
    x2 = -bh / 2;
    bLarger = new _ButtonArrow (Toolbar, {x2, 0, 0, bh / 2}, NULL, (_Action) ActionArrow, dUp);
    bSmaller = new _ButtonArrow (Toolbar, {x2, bh / 2, 0, 0}, NULL, (_Action) ActionArrow, dDown);
    x2 -= 4;
    eTarget = new _EditPopup (Toolbar, {x, 4, x2, -4}, NULL);
    y += bh;
    // Format controls, scripted
    cFormat = new _Container (Container, {0, y, 0, 26}, "Formatting");
    x = 0;
    i = 0;
    grp = 1;
    while (FormatsBtns [i])
      {
        if (*FormatsBtns [i] == 0)   // new group
          {
            grp++;
            x += 4;
          }
        else
          {
            sz = cFormat->TextMeasure ((char *) FormatsBtns [i]);
            btn = new _Button (cFormat, {x, 0, sz.x + 8, 0}, (char *) FormatsBtns [i], (_Action) ActionFormat);
            btn->Group = grp;
            btn->Toggle = true;
            btn->DataInt = FormatCodes [i];
            x += btn->Rect.Width;
          }
        i++;
      }
    y += cFormat->Rect.Height;
    // Contents / Input strip
    x = 4;
    lCellAddress = new _Label (Container, {x, y + 2, caw, lh - 4}, NULL); x += caw;
    lCellAddress->Border = bLowered;//;bMote
    lContents = new _Label (Container, {x, y + 2, -4, lh - 4}, "Cell Contents");
    lContents->Border = bLowered;//;bMote
    //lContents->Margin = 4;
    eContent = new _EditContents (lContents, {0});
    eContent->VisibleSet (false);
    y += lh;
    // Page Tabs & buttons
    x2 = 0;
    t = TabSt;
    i = 'A';
    while (true)
      {
        *t++ = i;
        if (i++ == 'Z')
          break;
        *t++ = '\t';
      }
    *t = 0;
    tPages = new _Tabs (Container, {0, y, x2, th}, TabSt, (_Action) ActionPageSelect, ActionPageDrag);
    tPages->ColourText = cRedDark;
    tPages->FontSet (NULL, 0, fsBold);
    y += th;
    //  Grid
    gSheet = new _GridView (Container, {0, y, 0, 0}, NULL, 8, 16, ActionCellDraw);
    gSheet->FontSet (NULL, FontSize);
    gSheet->Title = {1, 1};
    gSheet->Locked = {1, 1};
    gSheet->ActionCellRead = ActionCellRead;
    gSheet->ActionCellWrite = ActionCellWrite;
    gSheet->ActionCellSelect = ActionCellSelect;
    gSheet->Options |= goSelectMulti | goAdjustColumn;
    gSheet->CellWidthSet (0, 32);
    mpSheet = new _MenuPopup (gSheet, {0}, "\e2 Cut\t\e3 Copy\t\e4 Paste\t_\e3 Copy Values\tFreeze Above / Left", (_Action) ActionPopup);
    gSheet->FocusSet ();
  }

_FormMain::~_FormMain ()
  {
    BitmapDestroy (Icon);
  }

void ShowContents (void)
  {
    _Point SelA, SelB;
    bool Block;
    char *St, *s;
    int x, y, l;
    Complex Sum;
    _Cell *Cell;
    //
    GridGetSelected (&SelA, &SelB);
    Block = true;
    if (SelA.x == SelB.x)
      if (SelA.y == SelB.y)
        Block = false;
    // Calculate Total OR Find Cell
    if (Block)
      {
        ComplexSet (0.0, 0.0, &Sum);
        for (y = SelA.y; y <= SelB.y; y++)
          for (x = SelA.x; x <= SelB.x; x++)
            {
              Cell = CellFind (Page, x, y, false);
              if (Cell && !Cell->Error)
                ComplexAdd (&Sum, &Cell->Value, &Sum);
            }
      }
    else
      Cell = CellFind (Page, SelA.x, SelA.y, false);
    // Show Cell / Block Address
    s = St = NULL;
    l = 0;
    while (true)
      {
        l += StrCat (&s, "\a4");
        l += StrCat (&s, Page + 'A');
        l += StrCat (&s, '.');
        if (pInput)
          {
            fMain->lContents->Colour = cWhite;
            l += StrColumn (&s, InputPos.x);
            l += StrRow (&s, InputPos.y);
            l += StrCat (&s, "\a0");
          }
        else
          {
            if (Block)
              {
                l += StrColumn (&s, SelA.x);
                l += StrRow (&s, SelA.y);
                l += StrCat (&s, '-');
                l += StrColumn (&s, SelB.x);
                l += StrRow (&s, SelB.y);
              }
            else
              {
                l += StrColumn (&s, fMain->gSheet->Selected1.x - 1);
                l += StrRow (&s, fMain->gSheet->Selected1.y - 1);
              }
            l += StrCat (&s, "\a0");
          }
        if (St)
          {
            *s = 0;
            break;
          }
        s = St = (char *) malloc (l + 1);
      }
    fMain->lCellAddress->Colour = ColourAdjust (cBlue, 180);
    fMain->lCellAddress->TextSet (St);
    free (St);
    // Show Cell Contents OR Sum OR Input
    s = St = NULL;
    l = 0;
    while (true)
      {
        if (pInput)
          {
            fMain->lContents->Colour = cWhite;
            l += StrCat (&s, "\ab");
            l += StrCat (&s, Input);
            l += StrCat (&s, "\ab");
          }
        else
          {
            l += StrCat (&s, "\ab");
            if (Block)
              {
                l += StrCat (&s, "Sum = ");
                l += ComplexToStr (&s, &Sum, DigitsMax);
              }
            else
              if (Cell && Cell->Content)
                {
                  l += StrCat (&s, Cell->Content);
                  //if (Cell->Exp)
                  if (!Cell->Error)
                    //l += StrCat (&s, "** ERR");
                    {
                      l += StrCat (&s, "\ab    \a1>>\a2    \ab");
                      //else
                        l += ComplexToStr (&s, &Cell->Value, DigitsMax);  //StrCellFormatValue (&s, Cell, true);
                      l += StrCat (&s, "\a0");
                    }
                }
            l += StrCat (&s, "\ab");
            //fMain->lContents->Colour = -1;
            fMain->lContents->Colour = ColourAdjust (cBlue, 180);
          }
        if (St)
          {
            *s = 0;
            break;
          }
        s = St = (char *) malloc (l + 1);
      }
    fMain->lContents->TextSet (St);
    free (St);
  }

int main_ (int argc, char *argv [])
  {
    RandomSeed = time (NULL);   // Defined the random number generator seed
    ButtonStyleDefault = bsSimple;
    Title = StrMake ("Stewy's Maths Spreadsheet [", Revision);
    StrAdd (&Title, "]");
    fMain = new _FormMain (Title);
    ButtonStyleDefault = bsFlat;
    if (argc >= 2)
      ActionLoad__ (argv [1], NULL);
    ActionPageSelect (fMain->tPages);
    //
    while (!Quit)
      {
        usleep (10000);
        RecalculateCells ();   // Recalculate everything as required
        if (Redraw)
          {
            fMain->gSheet->Invalidate ();   // %%%% add range redraw
            Redraw = false;
          }
        if (HelpMsg)
          {
            fMain->lContents->TextSet ((char *) HelpMsg);
            HelpMsg = NULL;
            Status = false;
          }
        if (Status)
          {
            Status = false;
            ShowContents ();
          }
        fMain->bSave->EnabledSet (CellsChanged);
        if (FormsUpdate ())
          break;
      }
    while (FormList)
      delete FormList;
    free (Title);
    free (FileName);
    return 0;
  }
