
#include "App.h"
#include "Widgets/nvSDLContext.h"

#include "Vec.h"
#include "Mesh.h"
#include "MeshIO.h"

#include "GL/GLSLUniforms.h"
#include "GL/GLQuery.h"
#include "GL/GLTexture.h"
#include "GL/GLFramebuffer.h"
#include "GL/GLBuffer.h"
#include "GL/GLVertexArray.h"
#include "GL/GLBasicMesh.h"
#include "ProgramManager.h"

//! classe utilitaire : permet de construire une chaine de caracteres formatee. cf sprintf.
struct Format
{
    char text[1024];
    
    Format( const char *_format, ... )
    {
        text[0]= 0;     // chaine vide
        
        // recupere la liste d'arguments supplementaires
        va_list args;
        va_start(args, _format);
        vsnprintf(text, sizeof(text), _format, args);
        va_end(args);
    }
    
    ~Format( ) {}
    
    // conversion implicite de l'objet en chaine de caracteres stantard
    operator const char *( )
    {
        return text;
    }
    
    operator std::string( )
    {
        return std::string(text);
    }
};


struct PointLight
{
    gk::glsl::vec4 position;    // x, y, z, radius
    gk::glsl::vec4 emission;
    
    PointLight( ) : position(0.f, 0.f, 0.f, 1.f), emission(1.f, 1.f, 1.f) {}
};

class TP : public gk::App
{
    nv::SdlContext m_widgets;
    
    gk::GLProgram *m_display;
    gk::GLProgram *m_gbuffer;
    gk::GLProgram *m_deferred;
    gk::GLFramebuffer *m_framebuffer;
    
    gk::GLBasicMesh *m_mesh;
    gk::GLBasicMesh *m_points;
    gk::GLBasicMesh *m_quad;
    
    gk::GLQuery *m_time;
    gk::GLQuery *m_fragments;
    
    std::vector<PointLight> m_lights;
    gk::GLBuffer *m_light_buffer;
    
    float m_light_rotate;
    float m_rotate;
    float m_rotateX;
    float m_distance;
    
public:
    // creation du contexte openGL et d'une fenetre
    TP( )
        :
        gk::App()
    {
        // specifie le type de contexte openGL a creer :
        gk::AppSettings settings;
        settings.setGLVersion(3,3);     // version 3.3
        settings.setGLCoreProfile();      // core profile
        settings.setGLDebugContext();     // version debug pour obtenir les messages d'erreur en cas de probleme
        
        // cree le contexte et une fenetre
        if(createWindow(768, 480, settings) < 0)
            closeWindow();
        
        m_widgets.init();
        m_widgets.reshape(windowWidth(), windowHeight());
    }
    
    ~TP( ) {}
    
    // construit arbitrairement une liste de sources dans la boite englobante de la scene
    void initLights( const float scale, const int n )
    {
        m_lights.reserve(n);
        
        PointLight light;
        light.position= gk::glsl::vec4(10,10,10, scale);
        light.emission= gk::glsl::vec4(10,10,10);
        m_lights.push_back( light );
        
        for(int i= 1; i < n; i++)
        {
            PointLight light;
            
            float x= drand48() - 0.5;
            float y= drand48() - 0.5;
            float z= drand48() - 0.5;
            float radius= drand48() * 10.f;
            gk::Vector position(x, y, z);
            
            light.position= gk::glsl::vec4(x * scale, y * scale / 10.f, z * scale, radius);
            light.emission= gk::glsl::vec4(drand48(), 0.5 - drand48() * 0.5, drand48());
            
            m_lights.push_back( light );
        }
    }
    
    int init( )
    {
        gk::programPath("shaders");
        
        m_display= gk::createProgram("core.glsl");
        if(m_display == gk::GLProgram::null())
            return -1;        
        m_gbuffer= gk::createProgram("gbuffer.glsl");
        if(m_gbuffer == gk::GLProgram::null())
            return -1;
        m_deferred= gk::createProgram("deferred.glsl");
        if(m_deferred == gk::GLProgram::null())
            return -1;
        
        // construit un framebuffer avec color0, color1 + depth
        m_framebuffer= gk::createFramebuffer(GL_DRAW_FRAMEBUFFER, windowWidth(), windowHeight(), 
            gk::GLFramebuffer::COLOR0_BIT | gk::GLFramebuffer::COLOR1_BIT | gk::GLFramebuffer::DEPTH_BIT,
            gk::TextureRGB16F, gk::TextureDepth);
        
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
        
        // filtrage des textures
        glActiveTexture(GL_TEXTURE1);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        glActiveTexture(GL_TEXTURE0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        
        // execution d'un fragment sur toute l'image
        m_quad= new gk::GLBasicMesh(GL_TRIANGLE_STRIP, 4);
        
        // charge un mesh
        gk::Mesh *mesh= gk::MeshIO::readOBJ("bigguy.obj");
        if(mesh == NULL)
            return -1;
        
        // construit les buffers et le vertex array buffer
        m_mesh= new gk::GLBasicMesh(GL_TRIANGLES, mesh->indices.size());
        m_mesh->createBuffer(0, mesh->positions);
        m_mesh->createBuffer(1, mesh->texcoords);
        m_mesh->createBuffer(2, mesh->normals);
        m_mesh->createIndexBuffer(mesh->indices); 
        
        // construit la bbox du mesh
        gk::BBox bbox;
        for(unsigned int i= 0; i < mesh->positions.size(); i++)
            bbox.Union(gk::Point(mesh->positions[i]));
        
        // nettoyage
        delete mesh;
        
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        
        // mesure du temps de dessin
        m_time= gk::createTimeQuery();
        // nombre de fragments
        m_fragments= gk::createOcclusionQuery();

        // position initiale camera
        m_distance= 80.f;
        m_rotate= 0.f;
        m_rotateX= 20.f;
        m_light_rotate= 0.f;
        
        // 
        initLights(gk::Distance(bbox.pMin, bbox.pMax) * 10.f, 100);
        
        m_light_buffer= gk::createBuffer(GL_UNIFORM_BUFFER, m_lights);
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, m_light_buffer->name);
        glUniformBlockBinding(m_deferred->name, m_deferred->uniformBuffer("lightBuffer").index, 0);

        // affichage des positions des sources 
        m_points= new gk::GLBasicMesh(GL_POINTS, m_lights.size());
        {
            std::vector<gk::Vec4> positions(m_lights.size());
            for(unsigned int i= 0; i < m_lights.size(); i++)
                positions[i]= gk::Vec4(m_lights[i].position);
            
            m_points->createBuffer(0, positions);
        }
        
        glPointSize(3.5f);
        
        // nettoyage de l'etat opengl
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        
        // ok, tout c'est bien passe
        return 0;
    }
    
    int quit( )
    {
        return 0;
    }

    // a redefinir pour utiliser les widgets.
    void processWindowResize( SDL_WindowEvent& event )
    {
        m_widgets.reshape(event.data1, event.data2);
    }
    
    // a redefinir pour utiliser les widgets.
    void processMouseButtonEvent( SDL_MouseButtonEvent& event )
    {
        m_widgets.processMouseButtonEvent(event);
    }
    
    // a redefinir pour utiliser les widgets.
    void processMouseMotionEvent( SDL_MouseMotionEvent& event )
    {
        m_widgets.processMouseMotionEvent(event);
    }
    
    // a redefinir pour utiliser les widgets.
    void processKeyboardEvent( SDL_KeyboardEvent& event )
    {
        m_widgets.processKeyboardEvent(event);
    }
    
    int update( const int time, const int delta )
    {
        m_light_rotate= m_light_rotate + 0.2f;
        return 1;
    }
    
    int draw( )
    {
        if(key(SDLK_ESCAPE))
            // fermer l'application si l'utilisateur appuie sur ESCAPE
            closeWindow();
        
        if(key('r'))    // recharge et recompile les shaders
        {
            key('r')= 0;
            gk::reloadPrograms();
        }
        
        if(key('c'))    // enregistre l'image opengl
        {
            key('c')= 0;
            gk::writeFramebuffer("deferred.png");
        }
        
        // controle basique de la camera
        if(key(SDLK_LEFT))
            m_rotate-= 1.f;
        if(key(SDLK_RIGHT))
            m_rotate+= 1.f;
        if(key(SDLK_UP))
            m_distance+= 1.f;
        if(key(SDLK_DOWN))
            m_distance-= 1.f;
        
        int x, y;
        int button= SDL_GetRelativeMouseState(&x, &y);
        if(button & SDL_BUTTON(1))
        {
            m_rotate+= x;
            m_rotateX+= y;
        }
        
        // transformations
        gk::Transform model; 
        gk::Transform view= gk::Translate( gk::Vector(0.f, 0.f, -m_distance) ) * gk::RotateY(m_rotate) * gk::RotateX(m_rotateX);
        gk::Transform projection= gk::Perspective(50.f, 1.f, 1.f, 1000.f);
        
        gk::Transform mv= view * model;
        gk::Transform mvp= projection * mv;
        
        // mesurer le temps d'execution
        m_time->begin();
        GLint64 start; glGetInteger64v(GL_TIMESTAMP, &start);
        
    // passe 1 : dessiner les objets dans le framebuffer
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_framebuffer->name);
        GLenum buffers[]= { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
        glDrawBuffers(2, buffers);

        glViewport(0, 0, m_framebuffer->width, m_framebuffer->height);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        glUseProgram(m_gbuffer->name);
        m_gbuffer->uniform("mvpMatrix")= mvp.matrix();
        m_mesh->drawInstanced(100);      // dessine 100 copies de l'objet. le vertex shader le repositionne.

    // passe 2 : eclairer les objets
        // revient au framebuffer par defaut
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glViewport(0, 0, windowWidth(), windowHeight());
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(m_deferred->name);
        
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_framebuffer->texture(gk::GLFramebuffer::COLOR0)->name);
        m_deferred->sampler("gbuffer0")= 0;
        
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, m_framebuffer->texture(gk::GLFramebuffer::COLOR1)->name);
        m_deferred->sampler("gbuffer1")= 1;
        
        m_deferred->uniform("lightMatrix")= gk::RotateY(m_light_rotate).matrix();
        m_deferred->uniform("color")= gk::VecColor(0.5f, 0.5f, 0.5f);
        
        m_fragments->begin();
        m_quad->draw();
        m_fragments->end();
        
        // mesurer le temps d'execution
        GLint64 stop; glGetInteger64v(GL_TIMESTAMP, &stop);     // nano secondes
        m_time->end();
        GLuint64 cpu_time= (stop - start) / 1000;               // conversion en micro secondes
        GLuint64 gpu_time= m_time->result64() / 1000;        // conversion en micro secondes

    // passe 3 : afficher les sources
        glUseProgram(m_display->name);
        m_display->uniform("mvpMatrix")= (mvp * gk::RotateY(m_light_rotate)).matrix();
        m_display->uniform("color")= gk::VecColor(1.f, 0.2f, 0.2f);
        m_points->draw();
        
        // nettoyage
        glUseProgram(0);
        glBindVertexArray(0);
        
        // afficher le temps d'execution
        {
            m_widgets.begin();
            m_widgets.beginGroup(nv::GroupFlags_GrowDownFromLeft);
            
            m_widgets.doLabel(nv::Rect(), Format("cpu time % 6ldus", cpu_time));
            m_widgets.doLabel(nv::Rect(), Format("gpu time % 3ldms % 3ldus", gpu_time / 1000, gpu_time % 1000));
            
            m_widgets.doLabel(nv::Rect(), Format("fragments % 3d / pixels % 3d, ratio % f", 
                m_fragments->result(), windowWidth() * windowHeight(), (float) m_fragments->result() / (float) windowWidth() / (float) windowHeight()));

            m_widgets.endGroup();
            m_widgets.end();
        }
        
        // afficher le dessin
        present();
        return 1;
    }
};


int main( int argc, char **argv )
{
    srand48(0);
    TP app;
    app.run();
    
    return 0;
}

