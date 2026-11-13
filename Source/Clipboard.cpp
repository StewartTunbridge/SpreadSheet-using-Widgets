////////////////////////////////////////////////////////////////////////////
//
// CLIPBOARD

struct
  {
    // move to real Clipboard char *ContentFormat;
    int x;
    int y;
    //int SizeX;
    //int SizeY;
    bool Cut;   // true if from a Cut. Effects reference offset
  } Clipboard;

bool ReadPageParameters (char *PageParams)
  {
    int x, w;
    //
    if ((*PageParams & 0x7F) == 0x7F || *PageParams == '\\')
      {
        PageParams++;
        StepSpace (&PageParams);
        if (IsDigit (*PageParams))
          StrGetNum (&PageParams);   // discard fixed character column width
        else if (*PageParams == '{')
          {
            PageParams++;   // step past {
            x = 0;
            while (true)
              {
                StepSpace (&PageParams);
                if (!IsDigit (*PageParams))
                  {
                    PageParams++;   // step past }
                    break;
                  }
                w = StrGetNum (&PageParams);
                if (w)
                  ArraySetInt (&pPage->WidthColumns, x++, w);
              }
          }
        if (StrMatchChar (&PageParams, ','))
          if (IsDigit (*PageParams))
            pPage->PageLock = {0, StrGetNum (&PageParams)};
          else
            {
              if (!StrMatchChar (&PageParams, '{'))
                return false;
              pPage->PageLock.x = StrGetNum (&PageParams);
              if (!StrMatchChar (&PageParams, ','))
                return false;
              pPage->PageLock.y = StrGetNum (&PageParams);
              if (!StrMatchChar (&PageParams, '}'))
                return false;
            }
        if (StrMatchChar (&PageParams, ','))
          pPage->XSel = StrGetNum (&PageParams);
        if (StrMatchChar (&PageParams, ','))
          pPage->YSel = StrGetNum (&PageParams);
        /*if (StrMatchChar (&PageParams, ','))
          pPage->Radians = StrGetNum (&PageParams);
        if (StrMatchChar (&PageParams, ','))
          pPage->Commas = StrGetNum (&PageParams);*/
        return true;
      }
    return false;
  }

void StrFindDelimeter (char **Pos, bool Comma)
  {
    unsigned char c;
    //
    while (true)
      {
        c = **Pos;
        if (c < ' ' || (Comma && c == ','))
          break;
        (*Pos)++;
      }
    if ((c == '\r') && (**Pos == '\n'))   // step past lf in a crlf pair
      (*Pos)++;
  }

_Point TextToCells (int x1, int y1, char *Text, bool Comma)
  {
    int x, y;
    char *t1, *t2;
    char *CellContent;
    char c;
    _Point Res;
    //
    Res = {0, 0};
    if (StrPos (Text, '\t'))
      Comma = false;
    t2 = Text;
    x = x1;
    y = y1;
    while (true)
      {
        t1 = t2;
        StrFindDelimeter (&t2, Comma);
        c = *t2;
        CellContent = StrMake (t1, t2 - t1);
        if (!ReadPageParameters (CellContent))
          {
            Res.x = Max (x - x1 + 1, Res.x);
            Res.y = Max (y - y1 + 1, Res.y);
            WriteToCellContentFormat (CellContent, x, y);
            if (x >= pPage->XSelEnd)
              pPage->XSelEnd = x;
            if (y >= pPage->YSelEnd)
              pPage->YSelEnd = y;
            if (c == '\t' || c == ',')
              x++;
            else if (c == '\n' || c == '\r')
              {
                x = x1;
                y++;
              }
            else if (c == '\f')
              {
                SetPage (Page + 1);
                x = x1;
                y = y1;
              }
          }
        free (CellContent);
        if (c == 0)   // no more
          break;
        t2++;   // past separator
      }
    return Res;
  }

char *PageParametersToText (unsigned int *Size)
  {
    char *Buff, *b;
    int s, x;
    //
    Buff = NULL;
    while (true)
      {
        b = Buff;
        s = 0;
        s += StrCat (&b, '\\');
        s += StrCat (&b, '{');   // Set of Column Widths
        x = 0;
        while (true)
          {
            s += IntToStr (&b, ArrayGetInt (&pPage->WidthColumns, x, 0));
            if (x == pPage->XSelEnd)
              break;
            s += StrCat (&b, ' ');
            x++;
          }
        s += StrCat (&b, '}');
        s += StrCat (&b, ',');
        s += StrCat (&b, '{');
        s += NumToStr (&b, pPage->PageLock.x);
        s += StrCat (&b, ',');
        s += NumToStr (&b, pPage->PageLock.y);
        s += StrCat (&b, "},");
        s += NumToStr (&b, pPage->XSel);
        s += StrCat (&b, ',');
        s += NumToStr (&b, pPage->YSel);
        /*s += StrCat (&b, ',');
        s += NumToStr (&b, pPage->Radians);
        s += StrCat (&b, ',');
        s += NumToStr (&b, pPage->Commas);*/
        s += StrCat (&b, '\t');
        if (Buff)
          {
            *b = 0;   // terminate
            if (Size)   // report Size (if wanted)
              *Size = s;
            return Buff;
          }
        Buff = (char *) malloc (s + 1);
      }
  }

typedef enum {mSS, mVal, mRaw} _Mode;

// Covert a block of cells to tab/lf separated text
// caller must free result
char *CellsToText (int x1, int y1, int x2, int y2, unsigned int *Size, _Mode Mode)
  {
    _Cell *Cell;
    int x, y;
    char *Buff, *b;
    unsigned int s;
    //
    Buff = NULL;
    while (true)
      {
        b = Buff;
        s = 0;
        x = x1;
        y = y1;
        while (true)
          {
            Cell = CellFind (Page, x, y, false);
            if (Cell)
              {
                if (Mode == mVal)
                  s += StrCellFormatValue (&b, Cell, false);
                else if (Mode == mRaw)
                  s += StrCat (&b, Cell->Content);
                else // mSS => Content + Format
                  {
                    s += StrCat (&b, Cell->Content);
                    if (Cell->Format && Cell->Format [0])
                      {
                        StrCat (&b, '~');
                        s ++;
                        s += StrCat (&b, Cell->Format);
                      }
                  }
              }
            if (x < x2)
              {
                x++;
                StrCat (&b, '\t');
                s++;
              }
            else
              {
                x = x1;
                y++;
                if (y <= y2)
                  {
                    StrCat (&b, '\n');
                    s++;
                  }
                else
                  break;
              }
          }
        if (Buff)
          {
            *b = 0;
            if (Size)
              *Size = s;
            return Buff;
          }
        Buff = (char *) malloc (s + 1);
      }
  }

#define ContentFormatMax 1024

void AdjustContent (int x1, int y1, int xSize, int ySize, int xOffset, int yOffset, bool InsideOnly, _Point ClipSize)
  {
    _Cell *Cell;
    int x, y;
    char *c;
    char Res [ContentFormatMax], *r;
    int Pag, xRef, yRef;
    bool InClipboard;
    bool Digit;  // true if previous character was a digit (to filter out ##e#)
    //
    if (xOffset | yOffset)
      for (x = x1; x < x1 + xSize; x++)
        for (y = y1; y < y1 + ySize; y++)
          {
            Cell = CellFind (Page, x, y, false);
            if (Cell)
              if (Cell->Exp)
                {
                  c = Cell->Content;
                  r = Res;
                  Digit = false;
                  while (true)
                    {
                      if (*c == 0)   // End
                        break;
                      if (!Digit && StrGetCoord (&c, &Pag, &xRef, &yRef))   // cell reference
                        {
                          InClipboard = true;
                          if (InsideOnly)
                            if ( (Pag != Page) ||
                                 (CoordValue (xRef) < Clipboard.x) || (CoordValue (xRef) >= Clipboard.x + ClipSize.x) ||
                                 (CoordValue (yRef) < Clipboard.y) || (CoordValue (yRef) >= Clipboard.y + ClipSize.y) )
                              InClipboard = false;
                          if (Pag != Page)
                            {
                              StrColumn (&r, Pag);
                              StrCat (&r, '.');
                            }
                          if (CoordLocked (xRef))
                            StrCat (&r, '$');
                          else if (InClipboard && (Pag == Page))
                            xRef += xOffset;
                          StrColumn (&r, CoordValue (xRef));
                          if (CoordLocked (yRef))
                            StrCat (&r, '$');
                          else if (InClipboard && (Pag == Page))
                            yRef += yOffset;
                          StrRow (&r, CoordValue (yRef));
                        }
                      else
                        {
                          Digit = IsDigit (*c);
                          *r++ = *c++;
                        }
                    }
                  *r++ = 0;   // terminate result
                  WriteToCell (Res, Cell->Format, x, y);
                }
          }
  }

void BlockPaste (int BlockX1, int BlockY1, int BlockX2, int BlockY2)
  {
    int x, y;
    _Point Size;
    char *Clip;
    //
    if (ClipboardGet (fMain->Window))
      if (ClipboardData)
        {
          // Replace strange unnecessary characters
          Clip = StrReplace (ClipboardData, "\u2212", "-");   // swap out strange minus signs
          StrReplace (&Clip, "\u2796", "-"); //add all the hyphens
          //StrReplace (&Clip, "\u2018", "'");
          //StrReplace (&Clip, "\u2019", "'");
          //StrReplace (&Clip, "\u201C", "\"");
          //StrReplace (&Clip, "\u201D", "\"");
          // Convert to EOL-LF
          StrReplace (&Clip, "\r\n", "\n");   // CR+LF => LF
          StrReplace (&Clip, "\r", "\n");   // CR => LF
          x = BlockX1;
          y = BlockY1;
          while (true)
            {
              Size = TextToCells (x, y, Clip, false); //Clipboard.ContentFormat, false);
              if (!Clipboard.Cut)   // Clipboard was copied
                AdjustContent (x, y, Size.x, Size.y, x - Clipboard.x, y - Clipboard.y, false, Size);
              x += Size.x;
              if (x > BlockX2)
                {
                  x = BlockX1;
                  y += Size.y;
                  if (y > BlockY2)
                    break;
                }
            }
          if (Clipboard.Cut)
            AdjustContent (0, 0, pPage->SizeX, pPage->SizeY, BlockX1 - Clipboard.x, BlockY1 - Clipboard.y, true, Size);
          free (Clip);
          fMain->gSheet->Columns = Max (fMain->gSheet->Columns, BlockX1 + Size.x + 1);
          fMain->gSheet->Rows = Max (fMain->gSheet->Rows, BlockY1 + Size.y + 1);
      }
    Redraw = true;
  }
