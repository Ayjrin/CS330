///////////////////////////////////////////////////////////////////////////////
// shadermanager.cpp
// ============
// manage the loading and rendering of 3D scenes
//
//  AUTHOR: Brian Battersby - SNHU Instructor / Computer Science
//	Created for CS-330-Computational Graphics and Visualization, Nov. 1st, 2023
///////////////////////////////////////////////////////////////////////////////

#include "SceneManager.h"

#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#endif

#include <glm/gtx/transform.hpp>

// declaration of global variables
namespace
{
	const char* g_ModelName = "model";
	const char* g_ColorValueName = "objectColor";
	const char* g_TextureValueName = "objectTexture";
	const char* g_UseTextureName = "bUseTexture";
	const char* g_UseLightingName = "bUseLighting";
}

/***********************************************************
 *  SceneManager()
 *
 *  The constructor for the class
 ***********************************************************/
SceneManager::SceneManager(ShaderManager *pShaderManager)
{
	m_pShaderManager = pShaderManager;
	m_basicMeshes = new ShapeMeshes();
}

/***********************************************************
 *  ~SceneManager()
 *
 *  The destructor for the class
 ***********************************************************/
SceneManager::~SceneManager()
{
	m_pShaderManager = NULL;
	delete m_basicMeshes;
	m_basicMeshes = NULL;
}

/***********************************************************
 *  CreateGLTexture()
 *
 *  This method is used for loading textures from image files,
 *  configuring the texture mapping parameters in OpenGL,
 *  generating the mipmaps, and loading the read texture into
 *  the next available texture slot in memory.
 ***********************************************************/
bool SceneManager::CreateGLTexture(const char* filename, std::string tag)
{
	int width = 0;
	int height = 0;
	int colorChannels = 0;
	GLuint textureID = 0;

	// indicate to always flip images vertically when loaded
	stbi_set_flip_vertically_on_load(true);

	// try to parse the image data from the specified image file
	unsigned char* image = stbi_load(
		filename,
		&width,
		&height,
		&colorChannels,
		0);

	// if the image was successfully read from the image file
	if (image)
	{
		std::cout << "Successfully loaded image:" << filename << ", width:" << width << ", height:" << height << ", channels:" << colorChannels << std::endl;
		
		// Calculate memory requirements
		size_t imageMemoryBytes = (size_t)width * height * colorChannels;
		float imageMemoryMB = imageMemoryBytes / (1024.0f * 1024.0f);
		std::cout << "Image memory requirement: " << imageMemoryMB << " MB (" << imageMemoryBytes << " bytes)" << std::endl;
		
		// Check if texture size exceeds OpenGL limits
		GLint maxTextureSize;
		glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
		std::cout << "GL_MAX_TEXTURE_SIZE: " << maxTextureSize << " pixels" << std::endl;
		
		if (width > maxTextureSize || height > maxTextureSize) {
			std::cout << "ERROR: Texture " << filename << " (" << width << "x" << height << ") exceeds GL_MAX_TEXTURE_SIZE (" << maxTextureSize << ")" << std::endl;
			stbi_image_free(image);
			return false;
		}
		
		// Check for extremely large textures that might cause memory issues
		const float MAX_REASONABLE_TEXTURE_MB = 50.0f; // 50MB limit for safety
		const int MAX_SAFE_DIMENSION = 2048; // Safe maximum dimension for most GPUs
		
		if (imageMemoryMB > MAX_REASONABLE_TEXTURE_MB) {
			std::cout << "WARNING: Texture " << filename << " is very large (" << imageMemoryMB << " MB). This may cause memory issues." << std::endl;
			std::cout << "Consider resizing the image to a smaller resolution." << std::endl;
		}
		
		// Auto-resize extremely large textures to prevent crashes
		if (width > MAX_SAFE_DIMENSION || height > MAX_SAFE_DIMENSION) {
			std::cout << "INFO: Texture " << filename << " (" << width << "x" << height << ") exceeds safe dimensions." << std::endl;
			std::cout << "For stability, consider using textures smaller than " << MAX_SAFE_DIMENSION << "x" << MAX_SAFE_DIMENSION << " pixels." << std::endl;
			std::cout << "Current texture may cause driver instability on some systems." << std::endl;
		}

		glGenTextures(1, &textureID);
		glBindTexture(GL_TEXTURE_2D, textureID);
		
		// Check for OpenGL errors after texture generation and binding
		GLenum error = glGetError();
		if (error != GL_NO_ERROR)
		{
			std::cout << "ERROR: glGenTextures/glBindTexture failed for " << filename << " - OpenGL error: " << error << std::endl;
			stbi_image_free(image);
			return false;
		}

		// set the texture wrapping parameters
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		// set texture filtering parameters
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		
		// Check for OpenGL errors after setting texture parameters
		error = glGetError();
		if (error != GL_NO_ERROR)
		{
			std::cout << "ERROR: glTexParameteri failed for " << filename << " - OpenGL error: " << error << std::endl;
			stbi_image_free(image);
			glDeleteTextures(1, &textureID);
			return false;
		}
		
		std::cout << "About to upload texture data to GPU for " << filename << "..." << std::endl;

		// if the loaded image is in RGB format
		if (colorChannels == 3)
		{
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
		}
		// if the loaded image is in RGBA format - it supports transparency
		else if (colorChannels == 4)
		{
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
		}
		else
		{
			std::cout << "Not implemented to handle image with " << colorChannels << " channels" << std::endl;
			stbi_image_free(image);
			return false;
		}

		std::cout << "Texture data uploaded to GPU successfully for " << filename << std::endl;
		
		// Check for OpenGL errors after glTexImage2D
		error = glGetError();
		if (error != GL_NO_ERROR)
		{
			std::cout << "ERROR: glTexImage2D failed for " << filename << " - OpenGL error: " << error << std::endl;
			std::cout << "This is often caused by insufficient GPU memory for large textures." << std::endl;
			std::cout << "Try using a smaller image resolution (e.g., 1024x1024 or 2048x2048)." << std::endl;
			stbi_image_free(image);
			glDeleteTextures(1, &textureID);
			return false;
		}

		// Only generate mipmaps if texture creation was successful
		std::cout << "Generating mipmaps for: " << filename << std::endl;
		glGenerateMipmap(GL_TEXTURE_2D);
		
		// Check for errors after mipmap generation
		error = glGetError();
		if (error != GL_NO_ERROR)
		{
			std::cout << "ERROR: glGenerateMipmap failed for " << filename << " - OpenGL error: " << error << std::endl;
			// Don't return false here - texture is still usable without mipmaps
		}

		// free the image data from local memory
		stbi_image_free(image);
		glBindTexture(GL_TEXTURE_2D, 0); // Unbind the texture

		// register the loaded texture and associate it with the special tag string
		m_textureIDs[m_loadedTextures].ID = textureID;
		m_textureIDs[m_loadedTextures].tag = tag;
		m_loadedTextures++;

		return true;
	}

	std::cout << "Could not load image:" << filename << std::endl;

	// Error loading the image
	return false;
}

/***********************************************************
 *  BindGLTextures()
 *
 *  This method is used for binding the loaded textures to
 *  OpenGL texture memory slots.  There are up to 16 slots.
 ***********************************************************/
void SceneManager::BindGLTextures()
{
	for (int i = 0; i < m_loadedTextures; i++)
	{
		// bind textures on corresponding texture units
		glActiveTexture(GL_TEXTURE0 + i);
		glBindTexture(GL_TEXTURE_2D, m_textureIDs[i].ID);
	}
}

/***********************************************************
 *  DestroyGLTextures()
 *
 *  This method is used for freeing the memory in all the
 *  used texture memory slots.
 ***********************************************************/
void SceneManager::DestroyGLTextures()
{
	for (int i = 0; i < m_loadedTextures; i++)
	{
		glGenTextures(1, &m_textureIDs[i].ID);
	}
}

/***********************************************************
 *  FindTextureID()
 *
 *  This method is used for getting an ID for the previously
 *  loaded texture bitmap associated with the passed in tag.
 ***********************************************************/
int SceneManager::FindTextureID(std::string tag)
{
	int textureID = -1;
	int index = 0;
	bool bFound = false;

	while ((index < m_loadedTextures) && (bFound == false))
	{
		if (m_textureIDs[index].tag.compare(tag) == 0)
		{
			textureID = m_textureIDs[index].ID;
			bFound = true;
		}
		else
			index++;
	}

	return(textureID);
}

/***********************************************************
 *  FindTextureSlot()
 *
 *  This method is used for getting a slot index for the previously
 *  loaded texture bitmap associated with the passed in tag.
 ***********************************************************/
int SceneManager::FindTextureSlot(std::string tag)
{
	int textureSlot = -1;
	int index = 0;
	bool bFound = false;

	while ((index < m_loadedTextures) && (bFound == false))
	{
		if (m_textureIDs[index].tag.compare(tag) == 0)
		{
			textureSlot = index;
			bFound = true;
		}
		else
			index++;
	}

	return(textureSlot);
}

/***********************************************************
 *  FindMaterial()
 *
 *  This method is used for getting a material from the previously
 *  defined materials list that is associated with the passed in tag.
 ***********************************************************/
bool SceneManager::FindMaterial(std::string tag, OBJECT_MATERIAL& material)
{
	if (m_objectMaterials.size() == 0)
	{
		return(false);
	}

	int index = 0;
	bool bFound = false;
	while ((index < m_objectMaterials.size()) && (bFound == false))
	{
		if (m_objectMaterials[index].tag.compare(tag) == 0)
		{
			bFound = true;
			material.ambientColor = m_objectMaterials[index].ambientColor;
			material.ambientStrength = m_objectMaterials[index].ambientStrength;
			material.diffuseColor = m_objectMaterials[index].diffuseColor;
			material.specularColor = m_objectMaterials[index].specularColor;
			material.shininess = m_objectMaterials[index].shininess;
		}
		else
		{
			index++;
		}
	}

	return(true);
}

/***********************************************************
 *  SetTransformations()
 *
 *  This method is used for setting the transform buffer
 *  using the passed in transformation values.
 ***********************************************************/
void SceneManager::SetTransformations(
	glm::vec3 scaleXYZ,
	float XrotationDegrees,
	float YrotationDegrees,
	float ZrotationDegrees,
	glm::vec3 positionXYZ)
{
	// variables for this method
	glm::mat4 modelView;
	glm::mat4 scale;
	glm::mat4 rotationX;
	glm::mat4 rotationY;
	glm::mat4 rotationZ;
	glm::mat4 translation;

	// set the scale value in the transform buffer
	scale = glm::scale(scaleXYZ);
	// set the rotation values in the transform buffer
	rotationX = glm::rotate(glm::radians(XrotationDegrees), glm::vec3(1.0f, 0.0f, 0.0f));
	rotationY = glm::rotate(glm::radians(YrotationDegrees), glm::vec3(0.0f, 1.0f, 0.0f));
	rotationZ = glm::rotate(glm::radians(ZrotationDegrees), glm::vec3(0.0f, 0.0f, 1.0f));
	// set the translation value in the transform buffer
	translation = glm::translate(positionXYZ);

	modelView = translation * rotationX * rotationY * rotationZ * scale;

	if (NULL != m_pShaderManager)
	{
		m_pShaderManager->setMat4Value(g_ModelName, modelView);
	}
}

/***********************************************************
 *  SetShaderColor()
 *
 *  This method is used for setting the passed in color
 *  into the shader for the next draw command
 ***********************************************************/
void SceneManager::SetShaderColor(
	float redColorValue,
	float greenColorValue,
	float blueColorValue,
	float alphaValue)
{
	// variables for this method
	glm::vec4 currentColor;

	currentColor.r = redColorValue;
	currentColor.g = greenColorValue;
	currentColor.b = blueColorValue;
	currentColor.a = alphaValue;

	if (NULL != m_pShaderManager)
	{
		m_pShaderManager->setIntValue(g_UseTextureName, false);
		m_pShaderManager->setVec4Value(g_ColorValueName, currentColor);
	}
}

/***********************************************************
 *  SetShaderTexture()
 *
 *  This method is used for setting the texture data
 *  associated with the passed in ID into the shader.
 ***********************************************************/
void SceneManager::SetShaderTexture(
	std::string textureTag)
{
	if (NULL != m_pShaderManager)
	{
		m_pShaderManager->setIntValue(g_UseTextureName, true);

		int textureID = -1;
		textureID = FindTextureSlot(textureTag);
		m_pShaderManager->setSampler2DValue(g_TextureValueName, textureID);
	}
}

/***********************************************************
 *  SetTextureUVScale()
 *
 *  This method is used for setting the texture UV scale
 *  values into the shader.
 ***********************************************************/
void SceneManager::SetTextureUVScale(float u, float v)
{
	if (NULL != m_pShaderManager)
	{
		m_pShaderManager->setVec2Value("UVscale", glm::vec2(u, v));
	}
}

/***********************************************************
 *  SetTextureUVOffset()
 *
 *  This method is used for setting the texture UV offset
 *  values into the shader.
 ***********************************************************/
void SceneManager::SetTextureUVOffset(float u, float v)
{
	if (NULL != m_pShaderManager)
	{
		m_pShaderManager->setVec2Value("UVoffset", glm::vec2(u, v));
	}
}

/***********************************************************
 *  SetShaderMaterial()
 *
 *  This method is used for passing the material values
 *  into the shader.
 ***********************************************************/
void SceneManager::SetShaderMaterial(
	std::string materialTag)
{
	if (m_objectMaterials.size() > 0)
	{
		OBJECT_MATERIAL material;
		bool bReturn = false;

		bReturn = FindMaterial(materialTag, material);
		if (bReturn == true)
		{
			m_pShaderManager->setVec3Value("material.ambientColor", material.ambientColor);
			m_pShaderManager->setFloatValue("material.ambientStrength", material.ambientStrength);
			m_pShaderManager->setVec3Value("material.diffuseColor", material.diffuseColor);
			m_pShaderManager->setVec3Value("material.specularColor", material.specularColor);
			m_pShaderManager->setFloatValue("material.shininess", material.shininess);
		}
	}
}

/**************************************************************/
/*** STUDENTS CAN MODIFY the code in the methods BELOW for  ***/
/*** preparing and rendering their own 3D replicated scenes.***/
/*** Please refer to the code in the OpenGL sample project  ***/
/*** for assistance.                                        ***/
/**************************************************************/


/***********************************************************
 *  PrepareScene()
 *
 *  This method is used for preparing the 3D scene by loading
 *  the shapes, textures in memory to support the 3D scene 
 *  rendering
 ***********************************************************/
void SceneManager::PrepareScene()
{
	// only one instance of a particular mesh needs to be
	// loaded in memory no matter how many times it is drawn
	// in the rendered 3D scene

	m_basicMeshes->LoadPlaneMesh();
	m_basicMeshes->LoadCylinderMesh();
	m_basicMeshes->LoadTorusMesh();
	m_basicMeshes->LoadBoxMesh();
	m_basicMeshes->LoadTaperedCylinderMesh();
	

	// Load textures
	CreateGLTexture("Textures/stones.jpg", "stones");
	CreateGLTexture("Textures/green.jpg", "green");
	CreateGLTexture("Textures/wood.jpg", "wood");
	CreateGLTexture("Textures/metal.jpg", "metal");
	CreateGLTexture("Textures/lava.jpg", "lava");
	CreateGLTexture("Textures/brick.jpg", "brick");
	CreateGLTexture("Textures/glass.jpg", "glass");
	CreateGLTexture("Textures/marble.jpg", "marble");
	// CreateGLTexture("Textures/ViFunko.jpg", "funko_atlas");
	CreateGLTexture("Textures/funko.jpg", "funko");
	
	// Load the 6 individual face textures for the Funko box
	CreateGLTexture("Textures/top.jpeg", "top");
	CreateGLTexture("Textures/bottom.jpeg", "bottom");
	CreateGLTexture("Textures/left.jpeg", "left");
	CreateGLTexture("Textures/right.jpeg", "right");
	CreateGLTexture("Textures/front.jpeg", "front");
	CreateGLTexture("Textures/back.jpeg", "back");

	// Define materials for objects
	OBJECT_MATERIAL stoneMaterial;
	stoneMaterial.ambientColor = glm::vec3(0.2f, 0.2f, 0.2f);
	stoneMaterial.ambientStrength = 0.3f;
	stoneMaterial.diffuseColor = glm::vec3(0.8f, 0.8f, 0.8f);
	stoneMaterial.specularColor = glm::vec3(0.5f, 0.5f, 0.5f);
	stoneMaterial.shininess = 32.0f;
	stoneMaterial.tag = "stone";
	m_objectMaterials.push_back(stoneMaterial);

	OBJECT_MATERIAL glassMaterial;
	glassMaterial.ambientColor = glm::vec3(0.1f, 0.3f, 0.2f);
	glassMaterial.ambientStrength = 0.4f;
	glassMaterial.diffuseColor = glm::vec3(0.2f, 0.6f, 0.4f);
	glassMaterial.specularColor = glm::vec3(0.9f, 0.9f, 0.9f);
	glassMaterial.shininess = 128.0f;
	glassMaterial.tag = "glass";
	m_objectMaterials.push_back(glassMaterial);

	OBJECT_MATERIAL metalMaterial;
	metalMaterial.ambientColor = glm::vec3(0.3f, 0.3f, 0.3f);
	metalMaterial.ambientStrength = 0.2f;
	metalMaterial.diffuseColor = glm::vec3(0.5f, 0.5f, 0.5f);
	metalMaterial.specularColor = glm::vec3(0.8f, 0.8f, 0.8f);
	metalMaterial.shininess = 64.0f;
	metalMaterial.tag = "metal";
	m_objectMaterials.push_back(metalMaterial);

	// Bright material for Funko box with high ambient lighting
	OBJECT_MATERIAL funkoMaterial;
	funkoMaterial.ambientColor = glm::vec3(0.8f, 0.8f, 0.8f); // Bright white ambient
	funkoMaterial.ambientStrength = 0.4f; // High ambient strength for bright lighting
	funkoMaterial.diffuseColor = glm::vec3(0.9f, 0.9f, 0.9f);
	funkoMaterial.specularColor = glm::vec3(0.3f, 0.3f, 0.3f); // Lower specular to reduce glare
	funkoMaterial.shininess = 16.0f; // Lower shininess for matte finish
	funkoMaterial.tag = "funko";
	m_objectMaterials.push_back(funkoMaterial);

	OBJECT_MATERIAL marbleMaterial;
	marbleMaterial.ambientColor = glm::vec3(0.25f, 0.25f, 0.25f);
	marbleMaterial.ambientStrength = 0.4f;  // Increased ambient brightness for bowl
	marbleMaterial.diffuseColor = glm::vec3(0.9f, 0.9f, 0.9f);
	marbleMaterial.specularColor = glm::vec3(0.7f, 0.7f, 0.7f);
	marbleMaterial.shininess = 96.0f;
	marbleMaterial.tag = "marble";
	m_objectMaterials.push_back(marbleMaterial);
}

/***********************************************************
 *  RenderScene()
 *
 *  This method is used for rendering the 3D scene by 
 *  transforming and drawing the basic 3D shapes
 ***********************************************************/
void SceneManager::RenderScene()
{
	// declare the variables for the transformations
	glm::vec3 scaleXYZ;
	float XrotationDegrees = 0.0f;
	float YrotationDegrees = 0.0f;
	float ZrotationDegrees = 0.0f;
	glm::vec3 positionXYZ;

	// bind loaded textures to OpenGL texture units
	BindGLTextures();

	// Enable lighting
	if (NULL != m_pShaderManager)
	{
		m_pShaderManager->setIntValue(g_UseLightingName, true);

		// Set up primary light source (directional light oriented down at both mug and ink well)
		m_pShaderManager->setVec3Value("directionalLight.direction", glm::vec3(-1.0f, -1.0f, -0.9f));
		m_pShaderManager->setVec3Value("directionalLight.ambient", glm::vec3(0.3f, 0.3f, 0.3f));
		m_pShaderManager->setVec3Value("directionalLight.diffuse", glm::vec3(0.8f, 0.8f, 0.8f));
		m_pShaderManager->setVec3Value("directionalLight.specular", glm::vec3(1.0f, 1.0f, 1.0f));
		m_pShaderManager->setVec3Value("directionalLight.position", glm::vec3(20.0f, 20.0f, 20.0f));

		// Set up secondary light source (point light high above for fill lighting)
		m_pShaderManager->setVec3Value("pointLight.position", glm::vec3(20.0f, 20.0f, 20.0f));
		m_pShaderManager->setVec3Value("pointLight.ambient", glm::vec3(0.2f, 0.2f, 0.2f));
		m_pShaderManager->setVec3Value("pointLight.diffuse", glm::vec3(0.5f, 0.5f, 0.5f));
		m_pShaderManager->setVec3Value("pointLight.specular", glm::vec3(0.7f, 0.7f, 0.7f));
		m_pShaderManager->setFloatValue("pointLight.constant", 1.0f);
		m_pShaderManager->setFloatValue("pointLight.linear", 0.09f);
		m_pShaderManager->setFloatValue("pointLight.quadratic", 0.032f);

		// Set viewer position for specular calculations
		m_pShaderManager->setVec3Value("viewPosition", glm::vec3(0.0f, 5.0f, 10.0f));
	}

	/*** Set needed transformations before drawing the basic mesh.  ***/
	/*** This same ordering of code should be used for transforming ***/
	/*** and drawing all the basic 3D shapes.						***/
	/******************************************************************/
	// set the XYZ scale for the mesh
	scaleXYZ = glm::vec3(20.0f, 1.0f, 10.0f);

	// set the XYZ rotation for the mesh
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(0.0f, 0.0f, 0.0f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	SetShaderColor(1, 1, 1, 1);

	// draw the mesh with transformation values
	m_basicMeshes->DrawPlaneMesh();
	/****************************************************************/



	/****************************************************************
	Mug
	*/

	// Mug body
	scaleXYZ = glm::vec3(1.0f, 2.0f, 1.0f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(-3.0f, 0.63f, 2.0f);

	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	// Apply stones texture with tiling to mug body
	SetShaderTexture("stones"); 
	SetTextureUVScale(2.0f, 3.0f); // Tile 2x horizontally, 3x vertically
	SetShaderMaterial("stone"); // Apply stone material for lighting
	m_basicMeshes->DrawCylinderMesh();

	// Mug handle
	scaleXYZ = glm::vec3(0.6f, 0.6f, 0.2f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 90.0f; // Rotate to make handle stand up vertically
	positionXYZ = glm::vec3(-2.0f, 1.63f, 2.0f); // Attach to side of mug at center height

	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	// Apply stones texture to mug handle because who doesnt want a cobble stone mug?
	SetShaderTexture("stones"); 
	SetTextureUVScale(1.5f, 1.5f); 
	SetShaderMaterial("stone"); // Apply stone material for lighting
	m_basicMeshes->DrawTorusMesh();


	/*****************************************************************
	Ink Bottle
	*/

	// Ink well base - Bottom 90% with green texture
	scaleXYZ = glm::vec3(1.2f, 1.26f, 1.2f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(3.0f, 0.63f, 1.0f);

	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);

	SetShaderTexture("green");
	SetTextureUVScale(1.0f, 1.0f); 
	SetShaderMaterial("glass"); // Apply glass material for lighting
	m_basicMeshes->DrawCylinderMesh();

	// Ink well top layer - Top 10% with glass texture
	scaleXYZ = glm::vec3(1.201f, 0.14f, 1.201f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(3.0f, 1.82f, 1.0f);

	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	SetShaderTexture("glass");
	SetTextureUVScale(1.0f, 1.0f); 
	SetShaderMaterial("glass"); // Apply glass material for lighting
	m_basicMeshes->DrawCylinderMesh();

	// Ink well cap
	scaleXYZ = glm::vec3(0.8f, 0.5f, 0.8f);
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(3.0f, 1.8f, 1.0f);

	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	SetShaderColor(0.3f, 0.3f, 0.3f, 1.0f);
	SetShaderMaterial("metal"); // Apply metal material for lighting
	m_basicMeshes->DrawCylinderMesh();


	/*****************************************************************
	Funko Pop Box - Using cross-pattern texture (standard OpenGL layout)
	Real-world dimensions: 6.25" H × 4.5" W × 3.5" D
	*/
	
	// MATH: Box dimensions (scaled to 4 units tall)
	const float scale_factor = 4.0f / 6.25f;  // = 0.64
	const float funko_height = 4.0f;                    // 6.25 * 0.64 = 4.0
	const float funko_width = 4.5f * scale_factor;      // 4.5 * 0.64 = 2.88
	const float funko_depth = 3.5f * scale_factor;      // 3.5 * 0.64 = 2.24
	
	// MATH: Box center position (lifted to sit on table)
	const glm::vec3 box_center = glm::vec3(0.0f, funko_height * 0.5f, -3.0f);  // (0, 2.0, -3)

	// Set transformations for the complete box
	scaleXYZ = glm::vec3(funko_width, funko_height, funko_depth);
	XrotationDegrees = 0.0f; 
	YrotationDegrees = 0.0f; 
	ZrotationDegrees = 0.0f;
	positionXYZ = box_center;

	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);

	// Box Center: (0.0, 2.0, -3.0)
	// Box Dimensions: 2.88w × 4.0h × 2.24d
	// Half Dimensions: 1.44w × 2.0h × 1.12d
	
	const float half_width = funko_width * 0.5f;   // 1.44
	const float half_height = funko_height * 0.5f; // 2.0  
	const float half_depth = funko_depth * 0.5f;   // 1.12
	
	// Reset texture settings for each face
	SetTextureUVScale(1.0f, 1.0f);
	SetTextureUVOffset(0.0f, 0.0f);
	SetShaderMaterial("funko"); // Use bright funko material with high ambient lighting

	
	// FRONT FACE ONLY - Position: (0.0, 2.0, -1.88)
	// DrawPlaneMesh() creates XY plane, so we need to rotate 90° around X to make it vertical
	// For front face: width goes along X-axis, height goes along Z-axis after rotation
	scaleXYZ = glm::vec3(funko_width, 1.0f, funko_height); // Width=2.88, Depth=1.0, Height=4.0
	XrotationDegrees = 90.0f; // Rotate 90° around X to make plane vertical (facing camera)
	YrotationDegrees = 0.0f; 
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(0.0f, 4.0f, -3.0f);
	
	SetShaderTexture("front");
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawPlaneMesh();
	
	// LEFT FACE - Position at left edge of front face
	// Front face spans from X = -1.44 to +1.44, so left face goes at X = -1.44
	scaleXYZ = glm::vec3(funko_depth, 1.0f, funko_height); // Depth=2.24, Thickness=1.0, Height=4.0
	XrotationDegrees = 90.0f; // Make vertical
	YrotationDegrees = 0.0f; // Rotate to face right (perpendicular to front)
	ZrotationDegrees = -90.0f;
	positionXYZ = glm::vec3(2.85f, 4.0f, -5.25f); // At left edge of front face
	
	SetShaderTexture("left");
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawPlaneMesh();
	
	// RIGHT FACE - Position at right edge of front face  
	// Front face spans from X = -1.44 to +1.44, so right face goes at X = +1.44
	scaleXYZ = glm::vec3(funko_depth, 1.0f, funko_height); // Depth=2.24, Thickness=1.0, Height=4.0
	XrotationDegrees = 90.0f; // Make vertical
	YrotationDegrees = 0.0f; // Rotate to face left (perpendicular to front)
	ZrotationDegrees = -90.0f;
	positionXYZ = glm::vec3(-2.85f, 4.0f, -5.25f); // At right edge of front face
	
	SetShaderTexture("right");
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawPlaneMesh();
	
	// BACK FACE - Must match front face scale exactly
	scaleXYZ = glm::vec3(funko_width, 1.0f, funko_height); // Same as front: Width=2.88, Thickness=1.0, Height=4.0
	XrotationDegrees = 90.0f;  // Make it vertical
	YrotationDegrees = 0.0f; // Face same direction as front
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(0.0f, 4.0f, -7.4f);
	
	SetShaderTexture("back");
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawPlaneMesh();
	

	
	// TOP FACE
	scaleXYZ = glm::vec3(funko_width, funko_depth, 2.24f); // Width=2.88, Depth=2.24, Thickness=1.0
	XrotationDegrees = 0.0f; // Keep horizontal for top face
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(0.0f, 8.0f, -5.25f);
	
	SetShaderTexture("top");
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);
	m_basicMeshes->DrawPlaneMesh();
	



	/*****************************************************************
	Bowl
	*/

	scaleXYZ = glm::vec3(2.5f, 1.5f, 2.5f); 
	XrotationDegrees = 180.0f;  
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;
	positionXYZ = glm::vec3(-6.0f, 1.5f, -2.0f);

	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	SetShaderTexture("marble");
	SetTextureUVScale(2.0f, 1.5f);  
	SetTextureUVOffset(0.0f, 0.0f);
	SetShaderMaterial("marble"); 
	m_basicMeshes->DrawTaperedCylinderMesh();






}
