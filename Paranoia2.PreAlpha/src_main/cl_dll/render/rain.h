/*
r_weather.cpp - rain and snow code based on original code by BUzer
Copyright (C) 2011 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#ifndef R_WEATHER_H
#define R_WEATHER_H

#define DRIPSPEED			900	// скорость падения капель (пикс в сек)
#define SNOWSPEED			200	// скорость падения снежинок
#define SNOWFADEDIST		80

#define MAX_RAIN_VERTICES		20000 * 4	// snowflakes and waterrings draw as quads
#define MAX_RAIN_INDICES		MAX_RAIN_VERTICES

#define MAXDRIPS			20000	// лимит капель (можно увеличить при необходимости)
#define MAXFX			10000	// лимит дополнительных частиц (круги по воде и т.п.)

#define MODE_RAIN			0
#define MODE_SNOW			1

#define DRIP_SPRITE_HALFHEIGHT	46
#define DRIP_SPRITE_HALFWIDTH		8
#define SNOW_SPRITE_HALFSIZE		3
#define MAX_RING_HALFSIZE		25	// "радиус" круга на воде, до которого он разрастается за секунду	

typedef struct
{
	int		dripsPerSecond;
	float		distFromPlayer;
	float		windX, windY;
	float		randX, randY;
	int		weatherMode;	// 0 - snow, 1 - rain
	float		globalHeight;
} rain_properties;


typedef struct cl_drip
{
	float		birthTime;
	float		minHeight;	// капля будет уничтожена на этой высоте.
	Vector		origin;
	float		alpha;

	float		xDelta;		// side speed
	float		yDelta;
	int		landInWater;
} cl_drip_t;

typedef struct cl_rainfx
{
	float		birthTime;
	float		life;
	Vector		origin;
	float		alpha;
} cl_rainfx_t;

 
void ProcessRain( void );
void ProcessFXObjects( void );
void DrawRain( void );
void DrawFXObjects( void );
void R_DrawWeather( void );
void ResetRain( void );
void InitRain( void );

#endif//R_WEATHER_H