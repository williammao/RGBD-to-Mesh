#pragma once
#include <OpenNI.h>
#include "rgbddevice.h"
#include <iostream>
#include <string>
#include <sstream>

using namespace std;
using namespace openni;

class ONIKinectDevice :
	public RGBDDevice
{
protected:
	Device mDevice;
	VideoStream mDepthStream;
	VideoStream mColorStream;

public:
	ONIKinectDevice(void);
	~ONIKinectDevice(void);
	
	DeviceStatus initialize(void)  override;//Initialize 
	DeviceStatus connect(void)	   override;//Connect to any device
	DeviceStatus disconnect(void)  override;//Disconnect from current device
	DeviceStatus shutdown(void) override;
	
	bool hasDepthStream() override;
	bool hasColorStream() override;

	bool createColorStream() override;
	bool createDepthStream() override;

	bool destroyColorStream()  override;
	bool destroyDepthStream()  override;
	

};
