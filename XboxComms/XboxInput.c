#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/joystick.h>
#include <math.h>

#include "XboxInput.h"

#define MAXAXIS 32767
#define DEADZONELX (int)(0.25*MAXAXIS)
#define DEADZONELY (int)(0.25*MAXAXIS)
#define PI 3.14159265

struct js_event xbox;

int lastLX = 0;
int lastLY = 0;

int avgLX = 0;
int avgLY = 0;
double scaledLX = 0.0;
double scaledLY = 0.0;
int angleLX = 0;
int angleLY = 0;

// Opens the xbox controller file descriptor to read the controller input
void xbox_setup(int* fd, char* path) {
    *fd = open(path, O_RDONLY | O_NONBLOCK);

    // The rest is for the user
    if(*fd == -1) {
        perror("Could not find xbox controller at /dev/input/js2");
        exit(0);
    }
    // Prints details of the controller
    int axes=0, buttons=0;
    char name[128];
    ioctl(*fd, JSIOCGAXES, &axes);
    ioctl(*fd, JSIOCGBUTTONS, &buttons);
    ioctl(*fd, JSIOCGNAME(sizeof(name)), &name);
    printf("\n%s\n    %d Axes %d Buttons\n", name, axes, buttons);
}

// Pushes back all current values, pops out the last value, and
// inserts the new value to the front
void push(int buff[], int n) {
	// push back current values
	int i;
	for(i = 0; i < 4; i++) {
		buff[i+1] = buff[i];
	}
	buff[0] = n;
}

// Computes the average of the values in the JSBuffer, with weights as
// follows: 1, 4, 6, 4, 1 with respect to the position of the values
// in the array
int average(int buff[]) {
	return (double)((buff[0]) + 4*(buff[1]) + 6*(buff[2])
					+ 4*(buff[3]) + (buff[4])) / 16.0;
}

// Using the values of the average of LX and LY, computes the magnitude
// and angle of the vector formed by the point. Stores the magnitude
// and angle in the memory location drive and drive+1
void scaleVals(int LX[], int LY[], double* drive) {

	double speed;

	push(LX, lastLX);
	push(LY, lastLY);

	avgLX = average(LX);
	avgLY = (-1) * average(LY);

	// Calculate the scaled values
	if(abs(avgLX) >= DEADZONELX) {
		if(avgLX > 0) {
			scaledLX = (1.0*(avgLX - DEADZONELX) / (MAXAXIS-DEADZONELX));
			angleLX = avgLX - DEADZONELX;
		} else {
			scaledLX = (1.0*(avgLX + DEADZONELX) / (MAXAXIS-DEADZONELX));
			angleLX = avgLX + DEADZONELX;
		}
	} else {
		scaledLX = 0;
		angleLX = 0;
	}

	if(abs(avgLY) >= DEADZONELY) {
		if(avgLY > 0) {
			scaledLY = (1.0*(avgLY - DEADZONELY) / (MAXAXIS-DEADZONELY));
			angleLY = avgLY - DEADZONELY;
		} else {
			scaledLY = (1.0*(avgLY + DEADZONELY) / (MAXAXIS-DEADZONELY));
			angleLY = avgLY + DEADZONELY;
		}
	} else {
		scaledLY = 0;
		angleLY = 0;
	}

	speed = sqrt(scaledLX*scaledLX + scaledLY*scaledLY);
	if(speed > 1.0) *drive = 1.0;
	else *drive = speed;

	*(drive+1) = atan2(angleLY, angleLX) * 180.0 / PI;
}

// Reads the input of the xbox controller, storing the results
// to the given JSBuffers
void readInput(int* xboxfd, int LX[], int LY[], double* drive, int* buttons) {
	int a = read(*xboxfd, &xbox, sizeof(xbox));

	if(a == -1) {

	} else {
		if(xbox.type == 2) { // JOYSTICK
			if(xbox.number == 0) {
				lastLX = xbox.value;
			} else if(xbox.number == 1) {
				lastLY = xbox.value;
			}
		} else if(xbox.type == 1) { // BUTTONS
			*(buttons + xbox.number) = xbox.value;
		}
	}

	scaleVals(LX, LY, drive);
}