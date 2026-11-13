//////////////////////////////////////////////////////////////////////////
//
// FORMATTING

#define Round(x) (floor (x + 0.5))

const char *RadiansValueNames [] = {"Degrees", "Radians", NULL};

int StrColumn (char **St, int x)
  {
    char Res [32];
    int i;
    //
    // Make result backwards
    i = 32;
    Res [--i] = 0;   // terminate string
    while (true)
      {
        Res [--i] = (x % 26) + 'A';
        x = x / 26;
        if (x == 0)
          break;
        x--;   // greater order digits start at 'A' not 'B'
      }
    return StrCat (St, &Res [i]);
  }

int StrRow (char **St, int y)
  {
    return IntToStr (St, y + 1);
  }

int YearToStr (char **St, int Year)
  {
    int i;
    //
    if (Year >= 0)
      return IntToStr (St, Year, 4 | DigitsZeros);
    i = 0;
    i += IntToStr (St, -Year);
    i += StrCat (St, " BC");
    return i;
  }

bool RealToRationalStr (char **St, int *Size, double Value, int Format, const char *Tag)
  {
    int i, n, d;
    //
    if (RealToRational (Value, &n, &d, 1e-14))
      {
        // Negative value
        if (n < 0)
          {
            *Size += StrCat (St, '-');
            n = -n;
          }
        if (Tag)   // If a Tag string is used, only support improper fractions
          Format = 2;
        if (n == 0)
          *Size += StrCat (St, '0');
        else
          {
            // Proper fraction
            if (Format <= 1)   // Proper Fraction
              {
                i = n / d;
                if (i > 0)
                  {
                    *Size += IntToStr (St, i);
                    n -= i * d;
                    if (n)
                      *Size += StrCat (St, ' ');
                  }
              }
            if (n)
              {
                if (n != 1 || Tag == NULL)
                  *Size += IntToStr (St, n);
                if (Tag)
                  *Size += StrCat (St, Tag);
                if (d != 1 || Format >= 3)
                  {
                    if (Format >= 3)   // Ratio
                      *Size += StrCat (St, ':');
                    else   // Fraction
                      *Size += StrCat (St, '/');
                    *Size += IntToStr (St, d);
                  }
              }
          }
        return true;
      }
    return false;
  }

int ComplexToStr (char **St, Complex *Value, int Decimals)
  {
    int Res;
    //
    Res = DoubleToStr (St, Value->Real, Decimals);
    if (Value->Imaginary != 0)
      {
        Res += StrCat (St, " + ");
        Res += DoubleToStr (St, Value->Imaginary,Decimals );//DigitsMax
        Res += StrCat (St, 'i');
      }
    return Res;
  }

bool NumStrToStrFirst;

int NumStrToStr (char **Dest, longint Num, const char *Tag)
  {
    int Res;
    //
    Res = 0;
    if (Num)
      {
        if (!NumStrToStrFirst)
          Res += StrCat (Dest, ' ');
        NumStrToStrFirst = false;
        Res += NumToStr (Dest, Num);
        Res += StrCat (Dest, ' ');
        Res += StrCat (Dest, Tag);
        if (Num > 1)
          Res += StrCat (Dest, 's');
      }
    return Res;
  }

int RealFormattedToStr (char **St, double Value, char Format, word Decimals, bool Imaginary)
  {
    int Size;
    int e;
    int Year, Month, Day;
    word Hour, Min, Sec;
    int Value_;
    //
    Size = 0;
    if (Format != 'S' && Format != 'E')
      if (Value)
        if (Abs (Value) >= 1e18 || Abs (1 / Value) >= 1e18)
          Format = 'S';
    switch (Format)
      {
        case 'F': // Floating point
          Size += DoubleToStr (St, Value, Decimals);
          break;
        case 'S': // Scientific notation
          e = FloatScale (&Value, 1);
          Size += DoubleToStr (St, Value, Decimals);
          if (e)
            {
              StrCat (St, 'e');
              Size++;
              Size += IntToStr (St, e);
            }
          break;
        case 'E': // Engineering notation
          e = FloatScale (&Value, 3);
          Size += DoubleToStr (St, Value, Decimals);
          StrCat (St, 'e');
          Size++;
          Size += IntToStr (St, e * 3);
          break;
        case 'C': // Currency
          if (Value < 0.0)
            {
              StrCat (St, '-');
              Size++;
              Value = -Value;
            }
          StrCat (St, '$');
          Size++;
          if ((Decimals & DigitsMax) == DigitsMax)
            Decimals = (Decimals & ~DigitsMax) | 2;
          Size += DoubleToStr (St, Value, Decimals);
          break;
        case 'N': // perceNtage
          Size += DoubleToStr (St, Value * 100, Decimals < 0 ? 0 : Decimals);
          Size += StrCat (St, '%');
          break;
        case 'R': // Rational
          if (RealToRationalStr (St, &Size, Value, Decimals, Imaginary ? "i" : NULL))
            Imaginary = false;
          else
            Size += DoubleToStr (St, Value, Decimals);
          break;
        case 'H': // Hex
          Size += StrCat (St, "0x");
          if ((Decimals & DigitsMax) == DigitsMax)
            Decimals = 0;
          else
            Decimals |= DigitsZeros;
          Size += NumToHex (St, Round (Value), Decimals);
          break;
        case 'D': // Date
          DaysToDate (Value, &Year, &Month, &Day);
          if (Decimals <= 1)
            {
              Size += YearToStr (St, Year);
              StrCat (St, '-');
              NumToStr (St, Month, 2 | DigitsZeros);
              StrCat (St, '-');
              NumToStr (St, Day, 2 | DigitsZeros);
              Size += 6;
            }
          else if (Decimals <= 2)
            {
              NumToStr (St, Day, 2 | DigitsZeros);
              StrCat (St, ' ');
              Size += StrCat (St, (char *) MonthName [Month]);
              StrCat (St, ' ');
              Size += YearToStr (St, Year);
              Size += 4;
            }
          else if (Decimals <= 3)
            {
              Size += StrCat (St, (char *) MonthName [Month]);
              Size += StrCat (St, ' ');
              Size += NumToStr (St, Day, 2 | DigitsZeros);
              Size += StrCat (St, ' ');
              Size += YearToStr (St, Year);
            }
          else   // "d4"
            {
              Size += NumToStr (St, Day, 2 | DigitsZeros);
              Size += StrCat (St, '/');
              Size += NumToStr (St, Month, 2 | DigitsZeros);
              Size += StrCat (St, '/');
              Size += YearToStr (St, Year);
            }
          break;
        case 'T': // Time
          if (Value < 0)
            Size += StrCat (St, '-');
          else if (Value == 0)
            Size += StrCat (St, '0');
          RealToTime (Value, &Day, &Hour, &Min, &Sec);
          NumStrToStrFirst = true;
          Size += NumStrToStr (St, Day, "day");
          if (Decimals <= 1)
            {
              if (!NumStrToStrFirst)
                Size += StrCat (St, ' ');
              Size += NumToStr (St, Hour, 2 | DigitsZeros);
              Size += NumToStr (St, Min, 2 | DigitsZeros);
              Size += StrCat (St, ':');
              Size += NumToStr (St, Sec, 2 | DigitsZeros);
            }
          else
            {
              Size += NumStrToStr (St, Hour, "hour");
              Size += NumStrToStr (St, Min, "min");
              Size += NumStrToStr (St, Sec, "sec");
            }
          break;
        case 'A': // ASCII
          Value_ = Value;
          if (Value_ < 0x100)
            {
              Size += StrCat (St, '\"');
              Size += StrCat (St, (char) Value_);
              Size += StrCat (St, '\"');
            }
          else
            Size += StrCat (St, "Error");
      }
    if (Imaginary)
      Size += StrCat (St, 'i');
    return Size;
  }

int StrValueFormat (char **St, Complex *Value, char *Format, bool Exp, bool AllowCommas)
  {
    bool NumFormat;
    int Size;
    char format;
    word Decimals;
    bool Polar;
    bool BlockZero;
    double Re, Im;
    char c, *cp;
    //
    Size = 0;
    format = 'F';
    Decimals = DigitsMax;
    Radians = false;
    Polar = false;
    BlockZero = false;
    // Process Format
    NumFormat = false;
    if (Format)   // Format specified
      {
        cp = Format;
        while (*cp)
          {
            StepSpace (&cp);
            if (IsDigit (*cp))
              {
                Decimals = StrGetNum (&cp);
                NumFormat = true;
              }
            else
              {
                c = UpCase (*cp++);
                if (StrPos ("FSECHRNDT", c))
                  {
                    format = c;
                    NumFormat = true;
                  }
                else if (c == 'P')
                  Polar = true;
                else if (c == 'X')
                  Radians = true;
                else if (c == ',' && AllowCommas)
                  Decimals |= DigitsCommas;
                else if (c == 'Z')
                  BlockZero = true;
              }
          }
      }
    if (NumFormat | Exp)   // Numeric
      {
        if (IsINF (Value->Real) || IsINF (Value->Imaginary))
          Size += StrCat (St, "** INF");
        else if (IsNAN (Value->Real) || IsNAN (Value->Imaginary))
          Size += StrCat (St, "** NAN");
        else if (BlockZero && Value->Real == 0 && Value->Imaginary == 0)
          Size += StrCat (St, ' ');   // Zero value blocked (replaced with a space)
        else if (Polar)
          {
            ComplexCalcModArg (Value);
            Size += RealFormattedToStr (St, Value->Mod, format, Decimals, false);
            if (Value->Mod)
              {
                Size += StrCat (St, " arg ");
                //if (Cell->Value.Arg == 0.0)
                //  Size += CharToStr (St, '0');
                //else
                  if (Radians)
                    {
                      if (!RealToRationalStr (St, &Size, Value->Arg / M_PI, 2, "pi"))
                        Size += RealFormattedToStr (St, Value->Arg, format, Decimals, false);
                    }
                  else
                    {
                      Size += RealFormattedToStr (St, Value->Arg * 180.0 / M_PI, format, Decimals, false);
                      Size += StrCat (St, " deg");
                    }
              }
          }
        else
          {
            Re = Value->Real;
            Im = Value->Imaginary;
            if (Im != 0)
              if (Re + (Im / 100.0) == Re)
                Im = 0.0;
            if (Re != 0)
              if ((Re / 100.0) + Im == Im)
                Re = 0.0;
            if ((Re != 0.0) || (Im == 0))
              {
                Size += RealFormattedToStr (St, Re, format, Decimals, false);
                if (Im > 0.0)
                  {
                    StrCat (St, '+');
                    Size++;
                  }
              }
            if (Im != 0)
              Size += RealFormattedToStr (St, Im, format, Decimals, true);
          }
      }
    return Size;
  }

int StrCellFormatValue (char **St, _Cell *Cell, bool AllowCommas)
  {
    int Len;
    //
    Len = 0;
    if (Cell->Content && *Cell->Content)
      {
        if (Cell->Exp && Cell->Error)
          Len = StrCat (St, "** ERROR");
        else
          {
            if (!Cell->Error)
              Len = StrValueFormat (St, &Cell->Value, Cell->Format, Cell->Exp, AllowCommas);
            if (Len == 0)
              if ((Cell->Content [0] == '\"') || (Cell->Content [0] == '\''))
                Len = StrCat (St, &Cell->Content [1]);
              else
                Len = StrCat (St, Cell->Content);
          }
      }
    return Len;
  }

