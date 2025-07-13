#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include "I2C.h"    //I2C header file

#define F_CPU 1000000UL

//define buttons
#define DOWN_BUTTON PD2       //INT0
#define UP_BUTTON PD3         //INT1
#define RIGHT_BUTTON PD4      //Start buttons
#define LEFT_BUTTON PD5


//Variables to track button presses
volatile uint8_t downButtonPressed = 0;
volatile uint8_t upButtonPressed = 0;



//Parameters of games

volatile long frameStepMs = 300;
volatile long prevFrame = 0;
volatile long totalFrame = 0;
volatile uint8_t gameRunning = 0;
volatile unsigned long gameStartTime = 0;  //Variable to store the start time of game

//game components
char car = '>';		//represent the car.
char obstacle = 'O';	//represent obstacle.
char contents[2][16] = { ' ' };	//2D array represent the LCD screen contents
int carPosRow = 0;
int carPosCol = 0;

void sceneRender() {
    LCD_Command(0x80); //set cursor to the beginning of the first row
    for (int i = 0; i < 16; i++) {
        LCD_Char(contents[0][i]);
    }
    LCD_Command(0xC0); //set cursor to the beginning of the second row
    for (int i = 0; i < 16; i++) {
        LCD_Char(contents[1][i]);
    }
    _delay_ms(10);
}



//show countdown before game starts

void showCountdown() {
    for (int i = 3; i >= 0; i--) {
        LCD_Command(0x01); //clear display
        LCD_String("Starting in...");
        LCD_Command(0xC0); //move to the second line
        LCD_Char('0' + i); //display countdown
        _delay_ms(1000);
    }
    LCD_Command(0x01); //dlear display
}

//show countdown after score display
void showCountdown2() {
    for (int i = 3; i >= 0; i--) {
        LCD_Command(0x01); //clear display
        LCD_String("Restarting...");
        LCD_Command(0xC0); //move to the second line
        LCD_String("Start in ");
        LCD_Char('0' + i);
        _delay_ms(1000);
    }
    LCD_Command(0x01); //clear display
}

//Restart the game
void restartGame() {
    //reset game variables
    gameRunning = 1;
    frameStepMs = 300;
    prevFrame = 0;
    totalFrame = 0;
    gameStartTime = millis();

//clear game
    for (int r = 0; r < 2; r++) {
        for (int c = 0; c < 16; c++) {
            contents[r][c] = ' ';
        }
    }

//place car in initial position
    carPosRow = 0;
    carPosCol = 0;
    contents[carPosRow][carPosCol] = car;

	//Render initial scene
    sceneRender();
}

void showDeathMessage() {
    LCD_Command(0x01); //clear display
    LCD_String("Car crashed...");
    LCD_Command(0xC0); //move to the second line
    LCD_String("Score: ");

//Calculate and display the elapsed time as score
    unsigned long elapsedTime = (millis() - gameStartTime) / 100;
    char scoreStr[10];
    itoa(elapsedTime, scoreStr, 10);
    LCD_String(scoreStr);

    _delay_ms(3000);
    showCountdown2(); // Display countdown for restart

    restartGame();	//restart game
}

void updateScene() {
	//check for collision
    for (int r = 0; r < 2; r++) {
        if (contents[r][carPosCol] == car && contents[r][carPosCol + 1] != ' ') {
            showDeathMessage();
            return;
        }
    }

    //shift obstacles left
    for (int r = 0; r < 2; r++) {
        for (int c = 0; c < 15; c++) {
            if (contents[r][c] != car) {
                contents[r][c] = contents[r][c + 1];
            }
        }
    }

    //generate new obstacle in the rightmost column
    int num = rand() % 4;
    if (num == 0) {
        contents[0][15] = obstacle;
        contents[1][15] = ' ';
    } else if (num == 1) {
        contents[0][15] = ' ';
        contents[1][15] = obstacle;
    } else {
        contents[0][15] = ' ';
        contents[1][15] = ' ';
    }
}

//move the car between two rows.
void moveCarDown() {
    if (carPosRow == 0) {
        contents[carPosRow][carPosCol] = ' ';
        carPosRow = 1;
        contents[carPosRow][carPosCol] = car;
    }
}

void moveCarUp() {
    if (carPosRow == 1) {
        contents[carPosRow][carPosCol] = ' ';
        carPosRow = 0;
        contents[carPosRow][carPosCol] = car;
    }
}

//Wait for either the left or right button to start the game
void waitForStart() {
    LCD_Command(0x01); // Clear display
    LCD_String("Welcome.. Press");
    LCD_Command(0xC0);
    LCD_String("Lft/Rgt to start");

    // Wait until either left or right button is pressed
    while ((PIND & (1 << LEFT_BUTTON)) && (PIND & (1 << RIGHT_BUTTON))) {
        _delay_ms(100);
    }

    showCountdown(); //show countdown before starting
}

// Interrupt service routines for button presses
ISR(INT0_vect) {  //ISR for DOWN button (PD2)
    downButtonPressed = 1;
}

ISR(INT1_vect) {  //ISR for UP button (PD3)
    upButtonPressed = 1;
}

void setup() {
    LCD_Init();
    waitForStart();  //wait for player to press start button and countdown

    //Set button pins as input with pull up
    DDRD &= ~((1 << DOWN_BUTTON) | (1 << UP_BUTTON) | (1 << LEFT_BUTTON) | (1 << RIGHT_BUTTON));
    PORTD |= (1 << DOWN_BUTTON) | (1 << UP_BUTTON) | (1 << LEFT_BUTTON) | (1 << RIGHT_BUTTON);

    //Enable interrupts
    EIMSK |= (1 << INT0) | (1 << INT1);  //enable INT0 and INT1
    EICRA |= (1 << ISC01) | (1 << ISC11); //falling edge triggers

    sei();  //Enable global interrupts

    //Initialize game start time and start game
    gameStartTime = millis();
    restartGame();  //set up initial game state
}

void loop() {
    if (downButtonPressed) {
        downButtonPressed = 0;
        if (gameRunning) moveCarDown();
    }
    if (upButtonPressed) {
        upButtonPressed = 0;
        if (gameRunning) moveCarUp();
    }

//handle frame updates
    long frame = millis();
    if (frame - prevFrame >= frameStepMs) {
        updateScene();
        totalFrame++;
        prevFrame = frame;
        if (totalFrame % 10 == 0 && frameStepMs > 200) {
            frameStepMs -= 20;
        }
    }

	//render the scene if the game is running
    if (gameRunning) {
        sceneRender();
    }
}


//main function
int main(void) {
    setup();  //initialize and display

    while (1) {
        loop();  //run the game
    }

    return 0;
}