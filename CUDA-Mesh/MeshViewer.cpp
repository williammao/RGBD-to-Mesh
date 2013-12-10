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

const GLuint MeshViewer::quadPositionLocation = 0;
const GLuint MeshViewer::quadTexcoordsLocation = 1;
const char * MeshViewer::quadAttributeLocations[] = { "Position", "Texcoords" };

const GLuint MeshViewer::vbopositionLocation = 0;
const GLuint MeshViewer::vbocolorLocation = 1;
const GLuint MeshViewer::vbonormalLocation = 2;
const char * MeshViewer::vboAttributeLocations[] = { "Position", "Color", "Normal" };

MeshViewer* MeshViewer::msSelf = NULL;


MeshViewer::MeshViewer(RGBDDevice* device, int screenwidth, int screenheight)
{
	msSelf = this;
	mDevice = device;
	mWidth = screenwidth;
	mHeight = screenheight;
	mViewState = DISPLAY_MODE_OVERLAY;
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

	return initOpenGL(argc, argv);

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

	//Init elements
	initTextures();
	initShader();
	initQuad();
	initPBO();
	initFullScreenPBO();

	return DEVICESTATUS_OK;
}



void MeshViewer::initOpenGLHooks()
{
	glutKeyboardFunc(glutKeyboard);
	glutDisplayFunc(glutDisplay);
	glutIdleFunc(glutIdle);
	glutReshapeFunc(glutReshape);	
}


void MeshViewer::initShader()
{

	const char * pass_vert  = "shaders/passVS.glsl";
	const char * color_frag = "shaders/colorFS.glsl";
	const char * depth_frag = "shaders/depthFS.glsl";
	const char * pcbdebug_frag = "shaders/pointCloudBufferDebugFS.glsl";
	const char * pcvbo_vert = "shaders/pointCloudVBO_FS.glsl";
	const char * pcvbo_geom = "shaders/pointCloudVBO_FS.glsl";
	const char * pcvbo_frag = "shaders/pointCloudVBO_FS.glsl";

	//Color image shader
	color_prog = glslUtility::createProgram(pass_vert, NULL, color_frag, quadAttributeLocations, 2);

	//DEPTH image shader
	depth_prog = glslUtility::createProgram(pass_vert, NULL, depth_frag, quadAttributeLocations, 2);

	//Point Cloud Buffer Debug Shader
	pcbdebug_prog = glslUtility::createProgram(pass_vert, NULL, pcbdebug_frag, quadAttributeLocations, 2);

	//Point cloud VBO renderer
	pcvbo_prog = glslUtility::createProgram(pass_vert, NULL, pcbdebug_frag, vboAttributeLocations, 3);
}


void MeshViewer::initTextures()
{
	//Clear textures
	if (depthTexture != 0 || colorTexture != 0 || pointCloudTexture != 0) {
		cleanupTextures();
	}

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

	//Setup position texture
	glBindTexture(GL_TEXTURE_2D, positionTexture);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

	glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA32F , mXRes, mYRes, 0, GL_RGBA, GL_FLOAT,0);

	//Setup normals texture
	glBindTexture(GL_TEXTURE_2D, normalTexture);

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
	//Image space textures
	glDeleteTextures(1, &colorTexture);
	glDeleteTextures(1, &depthTexture);
	glDeleteTextures(1, &positionTexture);
	glDeleteTextures(1, &normalTexture);

	//screen space textures
	glDeleteTextures(1, &pointCloudTexture);
}


void MeshViewer::initPBO()
{
	// Generate a buffer ID called a PBO (Pixel Buffer Object)
	if(imagePBO0){
		glDeleteBuffers(1, &imagePBO0);
	}

	if(imagePBO1){
		glDeleteBuffers(1, &imagePBO1);
	}

	if(imagePBO2){
		glDeleteBuffers(1, &imagePBO2);
	}

	int num_texels = mXRes*mYRes;
	int num_values = num_texels * 4;
	int size_tex_data = sizeof(GLfloat) * num_values;
	glGenBuffers(1,&imagePBO0);
	glGenBuffers(1,&imagePBO1);
	glGenBuffers(1,&imagePBO2);

	// Make this the current UNPACK buffer (OpenGL is state-based)
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, imagePBO0);

	// Allocate data for the buffer. 4-channel float image
	glBufferData(GL_PIXEL_UNPACK_BUFFER, size_tex_data, NULL, GL_DYNAMIC_COPY);
	cudaGLRegisterBufferObject( imagePBO0);

	
	// Make this the current UNPACK buffer (OpenGL is state-based)
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, imagePBO1);

	// Allocate data for the buffer. 4-channel float image
	glBufferData(GL_PIXEL_UNPACK_BUFFER, size_tex_data, NULL, GL_DYNAMIC_COPY);
	cudaGLRegisterBufferObject( imagePBO1);

	
	// Make this the current UNPACK buffer (OpenGL is state-based)
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, imagePBO2);

	// Allocate data for the buffer. 4-channel float image
	glBufferData(GL_PIXEL_UNPACK_BUFFER, size_tex_data, NULL, GL_DYNAMIC_COPY);
	cudaGLRegisterBufferObject( imagePBO2);
}


void MeshViewer::initFullScreenPBO()
{
	// Generate a buffer ID called a PBO (Pixel Buffer Object)
	if(fullscreenPBO){
		glDeleteBuffers(1, &fullscreenPBO);
	}

	int num_texels = mWidth*mHeight;
	int num_values = num_texels * 4;
	int size_tex_data = sizeof(GLfloat) * num_values;
	glGenBuffers(1,&fullscreenPBO);

	// Make this the current UNPACK buffer (OpenGL is state-based)
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, fullscreenPBO);

	// Allocate data for the buffer. 4-channel float image
	glBufferData(GL_PIXEL_UNPACK_BUFFER, size_tex_data, NULL, GL_DYNAMIC_COPY);
	cudaGLRegisterBufferObject( fullscreenPBO);
}

void MeshViewer::initQuad() {
	vertex2_t verts [] = { {vec3(-1,1,0),vec2(0,0)},
	{vec3(-1,-1,0),vec2(0,1)},
	{vec3(1,-1,0),vec2(1,1)},
	{vec3(1,1,0),vec2(1,0)}};

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
	glVertexAttribPointer(quadPositionLocation, 3, GL_FLOAT, GL_FALSE,sizeof(vertex2_t),0);
	glVertexAttribPointer(quadTexcoordsLocation, 2, GL_FLOAT, GL_FALSE,sizeof(vertex2_t),(void*)sizeof(vec3));
	glEnableVertexAttribArray(quadPositionLocation);
	glEnableVertexAttribArray(quadTexcoordsLocation);

	//indices
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, device_quad.vbo_indices);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, 6*sizeof(GLushort), indices, GL_STATIC_DRAW);
	device_quad.num_indices = 6;
	//Unplug Vertex Array
	glBindVertexArray(0);
}


//Normalized device coordinates (-1 : 1, -1 : 1) center of viewport, and scale being 
void MeshViewer::drawQuad(GLuint prog, float xNDC, float yNDC, float widthScale, float heightScale, GLuint* textures, int numTextures)
{
	//Setup program and uniforms
	glUseProgram(prog);
	glEnable(GL_TEXTURE_2D);
	glDisable(GL_DEPTH_TEST);

	mat4 persp = mat4(1.0f);//Identity
	mat4 viewmat = mat4(widthScale, 0.0f, 0.0f, 0.0f,
		0.0f, heightScale, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		xNDC, yNDC, 0.0f, 1.0f);


	glUniformMatrix4fv(glGetUniformLocation(prog, "u_projMatrix"),1, GL_FALSE, &persp[0][0] );
	glUniformMatrix4fv(glGetUniformLocation(prog, "u_viewMatrix"),1, GL_FALSE, &viewmat[0][0] );

	//Setup textures
	int location = -1;
	switch(numTextures){
	case 5:
		if ((location = glGetUniformLocation(prog, "u_Texture4")) != -1)
		{
			//has texture
			glActiveTexture(GL_TEXTURE4);
			glBindTexture(GL_TEXTURE_2D, textures[4]);
			glUniform1i(location,0);
		}
	case 4:
		if ((location = glGetUniformLocation(prog, "u_Texture3")) != -1)
		{
			//has texture
			glActiveTexture(GL_TEXTURE3);
			glBindTexture(GL_TEXTURE_2D, textures[3]);
			glUniform1i(location,0);
		}
	case 3:
		if ((location = glGetUniformLocation(prog, "u_Texture2")) != -1)
		{
			//has texture
			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_2D, textures[2]);
			glUniform1i(location,0);
		}
	case 2:
		if ((location = glGetUniformLocation(prog, "u_Texture1")) != -1)
		{
			//has texture
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, textures[1]);
			glUniform1i(location,0);
		}
	case 1:
		if ((location = glGetUniformLocation(prog, "u_Texture0")) != -1)
		{
			//has texture
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, textures[0]);
			glUniform1i(location,0);
		}
	}


	//Draw quad
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


bool MeshViewer::drawColorImageBufferToTexture(GLuint texture)
{
	float4* dptr;
	cudaGLMapBufferObject((void**)&dptr, imagePBO0);
	bool result = drawColorImageBufferToPBO(dptr, mXRes, mYRes);
	cudaGLUnmapBufferObject(imagePBO0);
	if(result){
		//Draw to texture
		glBindBuffer( GL_PIXEL_UNPACK_BUFFER, imagePBO0);
		glBindTexture(GL_TEXTURE_2D, texture);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, mXRes, mYRes, 
			GL_RGBA, GL_FLOAT, NULL);

		glBindTexture(GL_TEXTURE_2D, 0);
	}

	return result;
}

bool MeshViewer::drawDepthImageBufferToTexture(GLuint texture)
{	
	float4* dptr;
	cudaGLMapBufferObject((void**)&dptr, imagePBO0);
	bool result = drawDepthImageBufferToPBO(dptr, mXRes, mYRes);
	cudaGLUnmapBufferObject(imagePBO0);
	if(result){
		//Draw to texture
		glBindBuffer( GL_PIXEL_UNPACK_BUFFER, imagePBO0);
		glBindTexture(GL_TEXTURE_2D, texture);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, mXRes, mYRes, 
			GL_RGBA, GL_FLOAT, NULL);

		glBindTexture(GL_TEXTURE_2D, 0);
	}

	return result;
}

void MeshViewer::drawPCBtoTextures(GLuint posTexture, GLuint colTexture, GLuint normTexture)
{
	float4* dptrPosition;
	float4* dptrColor;
	float4* dptrNormal;
	cudaGLMapBufferObject((void**)&dptrPosition, imagePBO0);
	cudaGLMapBufferObject((void**)&dptrColor, imagePBO1);
	cudaGLMapBufferObject((void**)&dptrNormal, imagePBO2);

	bool result = drawPCBToPBO(dptrPosition, dptrColor, dptrNormal, mXRes, mYRes);

	cudaGLUnmapBufferObject(imagePBO0);
	cudaGLUnmapBufferObject(imagePBO1);
	cudaGLUnmapBufferObject(imagePBO2);
	if(result){
		//Unpack to textures
		glBindBuffer( GL_PIXEL_UNPACK_BUFFER, imagePBO0);
		glBindTexture(GL_TEXTURE_2D, positionTexture);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, mXRes, mYRes, 
			GL_RGBA, GL_FLOAT, NULL);

		glBindBuffer( GL_PIXEL_UNPACK_BUFFER, imagePBO1);
		glBindTexture(GL_TEXTURE_2D, colorTexture);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, mXRes, mYRes, 
			GL_RGBA, GL_FLOAT, NULL);

		glBindBuffer( GL_PIXEL_UNPACK_BUFFER, imagePBO2);
		glBindTexture(GL_TEXTURE_2D, normalTexture);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, mXRes, mYRes, 
			GL_RGBA, GL_FLOAT, NULL);


		glBindTexture(GL_TEXTURE_2D, 0);
	}
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

	cudaDeviceSynchronize();
	//=====RENDERING======
	switch(mViewState)
	{
	case DISPLAY_MODE_DEPTH:
		drawDepthImageBufferToTexture(depthTexture);

		drawQuad(depth_prog, 0, 0, 1, 1, &depthTexture, 1);
		break;
	case DISPLAY_MODE_IMAGE:
		drawColorImageBufferToTexture(colorTexture);

		drawQuad(color_prog, 0, 0, 1, 1, &colorTexture, 1);
		break;
	case DISPLAY_MODE_OVERLAY:
		drawDepthImageBufferToTexture(depthTexture);
		drawColorImageBufferToTexture(colorTexture);


		drawQuad(color_prog, 0, 0, 1, 1, &colorTexture, 1);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);//Alpha blending
		drawQuad(depth_prog, 0, 0, 1, 1, &depthTexture, 1);
		glDisable(GL_BLEND);
		break;
	case DISPLAY_MODE_3WAY_DEPTH_IMAGE_OVERLAY:
		drawDepthImageBufferToTexture(depthTexture);
		drawColorImageBufferToTexture(colorTexture);

		drawQuad(color_prog, -0.5, -0.5, 0.5, 0.5, &colorTexture, 1);
		drawQuad(depth_prog, -0.5,  0.5, 0.5, 0.5, &depthTexture, 1);

		drawQuad(color_prog, 0.5, 0, 0.5, 1, &colorTexture, 1);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);//Alpha blending
		drawQuad(depth_prog, 0.5, 0, 0.5, 1, &depthTexture, 1);
		glDisable(GL_BLEND);
		break;
	case DISPLAY_MODE_PCB_COLOR:
	case DISPLAY_MODE_PCB_POSITION:
	case DISPLAY_MODE_PCB_NORMAL:
		drawPCBtoTextures(positionTexture, colorTexture, normalTexture);
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
	LogDevice* device = NULL;
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
	case '4':
		mViewState = DISPLAY_MODE_3WAY_DEPTH_IMAGE_OVERLAY;
		break;
	case('r'):
		cout << "Reloading Shaders" <<endl;
		initShader();
		break;
	case('p'):
		cout << "Restarting Playback" << endl;
		device = dynamic_cast<LogDevice*>(mDevice);
		if(device != 0) {
			// old was safely casted to LogDevice
			device->restartPlayback();
		}

		break;
	case '=':
		device = dynamic_cast<LogDevice*>(mDevice);
		if(device != 0) {
			// old was safely casted to LogDevice
			newPlayback = device->getPlaybackSpeed()+0.1;
			cout <<"Playback speed: " << newPlayback << endl;
			device->setPlaybackSpeed(newPlayback);		
		}
		break;
	case '-':
		device = dynamic_cast<LogDevice*>(mDevice);
		if(device != 0) {
			// old was safely casted to LogDevice
			newPlayback = device->getPlaybackSpeed()-0.1;
			cout <<"Playback speed: " << newPlayback << endl;
			device->setPlaybackSpeed(newPlayback);		
		}
		break;
	}

}


void MeshViewer::reshape(int w, int h)
{
	mWidth = w;
	mHeight = h;
	glBindFramebuffer(GL_FRAMEBUFFER,0);
	glViewport(0,0,(GLsizei)w,(GLsizei)h);

	initTextures();
	initFullScreenPBO();//Refresh fullscreen PBO for new resolution
}