attribute vec4 m_attrpos;
attribute vec4 m_attrcol;

varying vec4 m_cord;
varying vec4 m_col;

uniform mat4 m_proj;
uniform mat4 m_model;

void main ()
{
  mat4 mvp = m_proj * m_model;
  gl_Position = mvp * m_attrpos;
  m_col = m_attrcol;
}
