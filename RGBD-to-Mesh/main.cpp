/*****************************************************************************
*                                                                            *
*  OpenNI 2.x Alpha                                                          *
*  Copyright (C) 2012 PrimeSense Ltd.                                        *
*                                                                            *
*  This file is part of OpenNI.                                              *
*                                                                            *
*  Licensed under the Apache License, Version 2.0 (the "License");           *
*  you may not use this file except in compliance with the License.          *
*  You may obtain a copy of the License at                                   *
*                                                                            *
*      http://www.apache.org/licenses/LICENSE-2.0                            *
*                                                                            *
*  Unless required by applicable law or agreed to in writing, software       *
*  distributed under the License is distributed on an "AS IS" BASIS,         *
*  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  *
*  See the License for the specific language governing permissions and       *
*  limitations under the License.                                            *
*                                                                            *
*****************************************************************************/
#include <OpenNI.h>
#include "Viewer.h"
#include <iostream>
#include "ONIKinectDevice.h"

using namespace std;

void pause();


class RGBDDeviceListener : public RGBDDevice::DeviceDisconnectedListener,
	public RGBDDevice::DeviceConnectedListener,
	public RGBDDevice::DeviceMessageListener
{
public:
	virtual void onMessage(string msg)
	{
		printf("%s", msg.c_str());
	}

	virtual void onDeviceConnected()
	{
		printf("Device connected\n");
	}

	virtual void onDeviceDisconnected()
	{
		printf("Device disconnected\n");
	}
};


int main(int argc, char** argv)
{

	const char* deviceURI = openni::ANY_DEVICE;
	if (argc > 1)
	{
		deviceURI = argv[1];
	}

	ONIKinectDevice device;
	RGBDDeviceListener deviceStateListener;
	device.addDeviceConnectedListener(&deviceStateListener);
	device.addDeviceDisconnectedListener(&deviceStateListener);
	device.addDeviceMessageListener(&deviceStateListener);

	device.initialize();
	if(DEVICESTATUS_OK != device.connect())
	{
		printf("Could not connect to device");
		device.shutdown();
		pause();
		return 1;
	}

	if(!device.createDepthStream())
	{
		printf("Could not create depth stream\n");
		device.shutdown();
		pause();
		return 2;
	}

	if(!device.createColorStream())
	{
		printf("Could not create color stream\n");
		device.shutdown();
		pause();
		return 3;
	}

	/*


	rc = depth.create(device, openni::SENSOR_DEPTH);
	if (rc == openni::STATUS_OK)
	{
	rc = depth.start();
	if (rc != openni::STATUS_OK)
	{
	printf("SimpleViewer: Couldn't start depth stream:\n%s\n", openni::OpenNI::getExtendedError());
	depth.destroy();
	}
	}
	else
	{
	printf("SimpleViewer: Couldn't find depth stream:\n%s\n", openni::OpenNI::getExtendedError());
	}

	rc = color.create(device, openni::SENSOR_COLOR);
	if (rc == openni::STATUS_OK)
	{
	rc = color.start();
	if (rc != openni::STATUS_OK)
	{
	printf("SimpleViewer: Couldn't start color stream:\n%s\n", openni::OpenNI::getExtendedError());
	color.destroy();
	}
	}
	else
	{
	printf("SimpleViewer: Couldn't find color stream:\n%s\n", openni::OpenNI::getExtendedError());
	}

	if (!depth.isValid() || !color.isValid())
	{
	printf("SimpleViewer: No valid streams. Exiting\n");
	openni::OpenNI::shutdown();

	pause();
	return 2;
	}

	SampleViewer sampleViewer("Simple Viewer", device, depth, color);

	rc = sampleViewer.init(argc, argv);
	if (rc != openni::STATUS_OK)
	{
	openni::OpenNI::shutdown();
	return 3;
	}
	sampleViewer.run();
	*/

	//Tear down scaffolding from the top
	device.destroyColorStream();
	device.destroyDepthStream();
	device.removeDeviceConnectedListener(&deviceStateListener);
	device.removeDeviceDisconnectedListener(&deviceStateListener);
	device.removeDeviceMessageListener(&deviceStateListener);
	
	device.disconnect();
	device.shutdown();
	pause();
	return 0;
}


void pause()
{
	cout << "Press enter to continue..." << endl;
	cin.ignore();
}