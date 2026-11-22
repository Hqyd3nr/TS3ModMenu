// Shared analog input normalization; Sqrt is supplied by the host.
static void FlightStick(SHORT sx,SHORT sy,float& x,float& y){
 float fx=float(sx),fy=float(sy),n=Sqrt(fx*fx+fy*fy);
 if(n<=7849){x=y=0;return;}
 float length=(n-7849)/(32767-7849);if(length>1)length=1;
 x=fx/n*length;y=fy/n*length;
}
static float FlightTrigger(BYTE value){return value>30?float(value-30)/225:0;}
// Xbox A rises; B descends. Simultaneous presses cancel vertical motion.
static float FlightVerticalButtons(WORD buttons){return ((buttons&0x1000)?1.f:0.f)-((buttons&0x2000)?1.f:0.f);}
static WORD flightReleaseButtons;static bool flightWasBlocked;
static WORD FlightAvailableButtons(WORD buttons,bool blocked){
 if(blocked||flightWasBlocked)flightReleaseButtons|=buttons&0x3000;
 flightWasBlocked=blocked;flightReleaseButtons&=buttons;
 return blocked?0:WORD(buttons&~flightReleaseButtons);
}
