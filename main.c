#include <xc.h>
#include <string.h>

#pragma config JTAGEN = OFF     
#pragma config FWDTEN = OFF
#pragma config FNOSC =	FRCPLL
#pragma config FSOSCEN =	OFF
#pragma config POSCMOD =	EC
#pragma config OSCIOFNC =	ON
#pragma config FPBDIV =     DIV_1
#pragma config FPLLIDIV =	DIV_2
#pragma config FPLLMUL =	MUL_20
#pragma config FPLLODIV =	DIV_1

#define ENABLE() {PORTDbits.RD4=1;\
                  PORTDbits.RD4=0;\
                  busy();}

#define CONTROL() {PORTBbits.RB15=0; /*RS=0*/\
                  PORTDbits.RD5=0; /*RW=0*/}
                   
#define DATA() {PORTBbits.RB15=1; /*RS=0*/ \
                PORTDbits.RD5=0; /*RW=0*/}

#define FIRST_LINE_START 0x80
#define FIRST_LINE_END 0x8f
#define SECOND_LINE_START 0xc0
#define SECOND_LINE_END 0xcf

//Switches functions
void count();
void shift();
void fan();
char* determineSpeed();
void makeSound();

//LCD functions
void busy(void);
void delay();
void clearScreen();
void printScreen(char string[], int str_len, int pos);
void printTopLine(char* string, int len);

//global speed variables 
int speed;
int soundSpeed;

char space[16] = {0};

void main(){ 
    
    //LCD initialization 
    char control[]={0x38,0x38,0x38,0xc,0x6,0x1};
    TRISBbits.TRISB15 = 0; // RB15 (DISP_RS) set as an output
    ANSELBbits.ANSB15 = 0; // disable analog functionality on RB15 (DISP_RS)
    TRISDbits.TRISD5 = 0; // RD5 (DISP_RW) set as an output
    TRISDbits.TRISD4 = 0; // RD4 (DISP_EN) set as an output
    TRISE &= 0xff00;
    ANSELEbits.ANSE2 = 0;
    ANSELEbits.ANSE4 = 0;
    ANSELEbits.ANSE5 = 0;
    ANSELEbits.ANSE6 = 0;
    ANSELEbits.ANSE7 = 0;
   
    //LED's and Switches initialization
    TRISA &= 0xff00;
    TRISFbits.TRISF3 = 1; // RF3 (SW0) configured as input
    TRISFbits.TRISF5 = 1; // RF5 (SW1) configured as input
    TRISFbits.TRISF4 = 1; // RF4 (SW2) configured as input
    TRISDbits.TRISD15 = 1; // RD15 (SW3) configured as input
    TRISDbits.TRISD14 = 1; // RD14 (SW4) configured as input
    TRISBbits.TRISB11 = 1; // RB11 (SW5) configured as input
    ANSELBbits.ANSB11 = 0; // RB11 (SW5) disabled analog
    TRISBbits.TRISB10 = 1; // RB10 (SW6) configured as input
    ANSELBbits.ANSB10 = 0; // RB10 (SW6) disabled analog
    TRISBbits.TRISB9 = 1; // RB9 (SW7) configured as input
    ANSELBbits.ANSB9 = 0; // RB9 (SW7) disabled analog
    TRISBbits.TRISB14 = 0; // RB14 (Speaker) configured as output
    ANSELBbits.ANSB14 = 0; // RB14 (Speaker) disabled analog

    PORTA &= 0xff00; //Set all LED's off
    
    //LCD control loop
    CONTROL();
    for(int i=0 ; i < 6 ; i++){
        PORTE=control[i];
        ENABLE();
    }
    
    while(1)
    {
        clearScreen();
        PORTA &= 0xff00; //Set all LEDs to 0
        
        //Counter - SW0
        if(PORTFbits.RF3 && !PORTBbits.RB9) {
            count();
            clearScreen();
        }

        //Shift - SW1
        if(PORTFbits.RF5 && !PORTBbits.RB9){
            shift();
            clearScreen();
        }

        //Fan - SW2
        if(PORTFbits.RF4 && !PORTBbits.RB9){
            fan();
            clearScreen();
        }
        
        //Speaker - SW6
        if(PORTBbits.RB10 && !PORTBbits.RB9){
            makeSound();
            clearScreen();
        }
        
        //Exit - SW7
        if(PORTBbits.RB9){
            PORTA &= 0xff00;
            return;
        }
    }
}

void count(){
    int botPos = SECOND_LINE_END; //start on bottom line right
    char topMsg[] = "Mode 0: \0";
    printTopLine(topMsg,strlen(topMsg));
    
    //while SW0 is on
    while(PORTFbits.RF3 && !PORTBbits.RB9){
       
        //check PAUSE switch - SW05
        while(PORTBbits.RB11);
       
        //check priority ( SW0 < SW1 < SW2)
        if(PORTFbits.RF4){ 
            fan();
            break;
        }else if(PORTFbits.RF5){ 
            shift();
            break;
        }
        
       char botMsg[16] = "Counter \0";
        
        //check direction - SW03
        if(PORTDbits.RD15){
            PORTA--;
            strcat(botMsg,"Down \0");
        }else{
            PORTA++;
            strcat(botMsg,"Up \0");
        }
        
        //check SW04 - determine the speed and the pace string
        strcat(botMsg,determineSpeed());
        
        printScreen(botMsg,strlen(botMsg),botPos);
        botPos--; //move bottom print position left once
        if(botPos == FIRST_LINE_END) botPos = SECOND_LINE_END;
        
        //delay
        for(int i=0 ; i < speed ; i++);   
    }
}

void shift(){
    int botPos = SECOND_LINE_END; //start on bottom line right
    char topMsg[] = "Mode 1: \0";
    char direction[] = "DIRECTION \0";   
    printTopLine(topMsg,strlen(topMsg));

    PORTA &= 0xff00; //Set all LED's off 
    
    //while SW1 is on
    while(PORTFbits.RF5 && !PORTBbits.RB9){
        
        char botMsg[16] = "Shift \0";
            
        //check SW03 - direction
        if(PORTDbits.RD15 && ((PORTA & 0x00ff) == 0x0000) ){
            strcpy(direction,"Right \0");
            PORTA |= 0x80;
        }else if((PORTA & 0x00ff) == 0x0000){
            strcpy(direction,"Left \0");
            PORTA |= 0x01;
        }
        
        strcat(botMsg,direction); //concatenate direction str
        
        //check priority ( SW0 < SW1 < SW2)
        if(PORTFbits.RF4){
            fan();
            break;
        }
            
        //check PAUSE switch - SW05
        while(PORTBbits.RB11);
            
        //check SW04 - determine speed and concatenate string
        strcat(botMsg,determineSpeed());
       
        //print bottom line 
        printScreen(botMsg,strlen(botMsg),botPos);
        botPos--;
        if(botPos == FIRST_LINE_END) botPos = SECOND_LINE_END;
            
        //delay
        for(int j=0 ; j < speed ; j++);
           
        //check direction - SW03
        if(PORTDbits.RD15){
            PORTA = PORTA>>1;
        }else{
            PORTA = PORTA<<1;
        }
     
        //check SW07 - exit
        if(PORTBbits.RB9) return;
        //}
    }
}

void fan(){
    int comb;
    int botPos = SECOND_LINE_END; //start on bottom line right
    char topMsg[] = "Mode 2: \0";
    printTopLine(topMsg,strlen(topMsg));
    
    //While SW2 is on
    while(PORTFbits.RF4 && !PORTBbits.RB9){
        int right, left;
        
        //check direction - SW03
        if(PORTDbits.RD15){
            right = 0x80;
            left = 0x01;
        }else{ 
            right = 0x08;
            left = 0x10;
        }
        PORTA &= 0xff00;
          
        while((right>0 && !PORTDbits.RD15) || (PORTDbits.RD15 && right >= 0x10)){
            comb = left | right; //next state value
            PORTA = 0xff00 | comb; 
            char botMsg[16] = "Swing \0";
        
            //determine and concatenate direction string
            if(PORTDbits.RD15){
                strcat(botMsg,"Down \0");
            }else{
                 strcat(botMsg,"Up \0");
            }
            
            //check SW04 - determine speed and concatenate pace string
            strcat(botMsg,determineSpeed());
            
            //print bottom line
            printScreen(botMsg, strlen(botMsg), botPos);
            botPos--;
            if( botPos == FIRST_LINE_END ) botPos = SECOND_LINE_END;
            
            //check PAUSE switch - SW05
            while(PORTBbits.RB11 && !PORTBbits.RB9);
            
            //check SW07 - exit
            if(PORTBbits.RB9) return;
            
            //delay
            for(int j=0 ; j < speed ; j++);
            right = right >> 1;
            left = left << 1;    
        }
    }
    
}

char* determineSpeed(){
    if(PORTDbits.RD14){ 
        speed = 200000;
        soundSpeed = 150000;
        return "Fast \0";
    }else{
        speed = 280000;
        soundSpeed = 300000;
       return "Slow \0";
    }
}

void makeSound(){
    int i=100,j=100;
    
    //while SW06 is on
    while(PORTBbits.RB10 && !PORTBbits.RB9){
        PORTBbits.RB14 ^=1;
 
        //check PAUSE - SW05
        while(PORTBbits.RB11);
        
        for(;j<100+i; j++);
        i+=100;
          
        if(i>320000){
            
            //check SW04 - speed
           //determineSpeed();////////////////////////////////////////////////
            
            for (int k = 0; k< soundSpeed; k++);
            i=100;
            j=10;
        }
        
    }
    
}

void busy(void){
    
    //backup
    char RD,RS;
    int STATUS_TRISE;
    int portMap;
    RD=PORTDbits.RD5;
    RS=PORTBbits.RB15;
    STATUS_TRISE=TRISE;
	PORTDbits.RD5 = 1;  //RW=1 
	PORTBbits.RB15 = 0; //RS=0
    
    //set busy
    portMap = TRISE;
    portMap |= 0x80;
	TRISE = portMap;
    
    do{
        //flip enable
        PORTDbits.RD4=1;
        PORTDbits.RD4=0;
    }while(PORTEbits.RE7);     // BF Register
    
    //restore from backup
    PORTDbits.RD5=RD; 
    PORTBbits.RB15=RS;
    TRISE=STATUS_TRISE;   
}

void printScreen(char string[], int str_len, int pos){ 
    if(pos > SECOND_LINE_END || pos < FIRST_LINE_START) return;
    
    CONTROL();
    
    PORTE &= 0xff00;
    PORTE |= pos;
    ENABLE();
    
    DATA();
    
    for (int i=0; i< str_len ; i++ ){
        PORTE = string[i];
        ENABLE();
    }
    
    PORTE = ' '; //delete last char on previous print
    ENABLE();
}

void delay(){
    for(int i=0 ; i<230000 ; i++);
}

void clearScreen(){
    
    CONTROL();
    
    PORTE &= 0xFF00;     // Clear lower 8 bits of PORTE
    PORTE |= 0x0001;     // Set command 0x01 - clear
    
    ENABLE();
 
}

void printTopLine(char* string, int len){
    clearScreen();
    
    //print top line
    for(int i=0 ; i<16; i++ ){
        printScreen(string, len,FIRST_LINE_END-i); //print from right to left on top line
        for(int j=0; j<100000 ; j++); //delay
    }    
}
