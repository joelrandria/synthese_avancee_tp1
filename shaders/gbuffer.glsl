#version 330

#ifdef VERTEX_SHADER
    uniform mat4 mvpMatrix;

    layout(location= 0) in vec4 position;
    layout(location= 2) in vec3 normal;

    out vec3 vertex_normal;
    out vec3 vertex_position;

    void main( )
    {
        int x= gl_InstanceID % 10 - 5;
        int z= gl_InstanceID / 10 - 5;
        vec3 translate= vec3(x, 0, z) * 50.f;
        
        gl_Position= mvpMatrix * vec4(position.xyz + translate, 1.0);
        
        vertex_normal= normal;
        vertex_position= position.xyz + translate;
    }
#endif

#ifdef FRAGMENT_SHADER
    in vec3 vertex_position;
    in vec3 vertex_normal;

    layout(early_fragment_tests) in;
    layout(location= 0) out vec3 fragment_position;
    layout(location= 1) out vec3 fragment_normal;
    
    void main( )
    {
        fragment_position= vertex_position;
        fragment_normal= normalize(vertex_normal);
    }
#endif
