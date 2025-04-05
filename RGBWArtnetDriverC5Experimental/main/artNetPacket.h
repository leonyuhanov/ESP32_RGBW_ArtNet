class artNetPacket
{
  public:
    artNetPacket();
    void parseArtNetPacket(char *packetBuffer);
    short unsigned int scaleInput(unsigned short int index, unsigned short int minOutput, unsigned short int maxOutput);
    char dumpData(short int index);

    char header[7];             //0-6
    char opcode[2];             //8-9
    char protocolVersion[2];    //10-11
    char sequence;              //12
    char physical;              //13
    char universe[2];           //14-15
    char dataLength[2];         //16-17
    char* data;                 //18-530
    char hasChanged;
    
    short int pCnt;
    short int pIndex;
    
  private:
};
