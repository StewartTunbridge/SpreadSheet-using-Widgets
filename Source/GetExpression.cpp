//////////////////////////////////////////////////////////////////////////
//
// GET EXPRESSION
//
//////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////
//
// STRING SUPPORTING FUNCTIONS

bool StrMatchChar (char **Str, char Target)
  {
    StepSpace (Str);
    if (**Str == Target)
      {
        (*Str)++;
        return true;
      }
    return false;
  }

bool StrMatchWord (char **Str, char *Target)
  {
    int Index;
    char c;
    bool Alpha;
    //
    Alpha = IsAlpha (*Target);
    StepSpace (Str);
    Index = 0;
    while (true)
      {
        c = (*Str) [Index];
        if (Target [Index] == 0)   // matched all Target
          {
            if (Alpha)
              if (IsAlpha (c))   // word is longer than Target
                return false;
            (*Str) += Index;   // Found, step past word
            return true;
          }
        if (UpCase (c) != UpCase (Target [Index]))
          return false;
        Index++;
      }
  }

bool IsLocked (char **St)
  {
    if (**St == '$')
      {
        (*St)++;
        return true;
      }
    return false;
  }

// Read coordinates from String
// Zero based: ie "A1" is (0, 0)
// If locked (with a '$', Value becomes (-1 - Value)

#define CoordLocked(Val) (Val<0)
#define CoordValue(Val) (Val<0?(-(Val+1)):Val)

bool StrGetCoordAlpha (char **St, int *Val)
  {
    bool Locked;
    bool ok;
    //
    ok = false;
    Locked = IsLocked (St);
    *Val = -1;
    while (IsAlpha (**St))
      {
        *Val = ((*Val + 1) * 26) + (UpCase (**St) - 'A');
        (*St)++;
        ok = true;
      }
    if (Locked)
      *Val = -1 - *Val;
    return ok;
  }

bool StrGetCoordNumeric (char **St, int *Val)
  {
    bool Locked;
    bool ok;
    //
    ok = false;
    Locked = IsLocked (St);
    *Val = 0;
    while (IsDigit (**St))
      {
        *Val = (*Val * 10) + (**St - '0');
        (*St)++;
        ok = true;
      }
    if (*Val > 0)
      (*Val)--;   // convert to zero based value
    else
      ok = false;
    if (Locked)
      *Val = -1 - *Val;
    return ok;
  }

bool StrGetCoord (char **St, int *Pag, int *Col, int *Row)   // -ve if locked
  {
    char *StOld;
    bool ok;
    //
    ok = false;
    *Pag = Page;
    StOld = *St;
    ok = StrGetCoordAlpha (St, Col);
    if (ok)
      if (**St == '.')   // This was a Page reference
        {
          (*St)++;
          *Pag = CoordValue (*Col);
          ok = StrGetCoordAlpha (St, Col);
        }
    if (ok)
      ok = StrGetCoordNumeric (St, Row);
    if (ok)
      return true;   // so all good
    *St = StOld;   // otherwise restore pointer
    return false;
  }


//////////////////////////////////////////////////////////////////////////
//
// CALCULATION SUPPORT

longint GCD (longint a, longint b)
  {
    longint r, c;
    //
    if (a < b)
      {
        c = a; a = b; b = c;
      }
    while (true)
      {
        if (b == 0)
          return 0;
        r = a % b;
        if (r == 0)
          return b;
        a = b;
        b = r;
      }
  }

longint LCM (longint a, longint b)
  {
    longint c;
    //
    c = GCD (a, b);
    if (c == 0)
      return 0;
    return Abs (a * b) / c;
  }

bool RealToRational (double Val, int *Num, int *Den, double Error)
  {
    bool Neg;
    longint i;
    int n0, n1;
    int d0, d1;
    double Val_;
    //
    Neg = (Val < 0.0);
    Val = Abs (Val) + Error / 2.0;
    i = Val;
    Val = Val - i;
    if (Val <= Error)
      {
        *Num = i;
        *Den = 1;
      }
    else
      {
        n0 = 0;
        n1 = 1;
        d0 = d1 = 1;
        //
        while (true)
          {
            Val_ = (double) (n0 + n1) / (double) (d0 + d1);
            if (Abs (Val_ - Val) <= Error) //(Err / Val < Error)
              break;
            if (Val_ > Val)
              {
                n1 += n0;
                d1 += d0;
              }
            else
              {
                n0 += n1;
                d0 += d1;
              }
            if (d0 + d1 > 1000000)
              return false;
          }
        *Den = d0 + d1;
        *Num = i * *Den + (n0 + n1);
      }
    if (Neg)
      *Num = -*Num;
    return true;
  }

int FloatScale (double *Val, int Step)
  {
    double p;
    int e;
    //
    p = Power10 (Step);
    e = 0;
    if (*Val != 0.0)
      {
        while (Abs (*Val) >= p)
          {
            *Val /= p;
            e++;
          }
        while (Abs (*Val) < 1.0)
          {
            *Val *= p;
            e--;
          }
      }
    return e;
  }

// Date Functions

const char * const MonthName [] = {"???", "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec", NULL};

bool LeapYear (int Year)
  {
    if ((Year % 4) == 0)
      if (((Year % 100) != 0) || ((Year % 400) == 0))
        return true;
    return false;
  }

int DaysInYear (int Year)
  {
    if (LeapYear (Year))
      return 366;
    return 365;
  }

int DaysInMonth (int Year, int Month)
  {
    int Res;
    //
    // {31, (isLeap(year) ? 29 : 28), 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    Res = 0;
    if (Month > 0 && Month <= 12)
      if (Month == 2)   // Feb
        if (LeapYear (Year))
          Res = 29;
        else
          Res = 28;
      else
        {
          Res = 30;
          if (Bit [Month - 1] & 0xAD5) // 0b 1010 1101 0101)
            Res++;
        }
    return Res;
  }

void DaysToDate (double Value, int *Year, int *Month, int *Day)
  {
    int DaysLeft;
    int d ;
    //
    *Year = 1900;
    *Month = 1;
    *Day = 1;
    DaysLeft = floor (Value) - 1;
    while (DaysLeft <= 0)
      {
        (*Year)--;
        DaysLeft += DaysInYear (*Year);
      }
    while ((d = DaysInYear (*Year)) < DaysLeft)
      {
        DaysLeft -= d;
        (*Year)++;
      }
    while ((d = DaysInMonth (*Year, *Month)) < DaysLeft)
      {
        DaysLeft -= d;
        (*Month)++;
      }
    *Day = DaysLeft;
  }

bool DateToDays (double *Res, int Year, int Month, int Day)
  {
    int Days;
    int y, m, d;
    bool OK;
    //
    OK = false;
    Days = 1;
    if ((Year >= 1900) && (Year <= 2999))
      if ((Month >= 1) && (Month <= 12))
        if ((Day >= 1) && (Day <= 31))
          {
            for (y = 1900; y < Year; y++)
              Days += DaysInYear (y);
            for (m = 1; m < Month; m++)
              Days += DaysInMonth (Year, m);
            d = DaysInMonth (Year, Month);
            if (Day <= d)
              {
                Days += Day;
                OK = true;
              }
            else
              Days += d;
          }
    *Res = Days;
    return OK;
  }

bool DaysAddMonth (double *Days, int Delta)
  {
    int Year, Month, Day;
    //
    DaysToDate (*Days, &Year, &Month, &Day);
    Month += Delta;
    while (Month > 12)
      {
        Year++;
        Month -= 12;
      }
    while (Month < 1)
      {
        Year--;
        Month += 12;
      }
    return DateToDays (Days, Year, Month, Day);
  }

bool TimeToReal (double *Res, word Hour, word Min, word Sec)
  {
    if (Hour < 24)
      if ((Min < 60))
        if ((Sec < 60))
          {
            *Res = ((((Sec / 60.0) + Min) / 60.0) + Hour) / 24.0;
            return true;
          }
    return false;
  }

void RealToTime (double Value, int *Days, word *Hour, word *Min, word *Sec)
  {
    Value = Abs (Value) + (0.5 / 24.0 / 60.0 / 60.0);   // round to nearest second
    *Days = Value;
    Value = frac (Value) * 24;
    *Hour = Value;
    Value = frac (Value) * 60;
    *Min = Value;
    Value = frac (Value) * 60;
    *Sec = Value;
  }

int StrSearchMonth (char **St)
  {
    int i, j;
    char c;
    //
    StepSpace (St);
    i = 1;
    j = 0;
    while (true)
      {
        if (MonthName [i] == NULL)   // Not Found
          return -1;
        c = MonthName [i] [j];
        if (c == 0)   // Found
          {
            (*St) += j;
            return i;
          }
        if (UpCase ((*St) [j]) == UpCase (c))
          j++;
        else
          {
            i++;
            j = 0;
          }
      }
  }

// Test for date format:
//   0=NotDate
//   1=2020-10-20 | 2020-Oct-20
//   2=20/10/2020 | 20 Oct 2020
//   3=Oct 20 2020
//   4 =20-10-2020
//   Separator can be: spaces - /
//
int StrGetDate (char **St, double *Res)
  {
    char *StOld;
    int n [3];
    int i;
    byte mf;
    //
    StOld = *St;
    mf = 0;
    i = 0;
    while (true)
      {
        StepSpace (St);
        if (IsDigit (**St))
          n [i] = StrGetNum (St);
        else
          {
            n [i] = StrSearchMonth (St);
            mf |= Bit [i];
          }
        if (i >= SIZEARRAY (n) - 1)
          break;
        StepSpace (St);
        if ((**St == '-') || (**St == '/') || (**St == ','))
          (*St)++;
        i++;
      }
    if (mf == 0)
      {
        if (DateToDays (Res, n [0], n [1], n [2]))
          return 1;
        if (DateToDays (Res, n [2], n [1], n [0]))  // dd/mm/yyyy
          return 4;
      }
    else if ((mf == 0x01) && (DateToDays (Res, n [2], n [0], n [1])))   // mmm x x
      return 3;
    else if ((mf == 0x02) && (DateToDays (Res, n [2], n [1], n [0])))   // x mmm x
      return 2;
    *St = StOld;
    return 0;
  }

char *StrGetString (char **St)
  {
    char *Res, *r;
    char *s;
    int i;
    byte c;
    int p, x, y;
    _Cell *Cell;
    //
    StepSpace (St);
    s = *St;
    Res = r = NULL;
    if (StrMatchChar (St, '"'))   // constant string
      {
        s = *St;
        i = 0;
        while (true)
          {
            c = **St;
            (*St)++;
            if ((c < ' ') || (c & 0x80))   // end of field, no terminator %%%% suport UNICODE UYF-8
              {
                StrError = true;
                return NULL;
              }
            if (c == '"')   // string complete
              {
                if (r)
                  {
                    *r = 0;   // terminate string
                    return Res;
                  }
                r = Res = (char *) malloc (i + 1);
                *St = s;
                i = 0;
              }
            else
              i += StrCat (&r, c);
          }
      }
    else
      if (StrGetCoord (St, &p, &x, &y))
        {
          Cell = CellFind (p, CoordValue (x), CoordValue (y), false);
          if (Cell)
            if (!Cell->Exp)   // string value
              {
                Res = NULL;
                StrAssignCopy (&Res, Cell->Content);
                return Res;
              }
          *St = s;   // no string so go back
        }
    return NULL;
  }

void GetExpression (char **Expression, Complex *Result, const Complex *X);
void GetValue (char **Expression, Complex *Result, const Complex *X);

void SearchForTarget (int *cx, int *cy, int dx, int dy, char *Target)
  {
    _Cell *Cell;
    //
    if (Target)
      while (true)
        {
          Cell = CellFind (Page, *cx,*cy, false);
          if (!Cell)
            break;
          if (!Cell->Content)
            break;
          if (!Cell->Content [0])
            break;
          if (StrSame_ (Cell->Content, Target))   // found
            return;
          *cx += dx;
          *cy += dy;
        }
    StrError = true;
  }

void GetReferenceOffset (char **Expression, int *cx, int *cy, const Complex *X)
  {
    int cx0;
    Complex v;
    char *Target;
    //
    cx0 = *cx;
    Target = StrGetString (Expression);
    if (Target)
      SearchForTarget (cx, cy, +1, 0, Target);
    else
      {
        GetExpression (Expression, &v, X);
        *cx += Round (v.Real);
      }
    free (Target);
    Target = NULL;
    StepSpace (Expression);
    if (!StrMatchChar (Expression, ','))
      StrError = true;
    Target = StrGetString (Expression);
    if (Target)
      SearchForTarget (&cx0, cy, 0, +1, Target);
    else
      {
        GetExpression (Expression, &v, X);
        *cy += Round (v.Real);
      }
    free (Target);
  }

typedef struct
  {
    Complex Count;
    Complex Sum;
    Complex SumSq;
  } _BlockResult;

void BlockSum (char **Expression, _BlockResult *BlockResult)
  {
    int x, y;
    int pag1, pag2, x1, y1, x2, y2;
    _Cell *Cell;
    char *Exp, *e;
    Complex Val, X;
    bool OK;
    //
    OK = false;
    Exp = NULL;
    if (StrMatchChar (Expression, '('))
      {
        StepSpace (Expression);
        if (StrGetCoord (Expression, &pag1, &x1, &y1))
          if (StrMatchChar (Expression, ','))
            {
              StepSpace (Expression);
              if (StrGetCoord (Expression, &pag2, &x2, &y2))
                {
                  if (StrMatchChar (Expression, ','))   // Expression (of X) expected
                    {
                      Exp = *Expression;
                      GetExpression (Expression, &Val, &C_1);   // step past expression
                    }
                  if (StrMatchChar (Expression, ')'))
                    if (pag1 == pag2)
                      {
                        OK = true;
                        ComplexSet (0.0, 0.0, &BlockResult->Count);
                        ComplexSet (0.0, 0.0, &BlockResult->Sum);
                        ComplexSet (0.0, 0.0, &BlockResult->SumSq);
                        for (y = CoordValue (y1); y <= CoordValue (y2); y++)
                          for (x = CoordValue (x1); x <= CoordValue (x2); x++)
                            {
                              Cell = CellFind (pag1, x, y, false);
                              if (Cell)
                                if (Cell->Content)
                                  if (Cell->Content [0])
                                    if (Cell->Error)
                                      StrError = true;
                                    else
                                      {
                                        Val = X = Cell->Value;
                                        e = Exp;
                                        if (Exp)
                                          GetExpression (&e, &Val, &X);
                                        ComplexAdd (&BlockResult->Count, &C_1, &BlockResult->Count);
                                        ComplexAdd (&BlockResult->Sum, &Val, &BlockResult->Sum);
                                        ComplexMul (&Val, &Val, &Val);
                                        ComplexAdd (&BlockResult->SumSq, &Val, &BlockResult->SumSq);
                                      }
                            }
                      }
                }
            }
      }
    if (!OK)
      StrError = true;
  }

void StandardDeviation (char **Expression, Complex *Res, bool N)
  {
    _BlockResult BlockResult;
    Complex x;
    //
    BlockSum (Expression, &BlockResult);
    if (BlockResult.Count.Real < 2)
      StrError = true;
    else
      {
        // x = Sum^2 / N
        ComplexMul (&BlockResult.Sum, &BlockResult.Sum, &x);
        ComplexDiv (&x, &BlockResult.Count, &x);
        // Res = Sqrt (SumSq - x / (N-1))
        ComplexSub (&BlockResult.SumSq, &x, Res);
        if (!N)
          ComplexSub (&BlockResult.Count, &C_1, &BlockResult.Count);
        ComplexDiv (Res, &BlockResult.Count, Res);
        ComplexSqrt (Res, Res);
      }
  }

bool GetDate (char **Expression, double *Res, const Complex *X)
  {
    int Year, Month, Day;
    Complex temp;
    //
    if (StrMatchChar (Expression, '('))
      {
        GetExpression (Expression, &temp, X);
        Year = temp.Real;
        if (StrMatchChar (Expression, ','))
          {
            GetExpression (Expression, &temp, X);
            Month = temp.Real;
            if (StrMatchChar (Expression, ','))
              {
                GetExpression (Expression, &temp, X);
                Day = temp.Real;
                if (StrMatchChar (Expression, ')'))
                  if (DateToDays (Res, Year, Month, Day))
                    return true;
              }
          }
      }
    return false;
  }

bool GetTime (char **Expression, double *Res, const Complex *X)
  {
    word Hour, Min, Sec;
    Complex temp;
    //
    if (StrMatchChar (Expression, '('))
      {
        GetValue (Expression, &temp, X);
        Hour = temp.Real;
        if (StrMatchChar (Expression, ','))
          {
            GetValue (Expression, &temp, X);
            Min = temp.Real;
            if (StrMatchChar (Expression, ','))
              {
                GetValue (Expression, &temp, X);
                Sec = temp.Real;
                if (StrMatchChar (Expression, ')'))
                  if (TimeToReal (Res, Hour, Min, Sec))
                    return true;
              }
          }
      }
    return false;
  }

bool GetComplexFunction (char **Expression, Complex *Res, const Complex *X)
  {
    Complex temp1, temp2;
    bool OK;
    //
    OK = true;
    if (StrMatchWord (Expression, "Real"))
      {
        GetValue (Expression, Res, X);
        ComplexSet (Res->Real, 0, Res);
      }
    else if (StrMatchWord (Expression, "Imag"))
      {
        GetValue (Expression, Res, X);
        ComplexSet (Res->Imaginary, 0, Res);
      }
    else if (StrMatchWord (Expression, "Mod"))
      {
        GetValue (Expression, Res, X);
        ComplexCalcModArg (Res);
        ComplexSet (Res->Mod, 0, Res);
      }
    else if (StrMatchWord (Expression, "Arg"))
      {
        GetValue (Expression, Res, X);
        ComplexCalcModArg (Res);
        ComplexSet (Res->Arg, 0, Res);
      }
    else if (StrMatchWord (Expression, "Complex"))
      {
        OK = false;
        if (StrMatchChar (Expression, '('))
          {
            GetExpression (Expression, &temp1, X);
            if (StrMatchChar (Expression, ','))
              {
                GetExpression (Expression, &temp2, X);
                if (StrMatchChar (Expression, ')'))
                  {
                    ComplexSet (temp1.Real, temp2.Real, Res);
                    OK = true;
                  }
              }
          }
      }
    else if (StrMatchWord (Expression, "ComplexPolar"))
      {
        OK = false;
        if (StrMatchChar (Expression, '('))
          {
            GetExpression (Expression, &temp1, X);
            if (StrMatchChar (Expression, ','))
              {
                GetExpression (Expression, &temp2, X);
                if (StrMatchChar (Expression, ')'))
                  {
                    ComplexSetModArg (temp1.Real, temp2.Real, Res);
                    OK = true;
                  }
              }
          }
      }
    else
      return false;
    if (!OK)
      StrError = true;
    return true;
  }

bool GetBlockFunction (char **Expression, Complex *Res)
  {
    _BlockResult BlockResult;
    //
    if (StrMatchWord (Expression, "Count"))
      {
        BlockSum (Expression, &BlockResult);
        *Res = BlockResult.Count;
      }
    else if (StrMatchWord (Expression, "Sum"))
      {
        BlockSum (Expression, &BlockResult);
        *Res = BlockResult.Sum;
      }
    else if (StrMatchWord (Expression, "SumSquare"))
      {
        BlockSum (Expression, &BlockResult);
        *Res = BlockResult.SumSq;
      }
    else if (StrMatchWord (Expression, "Avg"))
      {
        BlockSum (Expression, &BlockResult);
        if (BlockResult.Count.Real == 0)
          StrError = true;
        else
          ComplexDiv (&BlockResult.Sum, &BlockResult.Count, Res);
      }
    else if (StrMatchWord (Expression, "StDev"))
      StandardDeviation (Expression, Res, false);
    else if (StrMatchWord (Expression, "StDevN"))
      StandardDeviation (Expression, Res, true);
    else
      return false;
    return true;
  }

bool GetMathsFunction (char **Expression, Complex *Res, const Complex *X)
  {
    Complex ln10;
    //
    if (StrMatchWord (Expression, "Sqrt"))
      {
        GetValue (Expression, Res, X);
        ComplexSqrt (Res, Res);
      }
    else if (StrMatchWord (Expression, "Sqr"))
      {
        GetValue (Expression, Res, X);
        ComplexMul (Res, Res, Res);
      }
    else if (StrMatchWord (Expression, "Ln"))
      {
        GetValue (Expression, Res, X);
        if (!ComplexLn (Res, Res))
          StrError = true;
      }
    else if (StrMatchWord (Expression, "Log"))
      {
        GetValue (Expression, Res, X);
        if (!ComplexLn (Res, Res))
          StrError = true;
        else
          {
            ComplexSet (10, 0, &ln10);
            ComplexLn (&ln10, &ln10);
            ComplexDiv (Res, &ln10, Res);
          }
      }
    else if (StrMatchWord (Expression, "Exp"))
      {
        GetValue (Expression, Res, X);
        ComplexExp (Res, Res);
      }
    else if (StrMatchWord (Expression, "Random"))
      {
        GetValue (Expression, Res, X);
        if ((int) Res->Real > 0)
          Res->Real = rand () % (int) Res->Real;
        else
          StrError = true;
        Res->Imaginary = 0.0;
      }
    else
      return false;
    return true;
  }

bool GetLogicalFunction (char **Expression, Complex *Res, const Complex *X)
  {
    Complex a, b;
    char *StTarget, *StObject;
    bool lt, gt, eq;
    bool BoolRes;
    //
    if (StrMatchWord (Expression, "If"))
      {
        if (StrMatchChar (Expression, '('))
          {
            BoolRes = false;
            StTarget = StrGetString (Expression);
            StObject = NULL;
            if (StTarget)   // string function
              {
                //DebugAddS ("StTarget", StTarget); //####
                if (StrMatchWord (Expression, "IN"))
                  {
                    StObject = StrGetString (Expression);
                    if (StrPos (StObject, StTarget))   // found
                      BoolRes = true;
                  }
                free (StTarget);
                free (StObject);
              }
            else   // numerical comparison
              {
                GetExpression (Expression, &a, X);
                lt = gt = eq = false;
                while (true)
                  {
                    if (StrMatchChar (Expression, '<'))
                      lt = true;
                    else if (StrMatchChar (Expression, '>'))
                      gt = true;
                    else if (StrMatchChar (Expression, '='))
                      eq = true;
                    else
                      break;
                  }
                GetExpression (Expression, &b, X);
                if (lt && (ComplexCompare (&a, &b) < 0))
                  BoolRes = true;
                if (gt && (ComplexCompare (&a, &b) > 0))
                  BoolRes = true;
                if (eq && (ComplexCompare (&a, &b) == 0))
                  BoolRes = true;
              }
            if (!StrMatchChar (Expression, ','))
              StrError = true;
            GetExpression (Expression, &a, X);
            if (!StrMatchChar (Expression, ','))
              StrError = true;
            GetExpression (Expression, &b, X);
            if (!StrMatchChar (Expression, ')'))
              StrError = true;
            if (BoolRes)
              *Res = a;
            else
              *Res = b;
          }
      }
    else
      return false;
    return true;
  }

bool IsINF (double Val)
  {
    return Val != 0 && Val + Val == Val;
  }

bool IsNAN (double Val)
  {
    return Val != Val;
  }

bool IsDifferent (double a, double b)
  {
    if (IsNAN (a) && IsNAN (b))
      return false;
    return a != b;
  }

double MultipyConsecutive (int a, int b)
  {
    double Res;
    //
    Res = 1.0;
    while (a <= b)
      {
        Res *= a;
        if (IsINF (Res))
          break;
        a++;
      }
    return Res;
  }

double Factorial (int n)
  {
    return MultipyConsecutive (1, n);
  }

double Permutation (int a, int b)
  {
    if (a < b)
      StrError = true;
    return MultipyConsecutive (a - b + 1, a);
  }

double Combination (int a, int b)
  {
    if (a < b)
      StrError = true;
    return MultipyConsecutive (a - b + 1, a) / MultipyConsecutive (1, b);
  }

double PowerInt (double Val, int n)
  {
    double Res;
    //
    Res = 1.0;
    while (n > 0)
      {
        Res *= Val;
        n--;
      }
    while (n < 0)
      {
        Res /= Val;
        n++;
      }
    return Res;
  }

double BinomialProbability (double p, int n, int r)
  {
    return Combination (n, r) * PowerInt (p, r) * PowerInt (1.0 - p, n - r);
  }

bool GetStatisticalFunction (char **Expression, Complex *Res, const Complex *X)
  {
    Complex a, b, c, d;
    int r;
    double x;
    int Num, Den;
    //
    if (StrMatchWord (Expression, "P"))
      {
        if (!StrMatchChar (Expression, '('))
          StrError = true;
        GetExpression (Expression, &a, X);
        if (!StrMatchChar (Expression, ','))
          StrError = true;
        GetExpression (Expression, &b, X);
        if (!StrMatchChar (Expression, ')'))
          StrError = true;
        ComplexSet (Permutation (Round (a.Real), Round (b.Real)), 0, Res);
      }
    else if (StrMatchWord (Expression, "C"))
      {
        if (!StrMatchChar (Expression, '('))
          StrError = true;
        GetExpression (Expression, &a, X);
        if (!StrMatchChar (Expression, ','))
          StrError = true;
        GetExpression (Expression, &b, X);
        if (!StrMatchChar (Expression, ')'))
          StrError = true;
        ComplexSet (Combination (Round  (a.Real), Round (b.Real)), 0, Res);
      }
    else if (StrMatchWord (Expression, "B"))
      {
        if (!StrMatchChar (Expression, '('))
          StrError = true;
        GetExpression (Expression, &a, X);
        if (a.Real < 0.0 || a.Real > 1.0)
          StrError = true;
        if (!StrMatchChar (Expression, ','))
          StrError = true;
        GetExpression (Expression, &b, X);
        if (!StrMatchChar (Expression, ','))
          StrError = true;
        GetExpression (Expression, &c, X);
        if (StrMatchChar (Expression, ','))   // 2nd r value given
          {
            GetExpression (Expression, &d, X);
            if (d.Real < c.Real)
              StrError = true;
            x = 0.0;
            r = Round (c.Real);
            while (true)
              {
                if (StrError)
                  break;
                x += BinomialProbability (a.Real, Round (b.Real), r);
                if (r++ >= d.Real)
                  break;
              }
          }
        else
          x = BinomialProbability (a.Real, Round (b.Real), Round (c.Real));
        if (!StrMatchChar (Expression, ')'))
          StrError = true;
        ComplexSet (x, 0, Res);
      }
    else if (StrMatchWord (Expression, "GCD"))
      {
        if (!StrMatchChar (Expression, '('))
          StrError = true;
        GetExpression (Expression, &a, X);
        if (!StrMatchChar (Expression, ','))
          StrError = true;
        GetExpression (Expression, &b, X);
        if (!StrMatchChar (Expression, ')'))
          StrError = true;
        ComplexSet (GCD (a.Real, b.Real), 0, Res);
      }
    else if (StrMatchWord (Expression, "LCM"))
      {
        if (!StrMatchChar (Expression, '('))
          StrError = true;
        GetExpression (Expression, &a, X);
        if (!StrMatchChar (Expression, ','))
          StrError = true;
        GetExpression (Expression, &b, X);
        if (!StrMatchChar (Expression, ')'))
          StrError = true;
        ComplexSet (LCM (a.Real, b.Real), 0, Res);
      }
    else if (StrMatchWord (Expression, "Rational"))
      {
        if (!StrMatchChar (Expression, '('))
          StrError = true;
        GetExpression (Expression, &a, X);
        if (!StrMatchChar (Expression, ','))
          StrError = true;
        GetExpression (Expression, &b, X);
        if (!StrMatchChar (Expression, ')'))
          StrError = true;
        if (!RealToRational (a.Real, &Num, &Den, b.Real))
          StrError = true;
        ComplexSet ((double) Num / (double) Den, 0, Res);
      }
    else
      return false;
    return true;
  }

const Complex Delta = {0.0000000001, 0.0, false, 0.0, 0.0};

void CalculateDelta (Complex *x, Complex *x_)
  {
    if ((x->Real == 0.0) && (x->Imaginary == 0.0))
      *x_ = Delta;
    else
      ComplexMul (x, (Complex *) &Delta, x_);
  }

bool GetExtendedFunction (char **Expression, Complex *Res, const Complex *X)
  {
    char *Exp, *e;
    Complex x0, x1;
    Complex y0, y1, z;
    Complex dx, dy;
    int Count;
    int n;
    bool SolveNewton;
    //
    if (StrMatchWord (Expression, "Solve"))
      {
        if (!StrMatchChar (Expression, '('))
          StrError = true;
        // Get the start of the expression and the initial value
        Exp = *Expression;
        GetExpression (Expression, Res, (Complex *) &Delta);
        if (!StrMatchChar (Expression, ','))
          StrError = true;
        SolveNewton = true;
        GetExpression (Expression, &x0, X);   // Get initial x0
        if (StrMatchChar (Expression, ','))   // Optional 2nd initial x1
          {
            GetExpression (Expression, &x1, X);
            SolveNewton = false;
          }
        if (!StrMatchChar (Expression, ')'))
          StrError = true;
        Count = 0;
        while (true)
          {
            if (StrError)
              break;
            if (SolveNewton)
              {
                // calculate dx
                CalculateDelta (&x0, &dx);
                // calculate x1
                ComplexAdd (&x0, &dx, &x1);
                // calculate y0, y1 & dy
                e = Exp;
                GetExpression (&e, &y0, &x0);
                e = Exp;
                GetExpression (&e, &y1, &x1);
                ComplexSub (&y1, &y0, &dy);   // dy = f(x1) - f(x0)
                // Check for infinity
                if (!ComplexOK (&y0))
                  StrError = true;
                // calculate new x0
                if ((dy.Real == 0.0) && (dy.Imaginary == 0.0))
                  StrError = true;   // flat function, can't solve
                else
                  {
                    ComplexMul (&y0, &dx, &z);   // x0 = x0 - (y0 * dx / dy) * Damper
                    ComplexDiv (&z, &dy, &z);
                    *Res = x0;
                    ComplexSub (&x0, &z, &x0);
                  }
              }
            else   // solve halving interval
              {
                e = Exp;
                GetExpression (&e, &y0, &x0);
                e = Exp;
                GetExpression (&e, &y1, &x1);
                if ((y0.Real > 0) == (y1.Real > 0))   // Both values same sign so ..
                  StrError = true;
                else
                  {
                    // Calculate midpoint x and evaluate y
                    ComplexSub (&x1, &x0, &dx);
                    ComplexDiv (&dx, &C_2, &dx);
                    ComplexAdd (&x0, &dx, &dx);
                    e = Exp;
                    GetExpression (&e, &dy, &dx);
                    if ((y0.Real > 0) == (dy.Real > 0))   // Both y0 and y have same sign so ..
                      x0 = dx;   // replace x0 with x
                    else
                      x1 = dx;   // replace x1 with x
                    *Res = x0;
                    if (x0.Real == x1.Real && x0.Imaginary == x1.Imaginary)   // Converged?
                      break;
                    ComplexSub (&x1, &x0, &z);
                  }
              }
            // Check for convergence
            if (Mod2 (&z) / Mod2 (&x0) < 1e-29)   // Converged?
              break;
            // check if broken
            if (++Count >= 1000)
              StrError = true;
          }
      }
    else if (StrMatchWord (Expression, "Integrate"))
      {
        if (!StrMatchChar (Expression, '('))
          StrError = true;
        // Get the function, the x bounds and the number of intervals
        Exp = *Expression;
        GetExpression (Expression, Res, (Complex *) &Delta);
        if (!StrMatchChar (Expression, ','))
          StrError = true;
        GetExpression (Expression, &x0, X);   // Get lower x bound
        if (!StrMatchChar (Expression, ','))
          StrError = true;
        GetExpression (Expression, &x1, X);   // Get higher x bound
        if (!StrMatchChar (Expression, ','))
          StrError = true;
        GetExpression (Expression, Res, NULL);   // Get number of intervals
        n = Res->Real;
        if (n == 0)
          n = 4;
        if (n & 1)
          StrError = true;
        if (!StrMatchChar (Expression, ')'))
          StrError = true;
        // dx = (x1 - x0) / n
        ComplexSub (&x1, &x0, &dx);
        ComplexDiv (&dx, Res, &dx);
        // Simpsons rule integration over n steps
        ComplexSet (0, 0, &y1);
        if (!StrError)
          for (Count = 0; Count <= n; Count++)
            {
              e = Exp;
              GetExpression (&e, &y0, &x0);
              if (Count & 1)   // odd interval(x4)
                ComplexMul (&y0, &C_4, &y0);
              else if ((Count > 0) && (Count < n))   // even inside interval (x2)
                ComplexMul (&y0, &C_2, &y0);
              ComplexAdd (&y0, &y1, &y1);   // accumulate weighted y value
              ComplexAdd (&x0, &dx, &x0);   // next x value
            }
        ComplexMul (&y1, &dx, &y1);
        ComplexDiv (&y1, &C_3, Res);
      }
    else if (StrMatchWord (Expression, "Differentiate"))
      {
        if (!StrMatchChar (Expression, '('))
          StrError = true;
        // Get the function and the x value
        Exp = *Expression;
        GetExpression (Expression, Res, (Complex *) &Delta);
        if (!StrMatchChar (Expression, ','))
          StrError = true;
        GetExpression (Expression, &x0, X);   // Get x
        if (!StrMatchChar (Expression, ')'))
          StrError = true;
        // Caluclate dx
        CalculateDelta (&x0, &dx);
        #ifdef Differentiate5
        // Five point differentiation: f(x-2h)-8f(x-h)+8f(x+h)-f(x+2h)
        ComplexSub (&x0, &dx, &x1);  // x-h
        ComplexSub (&x1, &dx, &x1);  // x-2h
        e = Exp;
        GetExpression (&e, &y0, &x1);  // y0 = f (x-2h)
        //
        ComplexSub (&x0, &dx, &x1);  // x-h
        e = Exp;
        GetExpression (&e, &z, &x1);  // f (x-h)
        ComplexMul (&z, &C_8, &z);  // 8*f (x-h)
        ComplexSub (&y0, &z, &y0);  // y0 -= 8*f (x-h)
        //
        ComplexAdd (&x0, &dx, &x1);  // x+h
        e = Exp;
        GetExpression (&e, &z, &x1);  // f (x+h)
        ComplexMul (&z, &C_8, &z);  // 8*f (x+h)
        ComplexAdd (&y0, &z, &y0);  // y0 += 8*f (x+h)
        //
        ComplexAdd (&x1, &dx, &x1);  // x+2h
        e = Exp;
        GetExpression (&e, &z, &x1);  // f (x+2h)
        ComplexSub (&y0, &z, &y0);  // y0 -= f (x+2h)
        //
        ComplexMul (&dx, &C_12, &z);  // 12 * h
        ComplexDiv (&y0, &z, Res);
        #else
        // calculate x0, x1, dx
        ComplexAdd (&x0, &dx, &x1);
        ComplexSub (&x0, &dx, &x0);
        ComplexSub (&x1, &x0, &dx);
        // calculate y0, y1 & dy
        e = Exp;
        GetExpression (&e, &y0, &x0);
        e = Exp;
        GetExpression (&e, &y1, &x1);
        ComplexSub (&y1, &y0, &dy);   // dy = f(x1) - f(x0)
        // calculate derivative: dy/dx
        ComplexDiv (&dy, &dx, Res);
        #endif
      }
    else
      return false;
    return true;
  }

bool GetTrigFunction (char **Expression, Complex *Res, const Complex *X)
  {
    if (StrMatchWord (Expression, "Pi"))
      ComplexSet (M_PI, 0, Res);
    else if (StrMatchWord (Expression, "Sin"))
      {
        GetValue (Expression, Res, X);
        if (!Radians)
          ComplexSet (Res->Real * M_PI / 180.0, 0, Res);
        ComplexSin (Res, Res);
      }
    else if (StrMatchWord (Expression, "Cos"))
      {
        GetValue (Expression, Res, X);
        if (!Radians)
          ComplexSet (Res->Real * M_PI / 180.0, 0, Res);
        ComplexCos (Res, Res);
      }
    else if (StrMatchWord (Expression, "Tan"))
      {
        GetValue (Expression, Res, X);
        if (!Radians)
          ComplexSet (Res->Real * M_PI / 180.0, 0, Res);
        ComplexTan (Res, Res);
      }
    else if (StrMatchWord (Expression, "ASin"))
      {
        GetValue (Expression, Res, X);
        ComplexArcSin (Res, Res);
        //ComplexSet (asin (Res->Real), 0, Res);
        if (!Radians)
          Res->Real *= 180.0 / M_PI;
      }
    else if (StrMatchWord (Expression, "ACos"))
      {
        GetValue (Expression, Res, X);
        ComplexArcCos (Res, Res);
        //ComplexSet (acos (Res->Real), 0, Res);
        if (!Radians)
          ComplexSet (Res->Real *= 180.0 / M_PI, 0, Res);;
      }
    else if (StrMatchWord (Expression, "ATan"))
      {
        GetValue (Expression, Res, X);
        ComplexArcTan (Res, Res);
        //ComplexSet (atan (Res->Real), 0, Res);
        if (!Radians)
          ComplexSet (Res->Real *= 180.0 / M_PI, 0, Res);;
      }
    else
      return false;
    return true;
  }

bool GetFinancialFunction (char **Expression, Complex *Res, const Complex *X)
  {
    Complex Val;
    double Principle, Rate;
    double Rate_;
    int n;
    //
    if (StrMatchWord (Expression, "Mortgage"))
      {
        if (!StrMatchChar (Expression, '('))
          StrError = true;
        GetExpression (Expression, &Val, X);
        Principle = Val.Real;
        if (!StrMatchChar (Expression, ','))
          StrError = true;
        GetExpression (Expression, &Val, X);
        Rate = Val.Real;
        if (!StrMatchChar (Expression, ','))
          StrError = true;
        GetExpression (Expression, &Val, X);
        n = Val.Real;
        if (!StrMatchChar (Expression, ')'))
          StrError = true;
        // P * r * (1 + r)^n / [(1 + r)^n - 1]
        Rate_ = PowerInt (1.0 + Rate, n);
        ComplexSet (Principle * Rate * Rate_ / (Rate_ - 1.0), 0.0, Res);
      }
    else
      return false;
    return true;
  }

bool GetFunction (char **Expression, Complex *Res, const Complex *X)
  {
    double DateTime;
    //
    if (StrMatchWord (Expression, "Abs"))
      {
        GetValue (Expression, Res, X);
        ComplexSet (Abs (Res->Real), Abs (Res->Imaginary), Res);
      }
    else if (StrMatchWord (Expression, "Int"))
      {
        GetValue (Expression, Res, X);
        ComplexSet (floor (Res->Real), floor (Res->Imaginary), Res);
      }
    else if (StrMatchWord (Expression, "Frac"))
      {
        GetValue (Expression, Res, X);
        ComplexSet (frac (Res->Real), frac (Res->Imaginary), Res);
      }
    else if (StrMatchWord (Expression, "Date"))
      {
        if (!GetDate (Expression, &DateTime, X))
          StrError = true;
        ComplexSet (DateTime, 0, Res);
      }
    else if (StrMatchWord (Expression, "Time"))
      {
        if (!GetTime (Expression, &DateTime, X))
          StrError = true;
        ComplexSet (DateTime, 0, Res);
      }
    else if (StrMatchWord (Expression, "AddMonth"))
      {
        if (!StrMatchChar (Expression, '('))
          StrError = true;
        GetValue (Expression, Res, X);
        DateTime = Res->Real;
        if (!StrMatchChar (Expression, ','))
          StrError = true;
        GetValue (Expression, Res, X);
        if (!StrMatchChar (Expression, ')'))
          StrError = true;
        DaysAddMonth (&DateTime, Res->Real);
        ComplexSet (DateTime, 0, Res);
      }
    else
      if (!GetMathsFunction (Expression, Res, X))
        if (!GetTrigFunction (Expression, Res, X))
          if (!GetComplexFunction (Expression, Res, X))
            if (!GetBlockFunction (Expression, Res))
              if (!GetExtendedFunction (Expression, Res, X))
                if (!GetStatisticalFunction (Expression, Res, X))
                  if (!GetFinancialFunction (Expression, Res, X))
                    if (!GetLogicalFunction (Expression, Res, X))
                      return false;
    //
    return true;
  }

void GetValue (char **Expression, Complex *Res, const Complex *X)
  {
    double x;
    int cp, cx, cy;
    _Cell *Cell;
    //
    StepSpace (Expression);
    // Process brackets
    if (StrMatchChar (Expression, '('))
      {
        GetExpression (Expression, Res, X);
        if (!StrMatchChar (Expression, ')'))
          StrError = true;
      }
    // Precess Negative
    else if (StrMatchChar (Expression, '-'))
      {
        GetValue (Expression, Res, X);
        ComplexMul (Res, &C_N1, Res);
      }
    // Precess Positive
    else if (StrMatchChar (Expression, '+'))
      GetValue (Expression, Res, X);
    // Process cell reference
    else if (StrGetCoord (Expression, &cp, &cx, &cy))
      {
        cx = CoordValue (cx);
        cy = CoordValue (cy);
        StepSpace (Expression);
        if (StrMatchChar (Expression, '['))   // indexed
          {
            GetReferenceOffset (Expression, &cx, &cy, X);
            if (!StrMatchChar (Expression, ']'))
              StrError = true;
          }
        Cell = CellFind (cp, cx, cy, false);
        ComplexSet (0.0, 0.0, Res);   // assume 0 if no cell
        if (Cell)
          {
            *Res = Cell->Value;
            StrError |= Cell->Error;
          }
        else
          StrError = true;
      }
    // Process number
    else if (StrMatchWord (Expression, "0x"))
      ComplexSet (StrGetHex (Expression), 0, Res);
    else
      {
        StrMatchChar (Expression, '$');   // absorb leading $
        if (IsDigit (**Expression) || (**Expression == '.'))
          {
            x = StrGetDouble (Expression);
            StepSpace (Expression);
            if (StrMatchWord (Expression, "i"))   // imaginary constant
              ComplexSet (0, x, Res);
            else
              ComplexSet (x, 0, Res);
          }
        // Process Function
        else if (!GetFunction (Expression, Res, X))
          // Imaginary unity
          if (StrMatchChar (Expression, 'i'))
            ComplexSet (0, 1, Res);
          // Variable reference, x
          else if (StrMatchChar (Expression, 'x'))
            {
              if (X)
                *Res = *X;
              else
                StrError = true;
            }
          else
            StrError = true;
      }
    while (true)
      {
        //StepSpace (Expression);
        if (!StrMatchChar (Expression, '!'))
          break;
        ComplexSet (Factorial (Round (Res->Real)), 0, Res);
      }
  }

void GetValuePower (char **Expression, Complex *Result, const Complex *X)
  {
    Complex temp;
    //
    StepSpace (Expression);
    GetValue (Expression, Result, X);
    while (true)
      {
        StepSpace (Expression);
        if (**Expression == 0)
          break;
        if (StrMatchChar (Expression, '^'))
          {
            GetValue (Expression, &temp, X);
            if (!ComplexPower (Result, &temp, Result))
              StrError = true;
          }
        else if (StrMatchWord (Expression, "<<"))
          {
            GetValue (Expression, &temp, X);
            ComplexSet ((long) Result->Real << (long) temp.Real, 0, Result);
          }
        else if (StrMatchWord (Expression, ">>"))
          {
            GetValue (Expression, &temp, X);
            ComplexSet ((long) Result->Real >> (long) temp.Real, 0, Result);
          }
        else
          break;
      }
  }

void GetValueMul (char **Expression, Complex *Result, const Complex *X)
  {
    Complex temp;
    //
    StepSpace (Expression);
    GetValuePower (Expression, Result, X);
    while (true)
      {
        StepSpace (Expression);
        if (**Expression == 0)
          break;
         if (StrMatchChar (Expression, '*'))
          {
            GetValuePower (Expression, &temp, X);
            ComplexMul (Result, &temp, Result);
          }
        else if (StrMatchChar (Expression, '/'))
          {
            GetValuePower (Expression, &temp, X);
            if (!ComplexDiv (Result, &temp, Result))
              StrError = true;
          }
        else if (StrMatchChar (Expression, '%'))
        //else if (StrMatchWord (Expression, "MOD")) this also works
          {
            GetValuePower (Expression, &temp, X);
            if (!ComplexMod (Result, &temp, Result))
              StrError = true;
          }
        else if (StrMatchChar (Expression, '&'))
          {
            GetValueMul (Expression, &temp, X);
            ComplexSet ((long) Result->Real & (long) temp.Real, 0, Result);
          }
        else
          break;
      }
  }

void GetExpression (char **Expression, Complex *Result, const Complex *X)
  {
    Complex temp;
    //
    StepSpace (Expression);
    GetValueMul (Expression, Result, X);
    while (true)
      {
        StepSpace (Expression);
        if (**Expression == 0)
          break;
        if (StrMatchChar (Expression, '+'))
          {
            GetValueMul (Expression, &temp, X);
            ComplexAdd (Result, &temp, Result);
          }
        else if (StrMatchChar (Expression, '-'))
          {
            GetValueMul (Expression, &temp, X);
            ComplexSub (Result, &temp, Result);
          }
        else if (StrMatchWord (Expression, "||"))
          {
            GetValueMul (Expression, &temp, X);
            ComplexSet ((long) Result->Real ^ (long) temp.Real, 0, Result);
          }
        else if (StrMatchChar (Expression, '|'))
          {
            GetValueMul (Expression, &temp, X);
            ComplexSet ((long) Result->Real | (long) temp.Real, 0, Result);
          }
        else
          break;
      }
  }

