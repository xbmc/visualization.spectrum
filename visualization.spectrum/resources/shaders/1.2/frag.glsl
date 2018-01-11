#ifndef GL_ES
#define lowp
#endif

varying lowp vec4 m_col;

void main ()
{
  gl_FragColor = m_col;
}
