#version 330

#ifdef VERTEX_SHADER
    out vec2 vertex_texcoords;
    
    void main( )
    {
        vec3 quad[4]= vec3[4]( vec3(-1, -1, 1), vec3( 1, -1, 1), vec3(-1,  1, 1), vec3( 1,  1, 1));
        
        gl_Position= vec4(quad[gl_VertexID], 1.0);
        vertex_texcoords= quad[gl_VertexID].xy * 0.5 + 0.5;
    }
#endif

#ifdef FRAGMENT_SHADER
    uniform vec4 color;
    uniform vec3 eyePosition;
    uniform mat4 lightMatrix;
    
    uniform sampler2D gbuffer0; // position (repere local de l'objet)
    uniform sampler2D gbuffer1; // normale (idem)
    
    layout(early_fragment_tests) in;
    
    struct Light
    {
        vec4 position;      // x, y, z (world) + radius
        vec4 emission;
    };
    
    uniform lightBuffer
    {
        Light lights[100];
    };
    
    in vec2 vertex_texcoords;
    out vec4 fragment_color;
    
    void main( )
    {
        vec3 r= vec3(0, 0, 0);
        
        // recuperer la position et la normale du fragment (stocke par la premiere passe).
        
        // parcourir les sources, calculer la quantite totale de lumiere reflechie
        // for(int i= 0; i < 100; i++) { lights[i] }
        
        fragment_color= vec4(r, 1.0);    
    }
#endif
    
