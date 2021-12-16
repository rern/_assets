#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <wiringPi.h> 

#define RotateAPin 4 //Define as CLK
#define RotateBPin 5 // Define as DT
#define SwitchPin 1 // Define as Push Button Switch

static volatile int globalCounter = 0 ;

unsigned char flag;
unsigned char Last_RoB_Status;
unsigned char Current_RoB_Status;

void btnISR(void)
{
	globalCounter = 0;
}

void rotaryDeal(void)
{
	Last_RoB_Status = digitalRead(RotateBPin);

	while(!digitalRead(RotateAPin)){
		Current_RoB_Status = digitalRead(RotateBPin);
		flag = 1;
	}

	if(flag == 1){
		flag = 0;
		if((Last_RoB_Status == 0)&&(Current_RoB_Status == 1)){
			globalCounter ++;
		}
		if((Last_RoB_Status == 1)&&(Current_RoB_Status == 0)){
			globalCounter --;
		}
	}
}

int main(void)
{
	if(wiringPiSetup() < 0){
		fprintf(stderr, "Unable to initialize wiringPi:%s\n",strerror(errno));
		return 1;
	}

	pinMode(SwitchPin, INPUT);
	pinMode(RotateAPin, INPUT);
	pinMode(RotateBPin, INPUT);

	pullUpDnControl(SwitchPin, PUD_UP);

	if(wiringPiISR(SwitchPin, INT_EDGE_FALLING, &btnISR) < 0){
		fprintf(stderr, "Unable to init ISR\n",strerror(errno));
		return 1;
	}

	int tmp = 0;

	while(1){
		rotaryDeal();
		if (tmp != globalCounter){
			printf("%d\n", globalCounter);
			tmp = globalCounter;
		}
	}

	return 0;
}
