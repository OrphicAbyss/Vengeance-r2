// Andreas Kirsch (c) 2006

uniform vec3 lightPosition;
uniform mat4 viewToLightMatrix;

varying vec3 lightDirection;
varying vec3 viewerDirection;
varying vec3 surfaceNormal;
varying vec2 texCoords;
varying vec3 cubemapCoords;

void main() {	
	float temp;
	gl_Position = ftransform();	
	
	texCoords = vec2( gl_MultiTexCoord0 );
	
	const vec4 eyeVector = gl_ModelViewMatrix * gl_Vertex;	
	const vec4 lightVector = viewToLightMatrix * eyeVector;
	cubemapCoords = vec3( lightVector );
	
	lightDirection = lightPosition - vec3( eyeVector );
	viewerDirection = vec3( 0.0 ) - vec3( eyeVector );
	surfaceNormal = gl_NormalMatrix * gl_Normal;
	gl_FrontColor = gl_Color;
}