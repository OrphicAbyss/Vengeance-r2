// Andreas Kirsch (c) 2006

uniform sampler2D diffuseSampler;
uniform samplerCube cubemapSampler;
//uniform sampler2D normalSampler;
//uniform sampler2D specularSampler;

varying vec2 texCoords;
varying vec3 cubemapCoords;

uniform vec3 lightColor;
uniform float lightMaxDistance;
varying vec3 lightDirection;

varying vec3 viewerDirection;

varying vec3 surfaceNormal;

/*
Generic light model function
*/
struct Light {
	vec3 direction;
	vec3 color;
	float maxDistance;
	float specularPower;
};

struct Surface {
	vec3 normal;
	vec4 diffuseFactor;
	vec3 specularFactor;
};
	
vec4 getColor( in Surface surface, in Light light, in vec3 viewerDirection ) {
	vec3 normLightDirection = normalize( light.direction );
	vec3 normViewerDirection = normalize( viewerDirection );
	
	vec3 diffuseColor = surface.diffuseFactor.rgb * max( 0.0, dot( normLightDirection, surface.normal ) );
	
	#if 1
		vec3 specularColor = surface.specularFactor * pow( max( 0.0, dot( normalize( reflect(-normLightDirection, surface.normal ) ), normViewerDirection ) ), light.specularPower );
	#else
		vec3 specularColor = vec3( 0.0 );
	#endif
	
	float lightDistance = length( light.direction );
	float lightFallOff = lightDistance / light.maxDistance;
	float attenuation = 1.0 - lightFallOff * lightFallOff;
	
	const vec3 cubemapModulation = textureCube( cubemapSampler, cubemapCoords ).rgb;
	vec3 finalColor = cubemapModulation * light.color * attenuation * (diffuseColor + specularColor);
	
	return vec4( finalColor, surface.diffuseFactor.a );
}

void main() {
	Light light = Light( lightDirection, lightColor, lightMaxDistance, 32.0 );
	
	vec4 diffuseFactor = texture2D( diffuseSampler, texCoords );
	Surface surface = Surface( normalize( surfaceNormal ), diffuseFactor, min( vec3( diffuseFactor ), vec3( 0.5 ) ) );
	
	gl_FragColor = getColor( surface, light, viewerDirection ) * gl_Color.rgba;
}
