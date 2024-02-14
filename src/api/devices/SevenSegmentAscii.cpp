#include "SevenSegmentAscii.h"
#include <Arduino.h>

// Constructor
SevenSegmentAscii::SevenSegmentAscii(TM1637Display& display, uint8_t brightness) : display(display) {

    display.setBrightness(brightness);

    uint8_t data[] = { 0xff, 0xff, 0xff, 0xff };
    uint8_t blank[] = { 0x00, 0x00, 0x00, 0x00 };

    // All segments on
   /* display.setSegments(data);
    delay(1000);
    display.setSegments(blank);
    delay(1000);*/
       
    //displayInteger(666);
    //delay(1000);
}


// Function to display a single character on the 7-segment display
void SevenSegmentAscii::displayCharacter(char c) {
     uint8_t segment = charToSegment(c);
    display.setSegments(&segment, 1);
}


// Function to display a string on the 7-segment display
void SevenSegmentAscii::displayString(const std::string& str) {
   bool colonPresent = false;
    std::vector<uint8_t> segmentArray;

    for (char c : str) {
        if (c == ':') {
            colonPresent = true;
        } else {
            segmentArray.push_back(charToSegment(c));
        }
    }

    // If colon is present, set the colon segment on the 2nd LED digit from the left
    if (colonPresent) {
        segmentArray[1] |= 0b10000000; // Set the MSB for the 2nd digit (assuming 0-based indexing)
    }

    display.setSegments(segmentArray.data(), segmentArray.size());
}

// Function to control the colon display   //  Penses que ca marche pas! :S
void SevenSegmentAscii::setColon(bool enabled) {
    // The segment for the colon (SEG_COLON) needs to be defined based on your display configuration
    // For example, if SEG_COLON represents the colon segment, you can use the following:
    uint8_t colonSegment = enabled ? SEG_DP : 0;
    display.setSegments(&colonSegment, 1, 1);
}

// Function to display an integer on the 7-segment display
void SevenSegmentAscii::displayInteger(int num) {
    std::vector<uint8_t> segmentArray;

    if (num == 0) {
        segmentArray.push_back(charToSegment('0'));
    } else {
        while (num > 0) {
            int digit = num % 10;
            segmentArray.push_back(charToSegment('0' + digit));
            num /= 10;
        }
    }

    display.setSegments(segmentArray.data(), segmentArray.size());
}


// Helper function to convert a character to its 7-segment representation
const uint8_t SevenSegmentAscii::charToSegment(char c) {
    if (c >= '0' && c <= '9') {
        switch (c) {
            case '0': return CHAR_0;
            case '1': return CHAR_1;
            case '2': return CHAR_2;
            case '3': return CHAR_3;
            case '4': return CHAR_4;
            case '5': return CHAR_5;
            case '6': return CHAR_6;
            case '7': return CHAR_7;
            case '8': return CHAR_8;
            case '9': return CHAR_9;
        }
    } else if (c >= 'A' && c <= 'Z') {
        switch (c) {
            case 'A': return CHAR_A;
            case 'B': return CHAR_B;
            case 'C': return CHAR_C;
            case 'D': return CHAR_D;
            case 'E': return CHAR_E;
            case 'F': return CHAR_F;
            case 'G': return CHAR_G;
            case 'H': return CHAR_H;
            case 'I': return CHAR_I;
            case 'J': return CHAR_J;
            case 'K': return CHAR_K;
            case 'L': return CHAR_L;
            case 'M': return CHAR_M;
            case 'N': return CHAR_N;
            case 'O': return CHAR_O;
            case 'P': return CHAR_P;
            case 'Q': return CHAR_Q;
            case 'R': return CHAR_R;
            case 'S': return CHAR_S;
            case 'T': return CHAR_T;
            case 'U': return CHAR_U;
            case 'V': return CHAR_V;
            case 'W': return CHAR_W;
            case 'X': return CHAR_X;
            case 'Y': return CHAR_Y;
            case 'Z': return CHAR_Z;
        }
    } else if (c >= 'a' && c <= 'z') {
        switch (c) {
            case 'a': return CHAR_a;
            case 'b': return CHAR_b;
            case 'c': return CHAR_c;
            case 'd': return CHAR_d;
            case 'e': return CHAR_e;
            case 'f': return CHAR_f;
            case 'g': return CHAR_g;
            case 'h': return CHAR_h;
            case 'i': return CHAR_i;
            case 'j': return CHAR_j;
            case 'k': return CHAR_k;
            case 'l': return CHAR_l;
            case 'm': return CHAR_m;
            case 'n': return CHAR_n;
            case 'o': return CHAR_o;
            case 'p': return CHAR_p;
            case 'q': return CHAR_q;
            case 'r': return CHAR_r;
            case 's': return CHAR_s;
            case 't': return CHAR_t;
            case 'u': return CHAR_u;
            case 'v': return CHAR_v;
            case 'w': return CHAR_w;
            case 'x': return CHAR_x;
            case 'y': return CHAR_y;
            case 'z': return CHAR_z;
        }
    } else if (c == ' ') {
        return CHAR_SPACE;
    } else if (c == '-') {
        return CHAR_DASH;
    } else if (c == ':') {
        return CHAR_COLON;
    }

    // Handle other characters if needed
    // For now, treat them as spaces
    return CHAR_SPACE;
}

// Helper function to convert an integer to an array of 7-segment characters
std::vector<uint8_t> SevenSegmentAscii::integerToSegmentArray(int num) {
    std::vector<uint8_t> segmentArray;

    if (num == 0) {
        segmentArray.push_back(CHAR_0);
    } else {
        while (num > 0) {
            int digit = num % 10;
            segmentArray.push_back(CHAR_0 + digit);
            num /= 10;
        }
    }

    // Reverse the vector manually
    for (size_t i = 0, j = segmentArray.size() - 1; i < j; i++, j--) {
        uint8_t temp = segmentArray[i];
        segmentArray[i] = segmentArray[j];
        segmentArray[j] = temp;
    }

    return segmentArray;
}

// Helper function to convert a string to an array of 7-segment characters
std::vector<uint8_t> SevenSegmentAscii::stringToSegmentArray(const std::string& str) {
    std::vector<uint8_t> segmentArray;
    for (char c : str) {
        segmentArray.push_back(charToSegment(c));
    }

    // Reverse the vector manually
    for (size_t i = 0, j = segmentArray.size() - 1; i < j; i++, j--) {
        uint8_t temp = segmentArray[i];
        segmentArray[i] = segmentArray[j];
        segmentArray[j] = temp;
    }

    return segmentArray;
}
