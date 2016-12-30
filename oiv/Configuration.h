#pragma once
//TODO: define the following using cmake
//MISC
#define OIV_CLIENT_BUILD 0

//Codecs
#define OIV_BUILD_PLUGIN_FREE_IMAGE 1
#define OIV_BUILD_PLUGIN_JPEG 1
#define OIV_BUILD_PLUGIN_PNG 1
#define OIV_BUILD_PLUGIN_PSD 1

//Renderer
// Allow null render for debug purpose, this flag is disabled by default.
#define OIV_ALLOW_NULL_RENDERER 1
// Build the OpenGL cross platform renderer, currently implemented only for windows.
#define OIV_BUILD_RENDERER_GL 1
// Build the Direct3D11 windows renderer.
#define OIV_BUILD_RENDERER_D3D11 1
