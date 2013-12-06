#include "MeshViewer.h"

//Platform specific code goes here
#include <GL/glew.h>
#include <GL/glut.h>

void MeshViewer::glutIdle()
{
	glutPostRedisplay();
}

void MeshViewer::glutDisplay()
{
	MeshViewer::msSelf->display();
}

void MeshViewer::glutKeyboard(unsigned char key, int x, int y)
{
	MeshViewer::msSelf->onKey(key, x, y);
}


void MeshViewer::glutReshape(int w, int h)
{
	MeshViewer::msSelf->reshape(w, h);
}


//End platform specific code


MeshViewer* MeshViewer::msSelf = NULL;


MeshViewer::MeshViewer(RGBDDevice* device, int screenwidth, int screenheight)
{
	msSelf = this;
	mDevice = device;
	mWidth = screenwidth;
	mHeight = screenheight;
}


MeshViewer::~MeshViewer(void)
{
	msSelf = NULL;
}

DeviceStatus MeshViewer::init(int argc, char **argv)
{
	//Stream Validation
	if (mDevice->isDepthStreamValid() && mDevice->isColorStreamValid())
	{

		int depthWidth = mDevice->getDepthResolutionX();
		int depthHeight = mDevice->getDepthResolutionY();
		int colorWidth = mDevice->getColorResolutionX();
		int colorHeight = mDevice->getColorResolutionY();

		if (depthWidth == colorWidth &&
			depthHeight == colorHeight)
		{
			mXRes = depthWidth;
			mYRes = depthHeight;

			printf("Color and depth same resolution: D: %dx%d, C: %dx%d\n",
				depthWidth, depthHeight,
				colorWidth, colorHeight);
		}
		else
		{
			printf("Error - expect color and depth to be in same resolution: D: %dx%d, C: %dx%d\n",
				depthWidth, depthHeight,
				colorWidth, colorHeight);
			return DEVICESTATUS_ERROR;
		}
	}
	else if (mDevice->isDepthStreamValid())
	{
		mXRes = mDevice->getDepthResolutionX();
		mYRes = mDevice->getDepthResolutionY();
	}
	else if (mDevice->isColorStreamValid())
	{
		mXRes = mDevice->getColorResolutionX();
		mYRes = mDevice->getColorResolutionY();
	}
	else
	{
		printf("Error - expects at least one of the streams to be valid...\n");
		return DEVICESTATUS_ERROR;
	}

	//Register frame listener
	mDevice->addNewRGBDFrameListener(this);
	initCuda(mXRes, mYRes);

	DeviceStatus status = initOpenGL(argc, argv);
	if(status == DEVICESTATUS_OK)
	{
		initQuad();
	}
	return status;
}



DeviceStatus MeshViewer::initOpenGL(int argc, char **argv)
{
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
	glutInitWindowSize(mWidth, mHeight);
	glutCreateWindow ("CUDA Point Cloud to Mesh");

	//Setup callbacks
	initOpenGLHooks();


	// Init GLEW
	glewInit();
	GLenum err = glewInit();
	if (GLEW_OK != err)
	{
		// Problem: glewInit failed, something is seriously wrong.
		std::cout << "glewInit failed, aborting." << std::endl;
		return DEVICESTATUS_ERROR;
	}

	//Init textures
	initTextures();

	return DEVICESTATUS_OK;
}



void MeshViewer::initOpenGLHooks()
{
	glutKeyboardFunc(glutKeyboard);
	glutDisplayFunc(glutDisplay);
	glutIdleFunc(glutIdle);
	glutReshapeFunc(glutReshape);	
}


void MeshViewer::initTextures()
{
	glGenTextures(1, &depthTexture);
	glGenTextures(1, &colorTexture);
	glGenTextures(1, &pointCloudTexture);

	//Setup depth texture
	glBindTexture(GL_TEXTURE_2D, depthTexture);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_DEPTH_TEXTURE_MODE, GL_INTENSITY);

	glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA32F, mXRes, mYRes, 0, GL_RGBA, GL_FLOAT, 0);

	//Setup color texture
	glBindTexture(GL_TEXTURE_2D, colorTexture);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

	glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA32F , mXRes, mYRes, 0, GL_RGBA, GL_FLOAT,0);

	//Setup point cloud texture
	glBindTexture(GL_TEXTURE_2D, pointCloudTexture);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

	glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA32F , mWidth, mHeight, 0, GL_RGBA, GL_FLOAT,0);

}


void MeshViewer::cleanupTextures()
{
	glDeleteTextures(1, &colorTexture);
	glDeleteTextures(1, &depthTexture);
	glDeleteTextures(1, &pointCloudTexture);
}


void MeshViewer::initQuad() {
	vertex2_t verts [] = { {vec3(-1,1,0),vec2(0,1)},
	{vec3(-1,-1,0),vec2(0,0)},
	{vec3(1,-1,0),vec2(1,0)},
	{vec3(1,1,0),vec2(1,1)}};

	unsigned short indices[] = { 0,1,2,0,2,3};

	//Allocate vertex array
	//Vertex arrays encapsulate a set of generic vertex attributes and the buffers they are bound too
	//Different vertex array per mesh.
	glGenVertexArrays(1, &(device_quad.vertex_array));
	glBindVertexArray(device_quad.vertex_array);


	//Allocate vbos for data
	glGenBuffers(1,&(device_quad.vbo_data));
	glGenBuffers(1,&(device_quad.vbo_indices));

	//Upload vertex data
	glBindBuffer(GL_ARRAY_BUFFER, device_quad.vbo_data);
	glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
	//Use of strided data, Array of Structures instead of Structures of Arrays
	glVertexAttribPointer(quad_attributes::POSITION, 3, GL_FLOAT, GL_FALSE,sizeof(vertex2_t),0);
	glVertexAttribPointer(quad_attributes::TEXCOORD, 2, GL_FLOAT, GL_FALSE,sizeof(vertex2_t),(void*)sizeof(vec3));
	glEnableVertexAttribArray(quad_attributes::POSITION);
	glEnableVertexAttribArray(quad_attributes::TEXCOORD);

	//indices
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, device_quad.vbo_indices);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, 6*sizeof(GLushort), indices, GL_STATIC_DRAW);
	device_quad.num_indices = 6;
	//Unplug Vertex Array
	glBindVertexArray(0);
}

void MeshViewer::drawQuad(GLuint texture, float xNDC, float yNDC, float widthNDC, float heightNDC)
{
	glBindVertexArray(device_quad.vertex_array);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, device_quad.vbo_indices);

	glDrawElements(GL_TRIANGLES, device_quad.num_indices, GL_UNSIGNED_SHORT,0);

	glBindVertexArray(0);
}

//Does not return;
void MeshViewer::run()
{
	glutMainLoop();
}



////All the important runtime stuff happens here:
void MeshViewer::display()
{
	ColorPixelArray localColorArray = mColorArray;
	DPixelArray localDepthArray = mDepthArray;

	glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	//=====CUDA CALLS=====
	//Push buffers
	pushColorArrayToBuffer(localColorArray.get(), mXRes, mYRes);
	pushDepthArrayToBuffer(localDepthArray.get(), mXRes, mYRes);

	//Generate point cloud
	convertToPointCloud();

	//Compute normals
	computePointCloudNormals();

	//=====RENDERING======
	switch(mViewState)
	{
	case DISPLAY_MODE_DEPTH:
		drawDepthImageBufferToTexture(depthTexture, mXRes, mYRes);
		glBlendFunc(GL_ZERO, GL_ONE);//Overwrite
		drawQuad(depthTexture, 0, 0, 1, 1);
		break;
	case DISPLAY_MODE_OVERLAY:
		drawDepthImageBufferToTexture(depthTexture, mXRes, mYRes);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);//Alpha blending
		drawQuad(colorTexture, 0, 0, 1, 1);
		drawQuad(depthTexture, 0, 0, 1, 1);
		break;
	}

	glutSwapBuffers();

}



void MeshViewer::onNewRGBDFrame(RGBDFramePtr frame)
{
	mLatestFrame = frame;
	if(mLatestFrame != NULL)
	{
		if(mLatestFrame->hasColor())
		{
			mColorArray = mLatestFrame->getColorArray();
		}

		if(mLatestFrame->hasDepth())
		{
			mDepthArray = mLatestFrame->getDepthArray();
		}
	}
}

void MeshViewer::onKey(unsigned char key, int /*x*/, int /*y*/)
{
	float newPlayback = 1.0;
	switch (key)
	{
	case 27://ESC
		mDevice->destroyColorStream();
		mDevice->destroyDepthStream();

		mDevice->disconnect();
		mDevice->shutdown();

		cleanupCuda();
		cleanupTextures();
		exit (1);
		break;
	case '1':
		mViewState = DISPLAY_MODE_OVERLAY;
		break;
	case '2':
		mViewState = DISPLAY_MODE_DEPTH;
		break;
	case '3':
		mViewState = DISPLAY_MODE_IMAGE;
		break;

	}

}


void MeshViewer::reshape(int w, int h)
{
	mWidth = w;
	mHeight = h;
	glBindFramebuffer(GL_FRAMEBUFFER,0);
	glViewport(0,0,(GLsizei)w,(GLsizei)h);
	if (depthTexture != 0 || colorTexture != 0 || pointCloudTexture != 0) {
		cleanupTextures();
	}
	initTextures();
}