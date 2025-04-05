#include <cstdint>

class NeoViaSPI
{
  public:
    NeoViaSPI();
	NeoViaSPI(unsigned short int numLEDs);
    void setPixel(short int pixelIndex, uint8_t *pixelColour);
    void getPixel(short int pixelIndex, uint8_t *pixelColour);
    void encode();
    
    uint8_t* LEDs;
    uint8_t* neoBits;
    
    unsigned short int _rainbowSize;
    unsigned short int _LEDframeLength;
    unsigned short int _NeoBitsframeLength;
    unsigned short int _numLEDs;
    unsigned short int _counter;
    unsigned short int _nCounter;
    uint8_t _colCounter;
    uint8_t _bitCounter;
    
  private:
    uint8_t _blankValue;
    uint8_t _tmpValue;
    uint8_t _blankBlock;
    uint8_t _testValues[4][2];
};

