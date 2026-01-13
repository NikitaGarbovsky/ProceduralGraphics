vec3 CalculateBlinnPhong(
	vec3 _normal,				// Normalized fragment normal
	vec3 _fragPos,				// Fragment world position
	vec3 _cameraPos,				// Camera position
	vec3 _lightPos,				// Light position
	vec3 _lightColor,			// Light color
	float _ambientStrength,		// Ambient intensity
	vec3 _ambientColor,			// Ambient Color
	float _specularStrength,		// Specular intensity
	float _shininess				// Shininess factor
){
	vec3 norm = normalize(_normal);
	vec3 lightDir = normalize(_fragPos - _lightPos);

	// Ambient
	vec3 ambient = _ambientStrength * _ambientColor;

	// Diffuse
	float diffStrength = max(dot(norm, -lightDir), 0.0);
	vec3 diffuse = diffStrength * _lightColor;

	// Specular (Blinn-Phong)
	vec3 viewDir = normalize(_cameraPos - _fragPos);
	vec3 halfwayVec = normalize(-lightDir + viewDir);
	float specReflect = pow(max(dot(norm, halfwayVec), 0.0), _shininess);
	vec3 specular = _specularStrength * specReflect * _lightColor;

	return ambient + diffuse + specular;
}