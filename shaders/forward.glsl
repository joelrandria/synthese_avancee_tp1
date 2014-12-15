#version 330

#ifdef VERTEX_SHADER
    uniform mat4 mvpMatrix;

    layout(location= 0) in vec4 position;
    layout(location= 2) in vec3 normal;

    out vec3 vertex_normal;
    out vec3 vertex_position;

    void main( )
    {
        // instanciation : place les objets sur une grille 10x10 dans le plan xz
        int x= gl_InstanceID % 10 - 5;
        int z= gl_InstanceID / 10 - 5;
        vec3 translate= vec3(x, 0, z) * 50.f;
        
        // transformation
        gl_Position= mvpMatrix * vec4(position.xyz + translate, 1.0);
        
        vertex_normal= normal;
        vertex_position= position.xyz + translate;
    }
#endif

#ifdef FRAGMENT_SHADER
    uniform vec4 color;
    uniform mat4 lightMatrix;

    layout(early_fragment_tests) in;

    struct Light
    {
        vec4 position;      // x, y, z, radius
        vec4 emission;
    };

    uniform lightBuffer
    {
        Light lights[100];
    };

    in vec3 vertex_position;
    in vec3 vertex_normal;

    out vec4 fragment_color;

    void main( )
    {
        vec3 r= vec3(0, 0, 0);
        
        // parcourir les sources, calculer la quantite totale de lumiere reflechie
        // for(int i= 0; i < 100; i++) { lights[i] }
        
        fragment_color= vec4(r, 1.0);    
    }
#endif
