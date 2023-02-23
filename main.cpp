#include <appdef.hpp>
#include <sdk/os/debug.hpp>
#include <sdk/os/input.hpp>
#include <sdk/os/lcd.hpp>
#include <sdk/os/mem.hpp>

/* This program was written by SnailMath for the claculator classpad II.
 * This is a simple hex editor.
 * You can type in an address and click on go
 * The data you see is read in bytes, if you want to read addresses, that can only
 * accessed as a word, type any 4 digits in the command line and press read to read
 * as a word or type 8 digits and presss read to read as a long. The read data will
 * be displayed in the command line.
 * To write to a location type 2, 4 or 8 digits in the command line and type write.
 *
 * This is based on the app_template from the hollyhock project by The6P4C, the
 * fork by Stellaris-code to be precise.
 */

//Der Bildschirm ist 53 zeichen breit, als 0-52.


//The HollyHock Info section:

APP_NAME("HexEditor")
APP_DESCRIPTION("A simple hex editor. Can read and write byte, word and long. github.com/SnailMath/CPhexEditor")
APP_AUTHOR("SnailMath")
APP_VERSION("1.0.0")

#define PIXEL(x, y) (vram[(x) + (y) * width])
#define mem_ch(x) numToAscii[memory[x]]
extern int width, height;
extern uint16_t *vram;
unsigned char numToAscii[257]= "................................ !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~.................................................................................................................................";
#define underlineLeft(x) ((x<4)?(18+(18*x)):(24+(18*x)))
#define underlineLeftAscii(x) ((x<4)?(174+(6*x)):(180+(6*x)))
#define underlineTop(y) (71+(12*y))
void initscreen();
void hexdump();
#define MAXinput 32
#if MAXinput%25==1
  #error MAXinput is not even
#endif
char input[MAXinput+3]; //'>' + 32characters + '_' + 0x00
char search[MAXinput/2];//32hex digits are 16 byte.
uint8_t *searchAddr;
char searchdirection=0; //Direction of the search. -1 means search left, +1 means search right, 0 means no search active.
char searchlen = 0;
unsigned char inputpos;
uint8_t timeBackPressed = 1; //The time back is being held down.

uint8_t *memory = (uint8_t*)0x808fcba0;
char cursorx = 2; //The cursor in the hex view
char cursory = 2; //The cursor in the hex view


extern "C"
void main() {
  LCD_GetSize(&width, &height);
  vram = LCD_GetVRAMAddress();
  input[0]='>';
  input[1]='_';
  input[2]=0;
  inputpos=1; //current cursor position in 'input'
  memory=(uint8_t*)0x8cff0d00;//((uint32_t)(&memory+0x200)&(~0xff));  //Initialize memory to an 'interesting' location

  struct InputEvent event;
  
  LCD_VRAMBackup();
  initscreen();
  LCD_Refresh();

  bool running = true;
  while(running){
    memset(&event, 0, sizeof(event));
    GetInput(&event, 0xFFFFFFFF, 0x10);
    switch(event.type){
    case EVENT_KEY:
      if(event.data.key.direction==KEY_PRESSED){
        char ch = ' ';
        bool typed = false;
        if(event.data.key.keyCode>=KEYCODE_0 && event.data.key.keyCode<=KEYCODE_9){
          ch = event.data.key.keyCode + ( '0' - KEYCODE_0 );
          typed = true;
        }
        if(event.data.key.keyCode==KEYCODE_EQUALS){ch = 'A';typed = true;}
        if(event.data.key.keyCode==KEYCODE_X     ){ch = 'B';typed = true;}
        if(event.data.key.keyCode==KEYCODE_Y     ){ch = 'C';typed = true;}
        if(event.data.key.keyCode==KEYCODE_Z     ){ch = 'D';typed = true;}
        if(event.data.key.keyCode==KEYCODE_POWER ){ch = 'E';typed = true;}
        if(event.data.key.keyCode==KEYCODE_DIVIDE){ch = 'F';typed = true;}
        if(event.data.key.keyCode==KEYCODE_RIGHT ){cursorx++;}
        if(event.data.key.keyCode==KEYCODE_LEFT  ){cursorx--;}
        if(event.data.key.keyCode==KEYCODE_DOWN  ){cursory++;}
        if(event.data.key.keyCode==KEYCODE_UP    ){cursory--;}
	if(cursorx> 7){cursorx= 0;cursory++;}
	if(cursorx< 0){cursorx= 7;cursory--;}
	if(cursory>31){cursory= 0;memory=memory+0x100;}
	if(cursory< 0){cursory=31;memory=memory-0x100;}
        if(typed){
          if(inputpos<=MAXinput){
            input[inputpos++]=ch;//MAXinput
            input[inputpos]='_'; //MAXinput+1
            input[inputpos+1]=0; //MAXinput+2
            if(inputpos==(MAXinput+1)){
              input[inputpos]=0; //MAXinput+1
	    }
          }
        }
        if(event.data.key.keyCode == KEYCODE_POWER_CLEAR){
          running = false;
        }
        if(event.data.key.keyCode==KEYCODE_BACKSPACE){
          if(inputpos>1){
	    input[inputpos--]=0;
            input[inputpos]='_';
          }
        }
      } else if(event.data.key.direction==KEY_HELD){
        if(event.data.key.keyCode==KEYCODE_BACKSPACE){
          if(timeBackPressed<20){timeBackPressed *= 2;}
          for(uint8_t i=0;i<timeBackPressed;i++){
            if(inputpos>1){
              input[inputpos--]=0;
              input[inputpos]='_';
            }
          }
	}
      } else if(event.data.touch_single.direction==KEY_RELEASED){
          timeBackPressed = 1; //The time back is being held down.        
      }
      initscreen();
      LCD_Refresh(); 
      break;
    case EVENT_TOUCH:
      unsigned int x = (unsigned int) event.data.touch_single.p1_x;
      unsigned int y = (unsigned int) event.data.touch_single.p1_y;
      //initscreen();
      Debug_Printf(46,2,false, 0,"%3d %3d",x,y);
      if(event.data.touch_single.direction==TOUCH_UP){
        if(y>=497){if(y<=521){
          bool newSearch = 0;
          if(x<=117&&x>=93){
            //Search Right
            //Debug_Printf(0,2,false, 0,"Search Right");
	    searchdirection=1;
            newSearch = 1,
	    searchAddr = memory + (cursory * 8) + cursorx + 1; //Start at the right of the current cursor position
          }
          else if(x<=81&&x>=57){
            //Search Left
            //Debug_Printf(0,2,false, 0,"Search Left");
	    searchdirection=-1;
            newSearch = 1;
	    searchAddr = memory + (cursory * 8) + cursorx - 1; //Start at the left of the current cursor position
          }
	  if(newSearch){
		//char input[MAXinput+3]; //'>' + 32characters + '_' + 0x00
		//char search[MAXinput/2];//32hex digits are 16 byte.
		//char searchlen = 0
            if(inputpos%2==1 && inputpos>1){ //the number of typed digits is inputpos-1 and has to be even. The input should not be empty.
              for (int i = 0; i<(MAXinput/2); i++){
                search[i]=(((input[1+(2*i)]<='9')?(input[1+(2*i)]-'0'):(input[1+(2*i)]+(10-'A'))) << 4) +
                          (((input[2+(2*i)]<='9')?(input[2+(2*i)]-'0'):(input[2+(2*i)]+(10-'A'))));
              }
	      searchlen = (inputpos-1)/2;
	    }
	    else{
              searchdirection=0;
	    }
	  }
          if(x>=135&&x<=177){
            //Goto
            //Debug_Printf(0,2,false, 0,"Goto");
	    if (inputpos>=2){
              uint32_t newaddr = (((input[1]<='9')?(input[1]-'0'):(input[1]+(10-'A'))) << 28);
	      if (inputpos>=3){
                newaddr += (((input[2]<='9')?(input[2]-'0'):(input[2]+(10-'A'))) << 24);
	      }
	      if (inputpos>=4){
                newaddr += (((input[3]<='9')?(input[3]-'0'):(input[3]+(10-'A'))) << 20);
	      }
	      if (inputpos>=5){
                newaddr += (((input[4]<='9')?(input[4]-'0'):(input[4]+(10-'A'))) << 16);
	      }
	      if (inputpos>=6){
                newaddr += (((input[5]<='9')?(input[5]-'0'):(input[5]+(10-'A'))) << 12);
	      }
	      if (inputpos>=7){
                newaddr += (((input[6]<='9')?(input[6]-'0'):(input[6]+(10-'A'))) <<  8);
	      }
	      if (inputpos==8){ //Set the cursor to the correct line
                cursory =  (((input[7]<='9')?(input[7]-'0'):(input[7]+(10-'A'))) <<  1);
                cursorx =  0;
	      }
	      if (inputpos==9){
                cursory =  (((input[7]<='9')?(input[7]-'0'):(input[7]+(10-'A'))) <<  1)+ 
                           (((input[8]<='9')?(input[8]-'0'):(input[8]+(10-'A'))) >>  3); 
                cursorx =  (((input[8]<='9')?(input[8]-'0'):(input[8]+(10-'A')))  &  7);
	      }
              memory=(uint8_t*)newaddr;
	    }
            //initscreen();
            //LCD_Refresh();
          }
          if(x>=261&&x<=309){ //Write
          //if(inputpos%2==1){//Even number of digits (don't count the preceeding ">")
              uint32_t write = 0;
              if(inputpos==3){ // ">FF_" Write a byte
                write=(((input[1]<='9')?(input[1]-'0'):(input[1]+(10-'A'))) <<  4) +
                      (((input[2]<='9')?(input[2]-'0'):(input[2]+(10-'A')))) ;
		asm volatile ("mov.b %1, @%0":: "r" ( memory+cursorx+(cursory*8)),"r"(write));
              }
              if(inputpos==5){ // ">FFFF_" Write a word
                write=(((input[1]<='9')?(input[1]-'0'):(input[1]+(10-'A'))) << 12) +
                      (((input[2]<='9')?(input[2]-'0'):(input[2]+(10-'A'))) <<  8) +
                      (((input[3]<='9')?(input[3]-'0'):(input[3]+(10-'A'))) <<  4) +
                      (((input[4]<='9')?(input[4]-'0'):(input[4]+(10-'A')))) ;
		asm volatile ("mov.w %1, @%0":: "r" ( memory+cursorx+(cursory*8)),"r"(write));
              }
              if(inputpos==9){ // ">FFFFFFFF_" Write a long
                write=(((input[1]<='9')?(input[1]-'0'):(input[1]+(10-'A'))) << 28) +
                      (((input[2]<='9')?(input[2]-'0'):(input[2]+(10-'A'))) << 24) +
                      (((input[3]<='9')?(input[3]-'0'):(input[3]+(10-'A'))) << 20) +
                      (((input[4]<='9')?(input[4]-'0'):(input[4]+(10-'A'))) << 16) +
                      (((input[5]<='9')?(input[5]-'0'):(input[5]+(10-'A'))) << 12) +
                      (((input[6]<='9')?(input[6]-'0'):(input[6]+(10-'A'))) <<  8) +
                      (((input[7]<='9')?(input[7]-'0'):(input[7]+(10-'A'))) <<  4) +
                      (((input[8]<='9')?(input[8]-'0'):(input[8]+(10-'A')))) ;
		asm volatile ("mov.l %1, @%0":: "r" ( memory+cursorx+(cursory*8)),"r"(write));
              }

	  //}
          }
          if(x>=201&&x<=243){ //Read
            uint32_t read=0;
            if(inputpos==3){ // ">FF_" Read a byte
		asm volatile ("mov.b @%1, %0":"=r"(read):"r"( memory+cursorx+(cursory*8)));
		input[1]=(((read>> 4)&0xf)<10)?(((read>> 4)&0xf)+'0'):(((read>> 4)&0xf)+('A'-10));
		input[2]=(((read    )&0xf)<10)?(((read    )&0xf)+'0'):(((read    )&0xf)+('A'-10)); 
	    }
            if(inputpos==5&&cursorx%2==0){ // ">FFFF_" Read a word
		asm volatile ("mov.w @%1, %0":"=r"(read):"r"( memory+cursorx+(cursory*8)));
		input[1]=(((read>>12)&0xf)<10)?(((read>>12)&0xf)+'0'):(((read>>12)&0xf)+('A'-10));
		input[2]=(((read>> 8)&0xf)<10)?(((read>> 8)&0xf)+'0'):(((read>> 8)&0xf)+('A'-10));
		input[3]=(((read>> 4)&0xf)<10)?(((read>> 4)&0xf)+'0'):(((read>> 4)&0xf)+('A'-10));
		input[4]=(((read    )&0xf)<10)?(((read    )&0xf)+'0'):(((read    )&0xf)+('A'-10)); 
	    }
            if(inputpos==9&&cursorx%4==0){ // ">FFFFFFFF_" Read a long
		asm volatile ("mov.l @%1, %0":"=r"(read):"r"( memory+cursorx+(cursory*8)));
		input[1]=(((read>>28)&0xf)<10)?(((read>>28)&0xf)+'0'):(((read>>28)&0xf)+('A'-10));
		input[2]=(((read>>24)&0xf)<10)?(((read>>24)&0xf)+'0'):(((read>>24)&0xf)+('A'-10));
		input[3]=(((read>>20)&0xf)<10)?(((read>>20)&0xf)+'0'):(((read>>20)&0xf)+('A'-10));
		input[4]=(((read>>16)&0xf)<10)?(((read>>16)&0xf)+'0'):(((read>>16)&0xf)+('A'-10));
		input[5]=(((read>>12)&0xf)<10)?(((read>>12)&0xf)+'0'):(((read>>12)&0xf)+('A'-10));
		input[6]=(((read>> 8)&0xf)<10)?(((read>> 8)&0xf)+'0'):(((read>> 8)&0xf)+('A'-10));
		input[7]=(((read>> 4)&0xf)<10)?(((read>> 4)&0xf)+'0'):(((read>> 4)&0xf)+('A'-10));
		input[8]=(((read    )&0xf)<10)?(((read    )&0xf)+'0'):(((read    )&0xf)+('A'-10)); 
	    }
          }
        }}
        if(y<=89){if(y>=65){
          if(x>=243&&x<=267){
            //left
	    memory-=0x100;
            //Debug_Printf(0,2,false, 0,"go left");
          }
          if(x>=279&&x<=303){
            //right
	    memory+=0x100;
            //Debug_Printf(0,2,false, 0,"go right");
          }
        }}
      } /*else if(event.data.touch_single.direction==TOUCH_HOLD_DRAG){
        if(x<(unsigned int)width && y<(unsigned int)height){
          unsigned a = 0; 
          while(a<(unsigned int)width){
            PIXEL(a,y) = 0b11111;
            a++;
          }
          a = 0; 
          while(a<(unsigned int)height){
            PIXEL(x,a) = 0b11111;
            a++;
          }
          //initscreen();
	}
      }*/
      //initscreen();
      //LCD_Refresh(); 
      break;
    }
    //initscreen();
    //LCD_Refresh(); 
    if (searchdirection){ //If a search is active
      //Search the first digit, if it matches check the next digits.

      asm ("searchloop:\n"
		"add %3, %1;"
		"mov.b @%1, r0;"
		"cmp/eq r0, %2;"
		"bf searchloop;"
		"nop;"
		"mov %1, %0"
           : "=r" ( searchAddr )        //output
           : "r"  ( searchAddr )        //input
           , "r"  ( search[0] )         //input
           , "r"  ( searchdirection )   //input
           : "r0"              //used register
       );
      //Now the variable searchAddr points to a memory location with the correct content.
      {
        uint8_t i = 0;
        while(1==1){
	  //if the current address is different, the whole search term is wrong.
          if(search[i] != (uint8_t)*(searchAddr+i)){
            break;
	  }
	  //if it is the same, check the next
          i++;
	  if (i==searchlen){//If we've reached the end
	    //we found it
	    searchdirection = 0; //stop the search
            memory  = (uint8_t*)((uint32_t)searchAddr & 0xffffff00);
            cursorx =            (uint32_t)searchAddr & 0x00000007;     //The cursor in the hex view
            cursory =           ((uint32_t)searchAddr & 0x000000f8)>>3; //The cursor in the hex view

	    break;
	  }
        }
      }

    }
    initscreen();
    LCD_Refresh(); 
  }
  LCD_VRAMRestore();
  LCD_Refresh();
}


void initscreen(){
  LCD_ClearScreen();
  Debug_SetCursorPosition(1,0);
                          //   ihbcm
                          //  gtu.o/
  Debug_PrintString("HexEditor      SnailMath",0);
  Debug_Printf(21,1,false, 0,"github.com/");
  Debug_Printf(0,4,false, 0,"   0  1  2  3   4  5  6  7");
  hexdump();
  Debug_Printf(40, 4,false, 0,"0x%08x",memory);
  Debug_Printf(40, 5,false, 0,"+---+ +---+");
  Debug_Printf(40, 6,false, 0,"| < | | > |");
  Debug_Printf(40, 7,false, 0,"+---+ +---+");
//  Debug_Printf(40, 9,false, 0,"+---------+");
//  Debug_Printf(40,10,false, 0,"| Edit( ) |");
//  Debug_Printf(40,11,false, 0,"+---------+");
  Debug_Printf(1,39,false, 0,input);
  Debug_Printf(9,40,false, 0,"0x%08x",searchAddr);
  Debug_Printf(9,41,false, 0,        "+---+ +---+  +------+   +------+  +-------+");
  Debug_Printf(1,42,false, 0,"Search: | < | | > |  | Goto |   | Read |  | Write |");
  Debug_Printf(9,43,false, 0,        "+---+ +---+  +------+   +------+  +-------+");
  //Debug_Printf(9,43,false, 0,        "+---+ +---+  +------+   +------+  +- %02x %02x", search[0], *searchAddr);
}

void hexdump(){
  for(int i=0; i<16;i++){
    Debug_Printf(1,5+(2*i),false, 0,"%X %02X %02X %02X %02X  %02X %02X %02X %02X  %c%c%c%c %c%c%c%c", i,
      memory[0+(16*i)],memory[1+(16*i)],memory[ 2+(16*i)],memory[ 3+(16*i)],memory[ 4+(16*i)],memory[ 5+(16*i)],memory[ 6+(16*i)],memory[ 7+(16*i)],
      mem_ch(0+(16*i)),mem_ch(1+(16*i)),mem_ch( 2+(16*i)),mem_ch( 3+(16*i)),mem_ch( 4+(16*i)),mem_ch( 5+(16*i)),mem_ch( 6+(16*i)),mem_ch( 7+(16*i)));
    Debug_Printf(1,6+(2*i),false, 0, "  %02X %02X %02X %02X  %02X %02X %02X %02X  %c%c%c%c %c%c%c%c",
      memory[8+(16*i)],memory[9+(16*i)],memory[10+(16*i)],memory[11+(16*i)],memory[12+(16*i)],memory[13+(16*i)],memory[14+(16*i)],memory[15+(16*i)],
      mem_ch(8+(16*i)),mem_ch(9+(16*i)),mem_ch(10+(16*i)),mem_ch(11+(16*i)),mem_ch(12+(16*i)),mem_ch(13+(16*i)),mem_ch(14+(16*i)),mem_ch(15+(16*i)));
  }
  //Debug_Printf(3,4,false, 0,">");
  for(int x = underlineLeft(cursorx); x<=underlineLeft(cursorx)+12;x++){
    PIXEL(x,underlineTop(cursory))=0b1111100000000000;
  }
  for(int x = underlineLeftAscii(cursorx); x<=underlineLeftAscii(cursorx)+6;x++){
    PIXEL(x,underlineTop(cursory))=0;
  }
}



