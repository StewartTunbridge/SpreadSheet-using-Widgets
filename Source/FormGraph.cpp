//////////////////////////////////////////////////////////////////////////
//
// GRAPH FORM

typedef enum {gtBar, gtXY} _GraphType;

class _Graph: public _Container
  {
    public:
      int DataPage;
      _Point DataRaw [2];   // All Graph information
      _Point Data [2];   // Calculated Graph Data %%%%
      _GraphType GraphType;
      unsigned int IncludeColumns;
      bool IncludeZero;
      bool IncludeLines, IncludeMarkers;
      bool BarLabels;
      bool TopRowLabels;
      double RangeX [2];
      double RangeY [2];
      double XStart, YStart, XStep, YStep;
      int XnSteps, YnSteps;
      _Point PixelsOrigin;
      _Point PixelsPerStep;
      _Graph (_Container *Parent);
     ~_Graph ();
      //
      virtual void DrawCustom (void);
      //virtual bool ProcessEventCustom (_Event *Event, _Point Offset);   // returns true when actioned
      void DrawDiamond (int x, int y, int Colour);
      void DrawAxes (void);
      void DrawLegend (void);
      void DrawBarGraph (void);
      void DrawXYGraph (void);
  };

/*int StrRange (char **St, double *Range, bool First)
  {
    int Res;
    //
    Res = 0;
    if (First)
      Res += StrCat (St, "N/A");
    else
      {
        Res += DoubleToStr (St, Range [0], 0);
        Res += StrCat (St, "-");
        Res += DoubleToStr (St, Range [1], 0);
      }
    return Res;
  }*/

void SetRange (double *Range, double Value, bool *First)
  {
    if (Value < Range [0] || *First)
      Range [0] = Value;
    if (Value > Range [1] || *First)
      Range [1] = Value;
    *First = false;
  }

const int Scales [] = {1, 2, 5};

bool CalculateScale (double *Range, double *Start, double *Step, int *nSteps)
  {
    double span;
    double order;
    int scl;
    //
    span = Range [1] - Range [0];
    if (span == 0)
      return false;
    scl = 0;
    order = 1.0;
    while (span > 5 * Scales [scl] * order)
      if (++scl >= SIZEARRAY (Scales))
        {
          scl = 0;
          order *= 10;
        }
    while (span <= 5 * Scales [scl] * order)
      if (--scl < 0)
        {
          scl = SIZEARRAY (Scales) - 1;
          order /= 10;
        }
    *Step = Scales [scl] * order;
    *Start = floor (Range [0] / *Step) * *Step;
    *nSteps = span / *Step;
    while (*Start + *Step * *nSteps < Range [1])
      (*nSteps)++;
    return true;
  }

#define DiamondSize 3

void _Graph::DrawDiamond (int x, int y, int Colour)
  {
    DrawLine (x - 3, y, x, y - 3, Colour, 2);
    DrawLineTo (x + 3, y);
    DrawLineTo (x, y + 3);
    DrawLineTo (x - 3, y);
  }

#define SizeAxes 6
#define Gap 4

void _Graph::DrawAxes (void)
  {
    int i, a, b;
    int yf;
    char St [128], *s;//%%%%
    double v;
    Complex v_;
    _Cell *Cell;
    _Point ls;
    bool Stagger;
    _Font *Font;
    //
    PixelsOrigin = {0};
    Font = FontFind ();
    if (Font)
      {
        PixelsOrigin.x = Font->YAdvance * 5;
        PixelsOrigin.y = Font->YAdvance * 4;
      }
    PixelsPerStep.x = Max (1, (Rect.Width - PixelsOrigin.x - 8) / XnSteps);
    PixelsPerStep.y = Max (1, (Rect.Height - PixelsOrigin.y - 8) / YnSteps);
    //
    DrawLine (PixelsOrigin.x, Rect.Height - PixelsOrigin.y - YnSteps * PixelsPerStep.y, PixelsOrigin.x, Rect.Height - PixelsOrigin.y, cGray, 2);
    DrawLineTo (PixelsOrigin.x + XnSteps * PixelsPerStep.x, Rect.Height - PixelsOrigin.y);
    //
    // Find first valid data
    yf = Data [0].y;
    i = yf;
    while (true)
      {
        if (i >= Data [1].y)
          break;
        Cell = CellFind (DataPage, DataRaw [0].x, i, false);
        if (Cell)
          if (Cell->Content && !Cell->Error)
            {
              yf = i;
              break;
            }
        i++;
      }
    // X Labels
    a = PixelsOrigin.x;
    v = XStart;
    Stagger = false;
    for (i = 0; i <= XnSteps; i++)
      {
        s = St;
        if (GraphType == gtBar)
          {
            if (i < XnSteps)   // don't include last edge case
              if (BarLabels)
                {
                  Cell = CellFind (DataPage, DataRaw [0].x, Data [0].y + i, false);
                  if (Cell)
                    StrCellFormatValue (&s, Cell, true);
                }
              else
                StrRow (&s, Data [0].y + i);
            *s = 0;
            ls = TextSize (FontFind (), St);
            b = Rect.Height - PixelsOrigin.y + Gap;
            if (Stagger)
              b += FontFind ()->YAdvance;
            Stagger = !Stagger;
            TextOutAligned ({a, b, PixelsPerStep.x, 0}, St, aLeft, aTop);
            if (ls.x < PixelsPerStep.x)
              Stagger = false;
              //DebugAdd ("Wide Label");
          }
        else if (GraphType == gtXY)
          {
            if (Abs (v) < XStep)
              v = 0.0;
            if (i < XnSteps)
              {
                ComplexSet (v, 0, &v_);
                Cell = CellFind (DataPage, Data [0].x, yf, false);
                if (Cell)
                  StrValueFormat (&s, &v_, Cell->Format, true, true);
                *s = 0;
                ls = TextSize (FontFind (), St);
                b = Rect.Height - PixelsOrigin.y + Gap;
                if (Stagger)
                  b += FontFind ()->YAdvance;
                Stagger = !Stagger;
                TextOutAligned ({a, b, PixelsPerStep.x, 0}, St, aLeft, aTop);
                if (ls.x < PixelsPerStep.x)
                  Stagger = false;
              }
            DrawLine (a, Rect.Height - PixelsOrigin.y, a, Rect.Height - PixelsOrigin.y - YnSteps * PixelsPerStep.y, cGray, 1);
          }
        a += PixelsPerStep.x;
        v += XStep;
      }
    //
    // Y Labels
    a = Rect.Height - PixelsOrigin.y;
    v = YStart;
    Stagger = false;
    for (i = 0; i <= YnSteps; i++)
      {
        s = St;
        if (Abs (v) < YStep)
          v = 0.0;
        //DoubleToStr (&s, v, DigitsMax);
        ComplexSet (v, 0, &v_);
        DebugAddP ("Data", Data [0]); //####
        DebugAddP ("DataRaw", DataRaw [0]); //####
        //Cell = CellFind (DataPage, GraphType == gtBar ? Data [0].x : Data [0].x + 1, Data [0].y, false);
        Cell = CellFind (DataPage, GraphType == gtBar ? Data [0].x : Data [0].x + 1, yf, false);
        if (Cell)
          StrValueFormat (&s, &v_, Cell->Format, true, true);
        *s = 0;
        TextOutWrap ({0, a - PixelsPerStep.y, PixelsOrigin.x - Gap, 2 * PixelsPerStep.y}, St, aRight, aCenter);
        DrawLine (PixelsOrigin.x, a, PixelsOrigin.x + XnSteps * PixelsPerStep.x, a, cGray, 1);
        a -= PixelsPerStep.y;
        v += YStep;
      }
  }

void _Graph::DrawLegend (void)
  {
    int x0;
    char *St, *s;
    int len;
    int x;
    _Cell *Cell;
    //
    if (TopRowLabels)
      {
        // X axis legend
        x0 = DataRaw [0].x;
        if (GraphType == gtXY || BarLabels)
          {
            Cell = CellFind (DataPage, x0, DataRaw [0].y, false);
            if (Cell)
              {
                s = NULL;
                len = StrCellFormatValue (&s, Cell, true);
                s = St = (char *) malloc (len + 1);
                StrCellFormatValue (&s, Cell, true);
                *s = '\0';
                TextOutAligned ({PixelsOrigin.x, Rect.Height - PixelsOrigin.y + FontFind ()->YAdvance + Gap, Rect.Width, Rect.Height}, St, aLeft, aTop);
                free (St);
              }
            x0++;
          }
        // Y axis legend
        St = NULL;
        len = 0;
        while (true)
          {
            s = St;
            for (x = x0; x <= Data [1].x; x++)
              if (IncludeColumns & Bit [x - DataRaw [0].x])
                {
                  Cell = CellFind (DataPage, x, DataRaw [0].y, false);
                  if (Cell)
                    {
                      len += StrCat (&s, '\a');
                      len += StrCat (&s, (char) ('1' + x - x0));
                      len += StrCellFormatValue (&s, Cell, true);
                      len += StrCat (&s, "    ");
                    }
                }
            if (St)
              break;
            St = (char *) malloc (len + 1);
          }
        *s = 0;
        TextOutAligned ({PixelsOrigin.x, PixelsOrigin.y, Rect.Width, Rect.Height - PixelsOrigin.y}, St, aLeft, aBottom);
        free (St);
      }
  }

void _Graph::DrawBarGraph (void)
  {
    int n;
    int x, y;
    int x_, x__, ht, yAxis, yZero;
    int WidthBar;
    int Col;
    _Cell *Cell;
    //
    if (CalculateScale (RangeY, &YStart, &YStep, &YnSteps))
      {
        XnSteps = Data [1].y - Data [0].y + 1;
        DrawAxes ();
        DrawLegend ();
        n = Data [1].x - Data [0].x + 1;
        WidthBar = PixelsPerStep.x / (n + 1);
        yAxis = Rect.Height - PixelsOrigin.y;
        yZero = yAxis + YStart / YStep * PixelsPerStep.y;
        //DrawLine (0, yZero, Rect.Width, yZero, cRed, 3); //####
        //DrawLine (0, yAxis, Rect.Width, yAxis, cGreen, 3); //####
        x_ = PixelsOrigin.x;
        for (y = Data [0].y; y <= Data [1].y; y++)
          {
            x__ = x_;
            for (x = Data [0].x; x <= Data [1].x; x++)
              if (IncludeColumns & Bit [x - DataRaw [0].x])
                {
                  Col = ColourTable [x - Data [0].x + 1];
                  Cell = CellFind (DataPage, x, y, false);
                  if (Cell && Cell->Content && !Cell->Error)
                    if (YStart >= 0)   // Normal Graph, maybe chopped off at the knees
                      {
                        ht = (Cell->Value.Real - YStart) / YStep * PixelsPerStep.y;
                        DrawRectangle ({x__, yAxis - ht, WidthBar - 1, ht}, Col, Col, ColourAdjust (Col, 160));
                      }
                    else
                      {
                        ht = (Cell->Value.Real) / YStep * PixelsPerStep.y;
                        if (ht >= 0)   // Positive
                          DrawRectangle ({x__, yZero - ht, WidthBar - 1, ht}, Col, Col, ColourAdjust (Col, 160));
                        else   // Negative
                          DrawRectangle ({x__, yZero, WidthBar - 1, -ht}, Col, Col, ColourAdjust (Col, 160));
                      }
                  x__ += WidthBar;
                }
            x_ += PixelsPerStep.x;
          }
      }
  }

void _Graph::DrawXYGraph (void)
  {
    int x, y, x_, y_;
    int x0, y0;
    _Cell *Cell;
    //
    if (CalculateScale (RangeX, &XStart, &XStep, &XnSteps))
      if (CalculateScale (RangeY, &YStart, &YStep, &YnSteps))
        {
          DrawAxes ();
          DrawLegend ();
          for (x = Data [0].x + 1; x <= Data [1].x; x++)
            {
              x0 = -1;
              if (IncludeColumns & Bit [x - DataRaw [0].x])
                for (y = Data [0].y; y <= Data [1].y; y++)
                  {
                    Cell = CellFind (DataPage, Data [0].x, y, false);
                    if (Cell && !Cell->Error)
                      {
                        x_ = PixelsOrigin.x + (Cell->Value.Real - XStart) / XStep * PixelsPerStep.x;
                        Cell = CellFind (DataPage, x, y, false);
                        if (Cell && Cell->Content && !Cell->Error)
                          {
                            y_ = Rect.Height - PixelsOrigin.y - (Cell->Value.Real - YStart) / YStep * PixelsPerStep.y;
                            if (IncludeMarkers)
                              DrawDiamond (x_, y_, ColourTable [x - Data [0].x]);
                            if (IncludeLines)
                              if (x0 >= 0)
                                DrawLine (x0, y0, x_, y_, ColourTable [x - Data [0].x], 1);
                            x0 = x_;
                            y0 = y_;
                          }
                      }
                  }
            }
        }
  }

void _Graph::DrawCustom (void)
  {
    //char st [128], *s;
    int x, y;
    bool FirstX, FirstY;
    _Cell *Cell;
    //
    Data [0] = DataRaw [0];
    Data [1] = DataRaw [1];
    RangeX [0] = 0;
    RangeX [1] = 0;
    RangeY [0] = 0;
    RangeY [1] = 0;
    FirstX = FirstY = true;
    if (IncludeZero)
      FirstY = false;
    if (GraphType == gtBar && BarLabels)
      Data [0].x++;
    if (TopRowLabels)
      Data [0].y++;
    for (x = Data [0].x; x <= Data [1].x; x++)
      if (IncludeColumns & Bit [x - DataRaw [0].x])
        for (y = Data [0].y; y <= Data [1].y; y++)
          {
            Cell = CellFind (DataPage, x, y, false);
            if (Cell && Cell->Content && !Cell->Error)
              {
                if (x == Data [0].x && GraphType == gtXY)
                  SetRange (RangeX, Cell->Value.Real, &FirstX);
                else
                  SetRange (RangeY, Cell->Value.Real, &FirstY);
              }
          }
    /*s = st;
    StrCat (&s, GraphType == gtBar ? "Bar" : "XY");
    StrCat (&s, " Page ");
    NumToStr (&s, DataPage);
    StrCat (&s, " Range ");
    PointToStr (&s, Data [0]);
    StrCat (&s, '-');
    PointToStr (&s, Data [1]);
    StrCat (&s, "   ");
    StrRange (&s, RangeX, FirstX);
    StrCat (&s, ' ');
    StrRange (&s, RangeY, FirstY);
    *s = 0;
    DebugAdd (st);*/
    if (GraphType == gtBar)
      DrawBarGraph ();
    else if (GraphType == gtXY)
      DrawXYGraph ();
  }

//bool _Graph::ProcessEventCustom (_Event *Event, _Point Offset)
//  {
//  }

void GridGetSelected (_Point *s0, _Point *s1);

_Graph::_Graph (_Container *Parent): _Container (Parent, {0}, NULL)
  {
    GraphType = gtBar;
    IncludeZero = false;
    IncludeLines = true;
    IncludeMarkers = true;
    BarLabels = false;
    TopRowLabels = false;
    DataPage = Page;
    GridGetSelected (&DataRaw [0], &DataRaw [1]);
    IncludeColumns = 0xFFFFFFFF;
  }

_Graph::~_Graph ()
  {
  }

class _FormGraph: public _Form
  {
    public:
      _Graph *Graph;
      _MenuPopup *mGraph;
      _FormGraph (char *Title, _Point Posn);
     ~_FormGraph ();
  };

void ActionSaveGraph (char *Result, void *Parameter)
  {
    _FormGraph *fGraph;
    //
    fGraph = (_FormGraph *) Parameter;
    if (!BitmapSave (fGraph->Window, fGraph->Image, Result), 0)  // SaveScreenShot
      Beep ();
  }

void ActionToggleColumn (_SelectionList *List)
  {
    _FormGraph *fGraph;
    //
    fGraph = (_FormGraph *) List->Form;
    if (List->Selected >= 0)
      fGraph->Graph->IncludeColumns ^= Bit [List->Selected];
    fGraph->Graph->Invalidate ();
    List->Die ();

  }

void PopupColumnToggle (_Point Pos, _FormGraph *fGraph)
  {
    char *List, *l;
    int Len;
    int x;
    _SelectionList *Popup;
    //
    List = l = NULL;
    Len = 0;
    while (true)
      {
        x = fGraph->Graph->DataRaw [0].x;
        while (true)
          {
            Len += StrCat (&l, "    Column ");
            Len += StrCat (&l, x + 'A');
            if (x >= fGraph->Graph->DataRaw [1].x)
              break;
            Len += StrCat (&l, '\t');
            x ++;
          }
        if (List)
          break;
        l = List = (char *) malloc (Len + 1);
      }
    *l = 0;
    Popup  = new _SelectionList (fGraph->Container, {Pos.x, Pos.y, 0, 0}, List, (_Action) ActionToggleColumn);
    Popup->Ticked = fGraph->Graph->IncludeColumns;
    free (List);
  }

void ActionGraphMenu (_MenuPopup *Menu)
  {
    _FormGraph *fGraph;
    //
    fGraph = (_FormGraph *) Menu->Form;
    if (Menu->Selected >= 0)
      if (Menu->Selected < 2)   // Graph Type
        {
          fGraph->Graph->GraphType = (_GraphType) Menu->Selected;
          Menu->Dotted = Bit [Menu->Selected];
          fGraph->Graph->Invalidate ();
        }
      else if (Menu->Selected == 2)   // Bar Labels
        {
          Menu->Ticked ^= Bit [2];   // Toggle flag in Menu
          fGraph->Graph->BarLabels = Menu->Ticked & Bit [2];
          fGraph->Graph->Invalidate ();
        }
      else if (Menu->Selected == 3)   // Top Row Labels
        {
          Menu->Ticked ^= Bit [3];   // Toggle flag in Menu
          fGraph->Graph->TopRowLabels = Menu->Ticked & Bit [3];
          fGraph->Graph->Invalidate ();
        }
      else if (Menu->Selected == 4)   // Include 0.0
        {
          Menu->Ticked ^= Bit [4];   // Toggle flag in Menu
          fGraph->Graph->IncludeZero = Menu->Ticked & Bit [4];
          fGraph->Graph->Invalidate ();
        }
      else if (Menu->Selected == 5)   // Include Lines
        {
          Menu->Ticked ^= Bit [5];   // Toggle flag in Menu
          fGraph->Graph->IncludeLines = Menu->Ticked & Bit [5];
          fGraph->Graph->Invalidate ();
        }
      else if (Menu->Selected == 6)   // Include Markers
        {
          Menu->Ticked ^= Bit [6];   // Toggle flag in Menu
          fGraph->Graph->IncludeMarkers = Menu->Ticked & Bit [6];
          fGraph->Graph->Invalidate ();
        }
      else if (Menu->Selected == 7)   // Toggle Columns
        PopupColumnToggle ({Menu->SelectionList->Rect.x, Menu->SelectionList->Rect.y}, fGraph);
      else if (Menu->Selected == 8)   // Export
        FileSelect ("Save Graph Image (bmp)", ".bmp", true, ActionSaveGraph, fGraph);
  }

_FormGraph::_FormGraph (char *Title, _Point Posn): _Form (Title, {Posn.x, Posn.y, 400, 400}, waAlwaysOnTop | waResizable)
  {
    Container->Colour = cWhite;
    Container->ColourGrad = -1;
    Container->FontSet (NULL); //, 14);
    Graph = new _Graph (Container);
    mGraph = new _MenuPopup (Container, {0}, "   Bar\t_   XY\t   Left Column Labels (Bar)\t   Top Row Labels\t   Include 0.0\t   Add Lines\t_   Add Markers\t_Toggle Columns\tExport", (_Action) ActionGraphMenu);
    mGraph->Dotted = Bit [Graph->GraphType];
    if (Graph->IncludeLines)
      mGraph->Ticked = Bit [5] | Bit [6];   // Add Lines
  }

_FormGraph::~_FormGraph ()
  {
  }

