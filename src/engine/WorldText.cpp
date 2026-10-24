#include "engine/WorldText.h"
#include <windows.h>
#include <array>

namespace {
constexpr int ATLAS_SIZE=2048, CELL=112;
struct Glyph { int x=0,y=0,w=0,h=0,left=0,top=0,advance=0; };
struct Atlas {
  std::array<Glyph,256> glyphs{};
  std::vector<u8> pixels;
  float capHeight=58;
  bool ready=false;
  Atlas() {
    HDC dc=CreateCompatibleDC(nullptr);
    if(!dc) return;
    HFONT font=CreateFontW(-80,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
        OUT_TT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"Bahnschrift");
    if(!font) { DeleteDC(dc); return; }
    HGDIOBJ old=SelectObject(dc,font);
    MAT2 identity={{0,1},{0,0},{0,0},{0,1}};
    pixels.assign(ATLAS_SIZE*ATLAS_SIZE,0);
    GLYPHMETRICS capital{};
    if(GetGlyphOutlineW(dc,L'H',GGO_GRAY8_BITMAP,&capital,0,nullptr,&identity)!=GDI_ERROR && capital.gmBlackBoxY)
      capHeight=(float)capital.gmBlackBoxY;
    ready=true;
    for(int cp=32;cp<256;cp++) {
      if(cp>=127 && cp<160) continue;
      GLYPHMETRICS gm{};
      DWORD bytes=GetGlyphOutlineW(dc,cp,GGO_GRAY8_BITMAP,&gm,0,nullptr,&identity);
      if(bytes==GDI_ERROR || gm.gmBlackBoxX>CELL-16 || gm.gmBlackBoxY>CELL-16) {ready=false;continue;}
      Glyph& g=glyphs[cp];
      g.x=((cp-32)%16)*CELL+8; g.y=((cp-32)/16)*CELL+8;
      g.w=(int)gm.gmBlackBoxX;g.h=(int)gm.gmBlackBoxY;
      g.left=gm.gmptGlyphOrigin.x;g.top=gm.gmptGlyphOrigin.y;g.advance=gm.gmCellIncX;
      if(!bytes || !g.w || !g.h) continue;
      std::vector<u8> bitmap(bytes);
      if(GetGlyphOutlineW(dc,cp,GGO_GRAY8_BITMAP,&gm,bytes,bitmap.data(),&identity)==GDI_ERROR) {ready=false;continue;}
      int stride=(g.w+3)&~3;
      for(int y=0;y<g.h;y++) for(int x=0;x<g.w;x++)
        pixels[(g.y+y)*ATLAS_SIZE+g.x+x]=(u8)std::min(255,bitmap[y*stride+x]*255/64);
    }
    SelectObject(dc,old);DeleteObject(font);DeleteDC(dc);
  }
};
const Atlas& GetAtlas() { static const Atlas atlas;return atlas; }
const Glyph& GetGlyph(wchar_t c) { return GetAtlas().glyphs[c>=32 && c<256 ? c : L'?']; }
}

bool WorldTextReady() { return GetAtlas().ready; }
float WorldTextWidth(const char* text,float height) {
  float width=0;auto word=Utf8ToWide(text);
  for(wchar_t c:word) width+=GetGlyph(c).advance+1.5f;
  return std::max(0.0f,width-1.5f)*height/GetAtlas().capHeight;
}

void BuildWorldText(MeshBuilder& b,const char* text,float height,float maxWidth,bool backing) {
  const Atlas& atlas=GetAtlas();
  float scale=height/atlas.capHeight, width=WorldTextWidth(text,height);
  float sx=scale*std::min(1.0f,maxWidth/std::max(width,.001f));
  float x=-std::min(width,maxWidth)*.5f;
  Material previous=b.CurMat();b.Mat(MAT_SIGN);
  for(wchar_t c:Utf8ToWide(text)) {
    const Glyph& g=GetGlyph(c);
    if(g.w && g.h) {
      float x0=x+(g.left-2)*sx,x1=x0+(g.w+4)*sx;
      float y1=-height*.5f+(g.top+2)*scale,y0=y1-(g.h+4)*scale;
      // Ink sits in front of its board; no coincident front/back faces or tiny boxes.
      b.Quad({x0,y0,.008f},{x1,y0,.008f},{x1,y1,.008f},{x0,y1,.008f});
      float u0=(g.x-2.0f)/ATLAS_SIZE,u1=(g.x+g.w+2.0f)/ATLAS_SIZE;
      float v0=(g.y-2.0f)/ATLAS_SIZE,v1=(g.y+g.h+2.0f)/ATLAS_SIZE;
      Vec2 uv[]={{u0,v1},{u1,v1},{u1,v0},{u0,v0}};
      auto& vertices=b.Target()->verts;size_t base=vertices.size()-4;
      for(int i=0;i<4;i++) {vertices[base+i].uv=uv[i];vertices[base+i].aux[3]=backing ? 255 : 0;}
    }
    x+=(g.advance+1.5f)*sx;
  }
  b.Mat(previous);
}

GLuint CreateWorldTextTexture() {
  const Atlas& atlas=GetAtlas();if(!atlas.ready) return 0;
  GLuint texture=0;glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
  // Eight-pixel glyph gutters remain separate through mip 3.
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAX_LEVEL,3);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
  glPixelStorei(GL_UNPACK_ALIGNMENT,1);
  std::vector<u8> pixels=atlas.pixels;
  for(int size=ATLAS_SIZE,level=0;;size/=2,level++) {
    glTexImage2D(GL_TEXTURE_2D,level,GL_R8,size,size,0,GL_RED,GL_UNSIGNED_BYTE,pixels.data());
    if(size==1) break;
    int next=size/2;std::vector<u8> reduced(next*next);
    for(int y=0;y<next;y++) for(int x=0;x<next;x++) {
      int p=y*2*size+x*2;
      reduced[y*next+x]=(u8)((pixels[p]+pixels[p+1]+pixels[p+size]+pixels[p+size+1]+2)/4);
    }
    pixels=std::move(reduced);
  }
  glPixelStorei(GL_UNPACK_ALIGNMENT,4);
  return texture;
}
