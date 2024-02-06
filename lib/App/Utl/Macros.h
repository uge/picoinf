#pragma once



// #define CHAR_TO_STR(c) \
//   c == 'a' ? "a" : ( \
//   c == 'b' ? "b" : ( \
//   c == 'c' ? "c" : ( \
//   c == 'd' ? "d" : ( \
//   c == 'e' ? "e" : ( \
//   c == 'f' ? "f" : ( \
//   c == 'g' ? "g" : ( \
//   c == 'h' ? "h" : ( \
//   c == 'i' ? "i" : ( \
//   c == 'j' ? "j" : ( \
//   c == 'k' ? "k" : ( \
//   c == 'l' ? "l" : ( \
//   c == 'm' ? "m" : ( \
//   c == 'n' ? "n" : ( \
//   c == 'o' ? "o" : ( \
//   c == 'p' ? "p" : ( \
//   c == 'q' ? "q" : ( \
//   c == 'r' ? "r" : ( \
//   c == 's' ? "s" : ( \
//   c == 't' ? "t" : ( \
//   c == 'u' ? "u" : ( \
//   c == 'v' ? "v" : ( \
//   c == 'w' ? "w" : ( \
//   c == 'x' ? "x" : ( \
//   c == 'y' ? "y" : ( \
//   c == 'z' ? "z" : ( \
//   c == 'A' ? "A" : ( \
//   c == 'B' ? "B" : ( \
//   c == 'C' ? "C" : ( \
//   c == 'D' ? "D" : ( \
//   c == 'E' ? "E" : ( \
//   c == 'F' ? "F" : ( \
//   c == 'G' ? "G" : ( \
//   c == 'H' ? "H" : ( \
//   c == 'I' ? "I" : ( \
//   c == 'J' ? "J" : ( \
//   c == 'K' ? "K" : ( \
//   c == 'L' ? "L" : ( \
//   c == 'M' ? "M" : ( \
//   c == 'N' ? "N" : ( \
//   c == 'O' ? "O" : ( \
//   c == 'P' ? "P" : ( \
//   c == 'Q' ? "Q" : ( \
//   c == 'R' ? "R" : ( \
//   c == 'S' ? "S" : ( \
//   c == 'T' ? "T" : ( \
//   c == 'U' ? "U" : ( \
//   c == 'V' ? "V" : ( \
//   c == 'W' ? "W" : ( \
//   c == 'X' ? "X" : ( \
//   c == 'Y' ? "Y" : ( \
//   c == 'Z' ? "Z" : ( "" ))))))))))))))))))))))))))))))))))))))))))))))))))))

#define CHAR_TO_STR(c) \
  c == 'a' ? "a" :  \
  c == 'b' ? "b" :  \
  c == 'c' ? "c" :  \
  c == 'd' ? "d" :  \
  c == 'e' ? "e" :  \
  c == 'f' ? "f" :  \
  c == 'g' ? "g" :  \
  c == 'h' ? "h" :  \
  c == 'i' ? "i" :  \
  c == 'j' ? "j" :  \
  c == 'k' ? "k" :  \
  c == 'l' ? "l" :  \
  c == 'm' ? "m" :  \
  c == 'n' ? "n" :  \
  c == 'o' ? "o" :  \
  c == 'p' ? "p" :  \
  c == 'q' ? "q" :  \
  c == 'r' ? "r" :  \
  c == 's' ? "s" :  \
  c == 't' ? "t" :  \
  c == 'u' ? "u" :  \
  c == 'v' ? "v" :  \
  c == 'w' ? "w" :  \
  c == 'x' ? "x" :  \
  c == 'y' ? "y" :  \
  c == 'z' ? "z" :  \
  c == 'A' ? "A" :  \
  c == 'B' ? "B" :  \
  c == 'C' ? "C" :  \
  c == 'D' ? "D" :  \
  c == 'E' ? "E" :  \
  c == 'F' ? "F" :  \
  c == 'G' ? "G" :  \
  c == 'H' ? "H" :  \
  c == 'I' ? "I" :  \
  c == 'J' ? "J" :  \
  c == 'K' ? "K" :  \
  c == 'L' ? "L" :  \
  c == 'M' ? "M" :  \
  c == 'N' ? "N" :  \
  c == 'O' ? "O" :  \
  c == 'P' ? "P" :  \
  c == 'Q' ? "Q" :  \
  c == 'R' ? "R" :  \
  c == 'S' ? "S" :  \
  c == 'T' ? "T" :  \
  c == 'U' ? "U" :  \
  c == 'V' ? "V" :  \
  c == 'W' ? "W" :  \
  c == 'X' ? "X" :  \
  c == 'Y' ? "Y" :  \
  c == 'Z' ? "Z" :  ""


// #define CHARLOWER(c)        \
//     (c) >= 'A'          ?   \
//     ((c) - ('A' - 'a')) :   \
//     (c)                     

// #define CHARLOWER(c)        \
//     c >= 'A'          ?   \
//     c - ('A' - 'a') :   \
//     c                     

#define CHARLOWER(c) c - ((c >= 'A') ? ('A' - 'a') : 0)

#define CHARAT(str, idx) #str[idx]

#define TOSTR(str) CHAR_TO_STR('A') CHAR_TO_STR(CHARAT(str, 0))
#define TOSTR2(str) CHAR_TO_STR('A') CHAR_TO_STR(CHARLOWER(CHARAT(str, 0)))

// #define MYLOWERZ(str)                     \
//     sizeof(#str) > 1 ? CHAR_TO_STR(CHARLOWER((#str)[0])) : "" \
//     sizeof(#str) > 2 ? CHAR_TO_STR(CHARLOWER((#str)[1])) : ""
#define MYLOWERZ(str)                 \
    CHAR_TO_STR(CHARLOWER(#str[1])) CHAR_TO_STR(CHARLOWER(#str[0])) CHAR_TO_STR(CHARLOWER(#str[1]))


#define MYLOWERZ0(str) CHAR_TO_STR((CHARLOWER(#str[1])))

#define WTF(str) #str[0]
#define WTF2(str) #str[1]

// #define MYLOWERZ(str)      \
//     CHAR_TO_STR(#str[0])   CHAR_TO_STR(#str[1])

// #define TESTIT(str) #str #str CHAR_TO_STR(#str[1]) CHAR_TO_STR(#str[1])
