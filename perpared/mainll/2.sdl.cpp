#include <stdio.h>
#include <SDL.h>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

using namespace glm;

float noise(vec3 p) //Thx to Las^Mercury
{
	vec3 i = floor(p);
	vec4 a = dot(i, vec3(1., 57., 21.)) + vec4(0., 57., 21., 78.);
	vec3 f = cos((p-i)*acos(-1.))*(-.5)+.5;
	a = mix(sin(cos(a)*a),sin(cos(1.+a)*(1.+a)), f.x);
	a.xy = mix(a.xz, a.yw, f.y);
	return mix(a.x, a.y, f.z);
}

float sphere(vec3 p, vec4 spr)
{
	return length(spr.xyz-p) - spr.w;
}

float flame(vec3 p)
{
	float d = sphere(p*vec3(1.,.5,1.), vec4(.0,-1.,.0,1.));
	return d + (noise(p+vec3(.0,iTime*2.,.0)) + noise(p*3.)*.5)*.25*(p.y) ;
}

float scene(vec3 p)
{
	return min(100.-length(p) , abs(flame(p)) );
}

vec4 raymarch(vec3 org, vec3 dir)
{
	float d = 0.0, glow = 0.0, eps = 0.02;
	vec3  p = org;
	bool glowed = false;
	
	for(int i=0; i<64; i++)
	{
		d = scene(p) + eps;
		p += d * dir;
		if( d>eps )
		{
			if(flame(p) < .0)
				glowed=true;
			if(glowed)
       			glow = float(i)/64.;
		}
	}
	return vec4(p,glow);
}

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
	vec2 v = -1.0 + 2.0 * fragCoord.xy / iResolution.xy;
	v.x *= iResolution.x/iResolution.y;
	
	vec3 org = vec3(0., -2., 4.);
	vec3 dir = normalize(vec3(v.x*1.6, -v.y, -1.5));
	
	vec4 p = raymarch(org, dir);
	float glow = p.w;
	
	vec4 col = mix(vec4(1.,.5,.1,1.), vec4(0.1,.5,1.,1.), p.y*.02+.4);
	
	fragColor = mix(vec4(0.), col, pow(glow*2.,4.));
	//fragColor = mix(vec4(1.), mix(vec4(1.,.5,.1,1.),vec4(0.1,.5,1.,1.),p.y*.02+.4), pow(glow*2.,4.));

}



int main(int argc, char *argv[]) {
    int a;
    scanf_s("%d", &a);
    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window * window = SDL_CreateWindow("",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        640, 480,
        SDL_WINDOW_RESIZABLE);

    SDL_Renderer * renderer = SDL_CreateRenderer(window,
        -1, SDL_RENDERER_PRESENTVSYNC);

    int width = 320;
    int height = 240;

    // Since we are going to display a low resolution buffer,
    // it is best to limit the window size so that it cannot
    // be smaller than our internal buffer size.
    SDL_SetWindowMinimumSize(window, width, height);

    SDL_RenderSetLogicalSize(renderer, width, height);
    SDL_RenderSetIntegerScale(renderer, SDL_TRUE);

    SDL_Texture * screen_texture = SDL_CreateTexture(renderer,
        SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING,
        width, height);

    unsigned int * pixels = (unsigned int * )malloc(width * height * 4);

    while (1)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT) exit(0);
        }

        // Set every pixel to white.
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                pixels[x + y * width] = 0xffffffff;
            }
        }

        // It's a good idea to clear the screen every frame,
        // as artifacts may occur if the window overlaps with
        // other windows or transparent overlays.
        SDL_RenderClear(renderer);
        SDL_UpdateTexture(screen_texture, NULL, pixels, width * 4);
        SDL_RenderCopy(renderer, screen_texture, NULL, NULL);
        SDL_RenderPresent(renderer);
    }
    return 0;
}