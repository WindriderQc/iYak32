#ifndef SEVENSEGMENTASCII_H
#define SEVENSEGMENTASCII_H

#include <TM1637Display.h>
#include <cstdint>
#include <string>
#include <vector>

class SevenSegmentAscii {
public:
    SevenSegmentAscii(TM1637Display& display, uint8_t brightness = 0x0f);  // Set the brightness level (0-7)
    void displayString(const std::string& str);
    void displayCharacter(char c);
    void displayInteger(int num);
    void setColon(bool enabled); // Function to control the colon display


private:
    TM1637Display& display;
    const uint8_t charToSegment(char c);
    std::vector<uint8_t> stringToSegmentArray(const std::string& str);
    std::vector<uint8_t> integerToSegmentArray(int num);

    const int TEST_DELAY = 500;  // The amount of time (in milliseconds) between tests

    // 7-segment segments for digits
    const uint8_t CHAR_0 = SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F;   
    const uint8_t CHAR_1 = SEG_B | SEG_C;                                 
    const uint8_t CHAR_2 = SEG_A | SEG_B | SEG_D | SEG_E | SEG_G;          
    const uint8_t CHAR_3 = SEG_A | SEG_B | SEG_C | SEG_D | SEG_G;          
    const uint8_t CHAR_4 = SEG_B | SEG_C | SEG_F | SEG_G;                  
    const uint8_t CHAR_5 = SEG_A | SEG_C | SEG_D | SEG_F | SEG_G;          
    const uint8_t CHAR_6 = SEG_A | SEG_C | SEG_D | SEG_E | SEG_F | SEG_G;   

    const uint8_t CHAR_7 = SEG_A | SEG_B | SEG_C;                         
    const uint8_t CHAR_8 = SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F | SEG_G;   
    const uint8_t CHAR_9 = SEG_A | SEG_B | SEG_C | SEG_D | SEG_F | SEG_G;   

    // 7-segment segments for uppercase alphabet characters (A-Z)
    const uint8_t CHAR_A = SEG_A | SEG_B | SEG_C | SEG_E | SEG_F | SEG_G;
    const uint8_t CHAR_B = SEG_C | SEG_D | SEG_E | SEG_F | SEG_G;
    const uint8_t CHAR_C = SEG_A | SEG_D | SEG_E | SEG_F;
    const uint8_t CHAR_D = SEG_B | SEG_C | SEG_D | SEG_E | SEG_G;
    const uint8_t CHAR_E = SEG_A | SEG_D | SEG_E | SEG_F | SEG_G;
    const uint8_t CHAR_F = SEG_A | SEG_E | SEG_F | SEG_G;
    const uint8_t CHAR_G = SEG_A | SEG_C | SEG_D | SEG_E | SEG_F;
    const uint8_t CHAR_H = SEG_B | SEG_C | SEG_E | SEG_F | SEG_G;
    const uint8_t CHAR_I = SEG_E | SEG_F;
    const uint8_t CHAR_J = SEG_B | SEG_C | SEG_D;
    const uint8_t CHAR_K = SEG_E | SEG_F | SEG_G;
    const uint8_t CHAR_L = SEG_D | SEG_E | SEG_F;
    const uint8_t CHAR_M = SEG_A | SEG_C | SEG_E;
    const uint8_t CHAR_N = SEG_A | SEG_C | SEG_E | SEG_G;
    const uint8_t CHAR_O = SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F;
    const uint8_t CHAR_P = SEG_A | SEG_B | SEG_E | SEG_F | SEG_G;
    const uint8_t CHAR_Q = SEG_A | SEG_B | SEG_C | SEG_F | SEG_G;
    const uint8_t CHAR_R = SEG_E | SEG_G;
    const uint8_t CHAR_S = SEG_A | SEG_C | SEG_D | SEG_F | SEG_G;
    const uint8_t CHAR_T = SEG_D | SEG_E | SEG_F | SEG_G;
    const uint8_t CHAR_U = SEG_B | SEG_C | SEG_D | SEG_E | SEG_F;
    const uint8_t CHAR_V = SEG_C | SEG_D | SEG_E;
    const uint8_t CHAR_W = SEG_B | SEG_D | SEG_F;
    const uint8_t CHAR_X = SEG_B | SEG_C | SEG_E | SEG_F | SEG_G;
    const uint8_t CHAR_Y = SEG_B | SEG_C | SEG_D | SEG_F | SEG_G;
    const uint8_t CHAR_Z = SEG_A | SEG_B | SEG_D | SEG_E | SEG_G;

    // 7-segment segments for lowercase alphabet characters (a-z)
    const uint8_t CHAR_a = SEG_A | SEG_B | SEG_C | SEG_E | SEG_F | SEG_G;
    const uint8_t CHAR_b = SEG_C | SEG_D | SEG_E | SEG_F | SEG_G;
    const uint8_t CHAR_c = SEG_A | SEG_D | SEG_E | SEG_F;
    const uint8_t CHAR_d = SEG_B | SEG_C | SEG_D | SEG_E | SEG_G;
    const uint8_t CHAR_e = SEG_A | SEG_D | SEG_E | SEG_F | SEG_G;
    const uint8_t CHAR_f = SEG_A | SEG_E | SEG_F | SEG_G;
    const uint8_t CHAR_g = SEG_A | SEG_C | SEG_D | SEG_E | SEG_F;
    const uint8_t CHAR_h = SEG_E | SEG_F | SEG_G;
    const uint8_t CHAR_i = SEG_E | SEG_F;
    const uint8_t CHAR_j = SEG_B | SEG_C | SEG_D;
    const uint8_t CHAR_k = SEG_E | SEG_F | SEG_G;
    const uint8_t CHAR_l = SEG_D | SEG_E | SEG_F;
    const uint8_t CHAR_m = SEG_A | SEG_C | SEG_E;
    const uint8_t CHAR_n = SEG_E | SEG_G;
    const uint8_t CHAR_o = SEG_C | SEG_D | SEG_E | SEG_G;
    const uint8_t CHAR_p = SEG_A | SEG_B | SEG_E | SEG_F | SEG_G;
    const uint8_t CHAR_q = SEG_A | SEG_B | SEG_C | SEG_F | SEG_G;
    const uint8_t CHAR_r = SEG_E | SEG_G;
    const uint8_t CHAR_s = SEG_A | SEG_C | SEG_D | SEG_F | SEG_G;
    const uint8_t CHAR_t = SEG_D | SEG_E | SEG_F | SEG_G;
    const uint8_t CHAR_u = SEG_C | SEG_D | SEG_E;
    const uint8_t CHAR_v = SEG_C | SEG_D | SEG_E;
    const uint8_t CHAR_w = SEG_B | SEG_D | SEG_F;
    const uint8_t CHAR_x = SEG_B | SEG_C | SEG_E | SEG_F | SEG_G;
    const uint8_t CHAR_y = SEG_B | SEG_C | SEG_D | SEG_F | SEG_G;
    const uint8_t CHAR_z = SEG_A | SEG_B | SEG_D | SEG_E | SEG_G;

    // 7-segment segments for special characters
    const uint8_t CHAR_SPACE = 0x00;   // Space
    const uint8_t CHAR_DASH = SEG_G;   // Dash
    const uint8_t CHAR_COLON = SEG_DP;
};

#endif // SEVENSEGMENTASCII_H
