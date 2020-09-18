﻿uniform sampler2D src_tx2Da;
uniform sampler2D src_tx2Db;
uniform sampler2D src_tx2Dc;
uniform sampler2D src_tx2Dd;

uniform int frametype;
uniform int h;
uniform int w;
uniform int approach;

uniform mat4 mMa;
uniform mat4 mMb;
uniform mat4 mMc;
uniform mat4 mMd;

in vec2 uv_coord;
//layout(early_fragment_tests) in;
out vec4 frag_out;

void main()
{
    int line = int(uv_coord.y * h);
    int col = int(uv_coord.x * w);
    vec4 p = vec4(2*uv_coord-vec2(1.0), 0.0, 1.0);
    if(line % 2 == 1){
        if(col % 2 == 1){
            frag_out = texture(src_tx2Dd, 0.5 * (mMd * p).xy + vec2(0.5));
            //frag_out = vec4(1.0, 0.0, 0.0, 1.0);
        }else{
            frag_out = texture(src_tx2Dc, 0.5 * (mMc * p).xy + vec2(0.5));
            //frag_out = vec4(0.0, 1.0, 0.0, 1.0);
        }
    } else {
        if(col % 2 == 0){
            frag_out = texture(src_tx2Da,  0.5 * (mMa * p).xy + vec2(0.5));
            //frag_out = vec4(0.0, 0.0, 1.0, 1.0);
        }else{
            frag_out = texture(src_tx2Db, 0.5 * (mMb * p).xy + vec2(0.5));
            //frag_out = vec4(1.0, 1.0, 1.0, 1.0);
        }  
    }
    //frag_out = mMa * vec4(uv_coord, 0.0 , 1.0);
    //frag_out = vec4(gl_FragCoord.x / w, gl_FragCoord.y / h, 0.0 , 1.0);  
}