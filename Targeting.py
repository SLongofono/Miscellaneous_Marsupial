#Filename:      Target.py
#
#Description:   First draft of a driver for my targeting system project.  Uses SimpleCV (Hough circles) to locate the centroid
#               of a circular target, then uses a distance sensor and some static characteristics of the webcam to map pixels
#               of the image to physical space.  The servos are rotated appropriately, and the laser is fired at the coordinates
#               of the target.  Fired is probably not the most appropriate verb for such a small laser, but I'm sticking with it.
#
#Created By:    Stephen Longofono
#               January 15, 2016
#               onofognol@ku.edu
#
#

import RPi.GPIO as GPIO
import time
import math

GPIO.setmode(GPIO.BCM)

# Vertical Servo
GPIO.setup(17, GPIO.OUT)

# Horizontal Servo
GPIO.setup(27, GPIO.OUT)

# Laser
GPIO.setup(22, GPIO.OUT)
GPIO.output(22, False)

#pin, frequency
PWM_OBJ_X = GPIO.PWM(27, 50)
PWM_OBJ_Y = GPIO.PWM(17, 50)

#The neutral position of each servo, a float representing the duty cycle as a percentage.
neutX = 7.5
neutY = 7.5

# The viewing angle of the webcam, as measured from center
viewX = (39.454)*(math.pi/180)
viewY = (31.685)*(math.pi/180)

#Duty Cycle = Pulse Width / Period
#We are modulating at 50 Hz, so we adjust as follows:
#
# Far right should be about a 0.5 ms pulse, so we need
# a .0005*50 = .025 = 2.5% duty
#
# Far left should be about a 2.5 ms pulse, so we need 
# a .0025*50 = .075 = 12.5% duty
#
# Center should be about a 1.5 ms pulse, so we need
# a .0015*50 = .075 = 7.5% duty
# All the above are oriented as viewed looking directly
# into the axis of rotation, with the label below.

# Control Function
#
# Since we know the extrema of our servo rotation, and the 
# servo motor responds linearly, we can define a function
# as such:
# y_1 = (0, 2.5)
# y_2 = (180, 12.5)
# f(x) = (x/18) + 2.5
# With some controls to monitor domain, we can use this
# to directly turn to a specific angle

#####
# Manual testing function for servos
def testRot():
	x = -1.0
	y = -1.0
	while(x<0 or x>180):
		x = float(raw_input("Enter an angle for horizontal rotation (0-180): "))
	while(y<0 or y>180):
		y = float(raw_input("Enter an angle for vertical rotation (0-180): "))
	return x,y

#####
# Returns duty of the given rotation as determined by the control function in the comments above
def getDuty(angle):
	return ((angle/18)+2.5)

#####
# Returns distance in cm as measured by the HC-SR04 ultrasonic distance sensor
def calibrate(trigger, echo):
	try:
		GPIO.setup(trigger, GPIO.OUT)
		GPIO.setup(echo, GPIO.IN)
		GPIO.output(trigger, False)
		time.sleep(2)
		speed = 34300.00 #This is the observed speed of sound at sea level in cm (constant) for use in d = rt
		
		# 10 uSecond pulse initializes ranging
		GPIO.output(trigger, True)
		time.sleep(0.00001)
		GPIO.output(trigger, False)
	
		#monitor for measurement pulse begin
		while GPIO.input(echo)==0:
			start = time.time()

		#monitor for measurement pulse end
		while GPIO.input(echo)==1:
			end = time.time()

		#divide total time in half for one-way time elapsed
		elapsed = ((end-start)/2)
		return (speed * elapsed)
	except:
		return None

#####
# Uses the Hough circles algorithm to identify a circle, then returns the coordinates of its center
def getCircles():
	from SimpleCV import Image, Color
	import os
	os.system("fswebcam -r 640x480 --save capture.png")
	img = Image("capture.png")
	#canny is the sensitivity of the circle finding - adjust downward to find more
	#default 100

	#Thresh is the sensitivity of the edge finder.  default 350

	#Distance adjusts how far concentric circle must be apart to be considered 
	#as distinct circles.

	#returns a list of vectors <x,y,radius>
	circles = img.findCircle(canny=150, thresh=145, distance=15)
	if not circles:
		print "no circles found"
		return None
	return circles[0][0], circles[0][1]


#####
# Calculates the appropriate rotation relative to the distance observed by the sensor.
# viewX and viewY are approximate viewing angles to the maximum X and Y of the images
# captured by the webcam, used here for scaling.
# They can be used to adjust for a skewed target plane if you're feeling particularly bored.

def getRot(viewX, viewY, distance, targetX, targetY):
	maxX = distance * math.tan(viewX)
	maxY = distance * math.tan(viewY)
	print "Height from center: ", maxY
	print "Distance from target plane: ", distance
	pixelWidth = maxY/240
	print "Pixel dimension: ", pixelWidth
	xDist = math.fabs(320-targetX)*pixelWidth
	yDist = math.fabs(240-targetY)*pixelWidth
	print "Panning distance: ", xDist
	print "Titling distance: ", yDist
	xRot = math.atan(xDist/distance)
	yRot = math.atan(yDist/distance)
	xRot = xRot * (180/math.pi)
	yRot = yRot *(180/math.pi)

	print "Pan rotation: ", xRot
	print "Tilt rotation: ",yRot

	#Since the servos have 90 as center, determine which way to rotate and adjust accordingly
	if(targetX < 320):#left of center
		xRot = 90 - xRot		
	else:
		xRot = 90 + xRot
	if(targetY < 240):#below center
		yRot = 90 - yRot
	else:
		yRot = 90 + yRot
	return xRot, yRot 


#########################################################
# Main Entry
print "Calibrating"
distance = calibrate(23, 24)

print "Finding circles..."
#cX, cY = getCircles()
cX = 1
cY = 1
if(cX and cY):
	print "Target found at ", cX, ", ", cY
	try:
	
		while True:
			
			PWM_OBJ_X.start(neutX)
			PWM_OBJ_Y.start(neutY)
			rotX, rotY = getRot(viewX, viewY, distance, cX, cY)
			print "Rotating: "
			print "X: ", rotX
			print "Y: ", rotY
			PWM_OBJ_X.ChangeDutyCycle(getDuty(rotX))
			PWM_OBJ_Y.ChangeDutyCycle(getDuty(rotY))
			
			#Fire the lasers!
			print "Lasers!"
			GPIO.output(22, True)
			time.sleep(3)
			GPIO.output(22, False)				
			cX += 20.0
			cY += 15.0



	except KeyboardInterrupt:
		PWM_OBJ_X.ChangeDutyCycle(neutX)
		PWM_OBJ_Y.ChangeDutyCycle(neutY)
		PWM_OBJ_X.stop()
		PWM_OBJ_Y.stop()

	except TypeError:
		PWM_OBJ_X.ChangeDutyCycle(neutX)
		PWM_OBJ_Y.ChangeDutyCycle(neutY)
		PWM_OBJ_X.stop()
		PWM_OBJ_Y.stop()
		GPIO.cleanup()
		exit(1)

	finally:
		GPIO.cleanup()
		exit(0)
else:
	print "Exiting..."
