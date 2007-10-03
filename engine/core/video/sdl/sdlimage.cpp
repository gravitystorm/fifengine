/***************************************************************************
 *   Copyright (C) 2005-2007 by the FIFE Team                              *
 *   fife-public@lists.sourceforge.net                                     *
 *   This file is part of FIFE.                                            *
 *                                                                         *
 *   FIFE is free software; you can redistribute it and/or modify          *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA              *
 ***************************************************************************/

// Standard C++ library includes
#include <cassert>
#include <iostream>

// 3rd party library includes

// FIFE includes
// These includes are split up in two parts, separated by one empty line
// First block: files included from the FIFE root src directory
// Second block: files included from the same folder
#include "util/logger.h"
#include "util/rect.h"
#include "util/settingsmanager.h"

#include "sdlblendingfunctions.h"
#include "sdlimage.h"

namespace FIFE {
	static Logger _log(LM_VIDEO);

	SDLImage::SDLImage(SDL_Surface* surface): 
		Image(surface),
		m_last_alpha(255),
		m_finalized(false),
		m_optimize_alpha(false) {
	 }

	SDLImage::~SDLImage() { }


	void SDL_BlitSurfaceWithAlpha( const SDL_Surface* src, const SDL_Rect* srcRect,
	                               SDL_Surface* dst,  SDL_Rect* dstRect, unsigned char alpha ) {
		if( 0 == alpha ) {
			return;
		}

		int screenX, screenY;
		if( dstRect ) {
			screenX = dstRect->x;
			screenY = dstRect->y;
		} else {
			screenX = dst->clip_rect.x;
			screenY = dst->clip_rect.y;
		}

		int width, height, tX, tY;
		if( srcRect ) {
			tX = srcRect->x;
			tY = srcRect->y;
			width = srcRect->w;
			height = srcRect->h;
		} else {
			tX = src->clip_rect.x;
			tY = src->clip_rect.y;
			width = src->clip_rect.w;
			height = src->clip_rect.h;
		}

		// Clipping.
		if( ( screenX >= ( dst->clip_rect.x + dst->clip_rect.w ) ) ||
			( screenY >= ( dst->clip_rect.y + dst->clip_rect.h ) ) ||
			( ( screenX + width ) <= dst->clip_rect.x ) ||
			( ( screenY + height ) <= dst->clip_rect.y ) ) {
			return;
		}

		if( screenX < dst->clip_rect.x ) {
			int dX = dst->clip_rect.x - screenX;
			screenX += dX;
			width -= dX;
			tX += dX;
		}

		if( ( screenX + width ) > ( dst->clip_rect.x + dst->clip_rect.w ) ) {
			int dX = ( screenX + width ) - ( dst->clip_rect.x + dst->clip_rect.w );
			width -= dX;
		}

		if( screenY < dst->clip_rect.y ) {
			int dY = dst->clip_rect.y - screenY;
			screenY += dY;
			height -= dY;
			tY += dY;
		}

		if( ( screenY + height ) > ( dst->clip_rect.y + dst->clip_rect.h ) ) {
			int dY = ( screenY + height ) - ( dst->clip_rect.y + dst->clip_rect.h );
			height -= dY;
		}

		if( ( 0 >= height ) || ( 0 >= width ) ) {
			return;
		}

		SDL_LockSurface( dst );

		unsigned char* srcData = reinterpret_cast< unsigned char* > ( src->pixels );
		unsigned char* dstData = reinterpret_cast< unsigned char* > ( dst->pixels );
		
		// move data pointers to the start of the pixels we're copying
		srcData += tY * src->pitch  + tX * src->format->BytesPerPixel;
		dstData += screenY * dst->pitch + screenX * dst->format->BytesPerPixel;

		switch( src->format->BitsPerPixel ) {
			case 32: {
				switch( dst->format->BitsPerPixel ) {
					case 16: {
						if( 0xFFFF == ( dst->format->Rmask | dst->format->Gmask | dst->format->Bmask ) ) {
							for( int y = height; y > 0; --y ) {
								SDL_BlendRow_RGBA8_to_RGB565( srcData, dstData, alpha, width );
								srcData += src->pitch;
								dstData += dst->pitch;
							}
						}
					}
					break;

					case 24: {
						for( int y = height; y > 0; --y ) {
							SDL_BlendRow_RGBA8_to_RGB8( srcData, dstData, alpha, width );
							srcData += src->pitch;
							dstData += dst->pitch;
						}
					}
					break;

					case 32: {
						for( int y = height; y > 0; --y ) {
							SDL_BlendRow_RGBA8_to_RGBA8( srcData, dstData, alpha, width );
							srcData += src->pitch;
							dstData += dst->pitch;
						}
					}
					break;

					default:
						break;
				}	///< switch( dst->format->BitsPerPixel )
			}
			break;

			case 16: {
				if( 0x000F == src->format->Amask ) {
					if( ( 16 == dst->format->BitsPerPixel ) &&
						( 0xFFFF == ( dst->format->Rmask | dst->format->Gmask | dst->format->Bmask ) ) ) {
						for( int y = height; y > 0; --y ) {
							SDL_BlendRow_RGBA4_to_RGB565( srcData, dstData, alpha, width );
							srcData += src->pitch;
							dstData += dst->pitch;
						}
					}
				}
			}
			break;

			default:
				break;
		}	///< switch( src->format->BitsPerPixel )

		SDL_UnlockSurface( dst );
	}

	void SDLImage::render(const Rect& rect, SDL_Surface* screen, unsigned char alpha) {
		if (alpha == 0) {
			return;
		}

		if (rect.right() < 0 || rect.x > static_cast<int>(screen->w) || rect.bottom() < 0 || rect.y > static_cast<int>(screen->h)) {
			return;
		}
		finalize();

		SDL_Surface* surface = screen;
		SDL_Rect r;
		r.x = rect.x;
		r.y = rect.y;
		r.w = rect.w;
		r.h = rect.h;
		
		if (m_surface->format->Amask == 0) {
			// Image has no alpha channel. This allows us to use the per-surface alpha.
			if (m_last_alpha != alpha) {
				m_last_alpha = alpha;
				SDL_SetAlpha(m_surface, SDL_SRCALPHA | SDL_RLEACCEL, alpha);
			}
			SDL_BlitSurface(m_surface, 0, surface, &r);
		} else {
			if( 255 != alpha ) {
				// Special blitting routine with alpha blending:
				// dst.rgb = ( src.rgb * src.a * alpha ) + ( dst.rgb * (255 - ( src.a * alpha ) ) );
				SDL_BlitSurfaceWithAlpha( m_surface, 0, surface, &r, alpha );
			} else {
				SDL_BlitSurface(m_surface, 0, surface, &r);
			}
		}
	}

	unsigned int SDLImage::getWidth() const {
		return m_surface->w;
	}

	unsigned int SDLImage::getHeight() const {
		return m_surface->h;
	}

	void SDLImage::finalize() {
		if( m_finalized ) {
			return;
		}
		m_finalized = true;
		SDL_Surface *old_surface = m_surface;

		if (m_surface->format->Amask == 0) {
			SDL_SetAlpha(m_surface, SDL_SRCALPHA | SDL_RLEACCEL, 255);
			m_surface = SDL_DisplayFormat(m_surface);
		} else {
			m_optimize_alpha &= SettingsManager::instance()->read<bool>("SDLRemoveFakeAlpha",true);
			if( m_optimize_alpha ) {
				m_surface = optimize(m_surface);
			} else  {
				SDL_SetAlpha(m_surface, SDL_SRCALPHA, 255);
				m_surface = SDL_DisplayFormatAlpha(m_surface);
			}
		}
		SDL_FreeSurface(old_surface);
	}
	void SDLImage::setAlphaOptimizerEnabled(bool optimize) {
		m_optimize_alpha = optimize;
	}

	SDL_Surface* SDLImage::optimize(SDL_Surface* src) {
		// The algorithm is originally by "Tim Goya" <tuxdev103@gmail.com>
		// Few modifications and adaptions by the FIFE team.
		//
		// It tries to determine whether an image with a alpha channel
		// actually uses that. Often PNGs contains an alpha channels
		// as they don't provide a colorkey feature(?) - so to speed
		// up SDL rendering we try to remove the alpha channel.

		// As a reminder: src->format->Amask != 0 here

		int transparent = 0;
		int opaque = 0;
		int semitransparent = 0;
		int alphasum = 0;
		int alphasquaresum = 0;
		bool colors[(1 << 12)];
		memset(colors, 0, (1 << 12) * sizeof(bool));
	
		int bpp = src->format->BytesPerPixel;
		if(SDL_MUSTLOCK(src)) {
			SDL_LockSurface(src);
		}
		/*	In the first pass through we calculate avg(alpha), avg(alpha^2)
			and the number of semitransparent pixels.
			We also try to find a useable color.
		*/
		for(int y = 0;y < src->h;y++) {
			for(int x = 0;x < src->w;x++) {
				Uint8 *pixel = (Uint8 *) src->pixels + y * src->pitch + x * bpp;
				Uint32 mapped = 0;
				switch(bpp) {
					case 1:
						mapped = *pixel;
						break;
					case 2:
						mapped = *(Uint16 *)pixel;
						break;
					case 3:
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
						mapped |= pixel[0] << 16;
						mapped |= pixel[1] << 8;
						mapped |= pixel[2] << 0;
#else
						mapped |= pixel[0] << 0;
						mapped |= pixel[1] << 8;
						mapped |= pixel[2] << 16;
#endif
						break;
					case 4:
						mapped = *(Uint32 *)pixel;
						break;
				}
				Uint8 red, green, blue, alpha;
				SDL_GetRGBA(mapped, src->format, &red, &green, &blue, &alpha);
				if(alpha < 16) {
					transparent++;
				} else if (alpha > 240) {
					opaque++;
					alphasum += alpha;
					alphasquaresum += alpha*alpha;
				} else {
					semitransparent++;
					alphasum += alpha;
					alphasquaresum += alpha*alpha;
				}
				// mark the color as used.
				if( alpha != 0 ) {
					colors[((red & 0xf0) << 4) | (green & 0xf0) | ((blue & 0xf0) >> 4)] = true;
				}
			}
		}
		int avgalpha = (opaque + semitransparent) ? alphasum / (opaque + semitransparent) : 0;
		int alphavariance = 0;

		if(SDL_MUSTLOCK(src)) {
			SDL_UnlockSurface(src);
		}
		alphasquaresum /= (opaque + semitransparent) ? (opaque + semitransparent) : 1;
		alphavariance = alphasquaresum - avgalpha*avgalpha;
		if(semitransparent > ((transparent + opaque + semitransparent) / 8) 
		   && alphavariance > 16) {
			FL_DBG(_log, LMsg("sdlimage")
				<< "Trying to alpha-optimize image. FAILED: real alpha usage. "
				<< " alphavariance=" << alphavariance
				<< " total=" << (transparent + opaque + semitransparent)
				<< " semitransparent=" << semitransparent
				<< "(" << (float(semitransparent)/(transparent + opaque + semitransparent))
				<< ")");
			return SDL_DisplayFormatAlpha(src);
		}

		// check availability of a suitable color as colorkey
		int keycolor = -1;
		for(int i = 0;i < (1 << 12);i++) {
			if(!colors[i]) {
				keycolor = i;
				break;
			}
		}
		if(keycolor == -1) {
			FL_DBG(_log, LMsg("sdlimage") << "Trying to alpha-optimize image. FAILED: no free color");
			return SDL_DisplayFormatAlpha(src);
		}

		SDL_Surface *dst = SDL_CreateRGBSurface(src->flags & ~(SDL_SRCALPHA) | SDL_SWSURFACE,
		                                        src->w, src->h,
		                                        src->format->BitsPerPixel,
		                                        src->format->Rmask,  src->format->Gmask,
		                                        src->format->Bmask, 0);
		bpp = dst->format->BytesPerPixel;
		Uint32 key = SDL_MapRGB(dst->format,
		                        (((keycolor & 0xf00) >> 4) | 0xf),
		                        ((keycolor & 0xf0) | 0xf),
		                        (((keycolor & 0xf) << 4) | 0xf));
		if(SDL_MUSTLOCK(src)) {
			SDL_LockSurface(src);
		}
		if(SDL_MUSTLOCK(dst)) {
			SDL_LockSurface(dst);
		}
		for(int y = 0;y < dst->h;y++) {
			for(int x = 0;x < dst->w;x++) {
				Uint8 *srcpixel = (Uint8 *) src->pixels + y * src->pitch + x * bpp;
				Uint8 *dstpixel = (Uint8 *) dst->pixels + y * dst->pitch + x * bpp;
				Uint32 mapped = 0;
				switch(bpp) {
					case 1:
						mapped = *srcpixel;
						break;
					case 2:
						mapped = *(Uint16 *)srcpixel;
						break;
					case 3:
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
						mapped |= srcpixel[0] << 16;
						mapped |= srcpixel[1] << 8;
						mapped |= srcpixel[2] << 0;
#else
						mapped |= srcpixel[0] << 0;
						mapped |= srcpixel[1] << 8;
						mapped |= srcpixel[2] << 16;
#endif
						break;
					case 4:
						mapped = *(Uint32 *)srcpixel;
						break;
				}
				Uint8 red, green, blue, alpha;
				SDL_GetRGBA(mapped, src->format, &red, &green, &blue, &alpha);
				if(alpha < (avgalpha / 4)) {
					mapped = key;
				} else {
					mapped = SDL_MapRGB(dst->format, red, green, blue);
				}
				switch(bpp) {
					case 1:
						*dstpixel = mapped;
						break;
					case 2:
						*(Uint16 *)dstpixel = mapped;
						break;
					case 3:
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
						dstpixel[0] = (mapped >> 16) & 0xff;
						dstpixel[1] = (mapped >> 8) & 0xff;
						dstpixel[2] = (mapped >> 0) & 0xff;
#else
						dstpixel[0] = (mapped >> 0) & 0xff;
						dstpixel[1] = (mapped >> 8) & 0xff;
						dstpixel[2] = (mapped >> 16) & 0xff;
#endif
						break;
					case 4:
						*(Uint32 *)dstpixel = mapped;
						break;
				}
			}
		}
		if(SDL_MUSTLOCK(dst)) {
			SDL_UnlockSurface(dst);
		}
		if(SDL_MUSTLOCK(src)) {
			SDL_UnlockSurface(src);
		}
		// Using the per surface alpha value does not
		// work out for mostly transparent pixels.
		// Thus disabling the part here - this needs a
		// more complex refactoring.
		// if(avgalpha < 240) {
		//	SDL_SetAlpha(dst, SDL_SRCALPHA | SDL_RLEACCEL, avgalpha);
		//}
		SDL_SetColorKey(dst, SDL_SRCCOLORKEY | SDL_RLEACCEL, key);
		SDL_Surface *convert = SDL_DisplayFormat(dst);
		SDL_FreeSurface(dst);
		FL_DBG(_log, LMsg("sdlimage ")
			<< "Trying to alpha-optimize image. SUCCESS: colorkey is " << key);
		return convert;
	} // end optimize
}
