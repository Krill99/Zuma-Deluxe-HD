#include "Bullet.h"

void Bullet_Init(Bullet* bullet, float x, float y, float motionVecX, 
	float motionVecY, char color, float speed) {

	bullet->x = x;
	bullet->y = y;
	bullet->motionVecX = motionVecX;
	bullet->motionVecY = motionVecY;
	bullet->color = color;
	bullet->onScreen = 1;
	// The level's firespeed is authored in the original 640-wide space; HD
	// path coordinates are scaled by 1.5.
	bullet->speed = speed * 1.5f;

	SDL_Rect animBallRect = {48 * bullet->color, 0, 48, 48};
	Animation_Init(&bullet->anim, TEX_GAME_OBJECTS, animBallRect);
    Animation_Set(&bullet->anim, 24, 47, 0);
}

void Bullet_Update(Bullet* bullet) {
	bullet->x += bullet->motionVecX * bullet->speed;
	bullet->y += bullet->motionVecY * bullet->speed;
}

void Bullet_Draw(Engine *engine, Bullet* bullet) {
	Animation_Draw(engine, &bullet->anim, bullet->x, bullet->y);
}

void BulletsArr_UpdateOnScreenStatus(Engine *engine, BulletsArr* bulletsArr, int* chainBonus) {
	// Bullet positions are logical coordinates, independent of window size.
	const int w = (int)WINDOW_WIDTH, h = (int)WINDOW_HEIGHT;

	for (int i = 0; i < bulletsArr->len; i++) {
		if (!bulletsArr->bullets[i].onScreen) continue;
		if (bulletsArr->bullets[i].x > w + 50 || 
		    bulletsArr->bullets[i].x < -50
		) {
			bulletsArr->bullets[i].onScreen = 0;
			*chainBonus = 0;
		} 
		
		else if (bulletsArr->bullets[i].y > h + 50 || 
				   bulletsArr->bullets[i].y < -50
		) {
			bulletsArr->bullets[i].onScreen = 0;
			*chainBonus = 0;
		}
	}
}

void BulletsArr_AddBullet(BulletsArr* bulletsArr, Frog* frog, float speed) {
	for (int i = 0; i < bulletsArr->len; i++) {
		if (!bulletsArr->bullets[i].onScreen) {
			Bullet_Init(
				&bulletsArr->bullets[i], 
				frog->x + frog->forwardX*FROG_CUR_BALL_PAD, 
				frog->y + frog->forwardY*FROG_CUR_BALL_PAD,
				frog->forwardX,
				frog->forwardY,
				frog->color,
				speed
			);
			Animation_SetAngle(&bulletsArr->bullets[i].anim, frog->rotation);
			
			return;
		}
	}

	if (bulletsArr->len >= BULLETS_ARR_MAX_LEN) return;

	Bullet_Init(
		&bulletsArr->bullets[bulletsArr->len], 
		frog->x + frog->forwardX*FROG_CUR_BALL_PAD, 
		frog->y + frog->forwardY*FROG_CUR_BALL_PAD,
		frog->forwardX,
		frog->forwardY,
		frog->color,
		speed
	);
	Animation_SetAngle(&bulletsArr->bullets[bulletsArr->len].anim, frog->rotation);
	bulletsArr->len++;
}

char Bullet_isInsertInBack(Engine *engine, Bullet* bullet, Ball* ball, SpiralDot* spiral, int spiralLen) {
	float x1 = ball->x;
	float y1 = ball->y;
	float pos1 = ball->pos;

	float x2 = ball->x;
	float y2 = ball->y;
	float pos2 = ball->pos;

	if (spiralLen <= 0) return 0;

	while (pos1 - ball->pos < BALLS_CHAIN_PAD && (int)pos1 < spiralLen) {
		x1 += spiral[(int)pos1].dx * engine->scale_x * 0.5;
		y1 += spiral[(int)pos1].dy * engine->scale_y * 0.5;

		pos1 += 0.5 * 2;
	}

	while (ball->pos - pos2 < BALLS_CHAIN_PAD) {
		if (pos2 < 0) return 0;
		x2 -= spiral[(int)pos2].dx * engine->scale_x * 0.5;
		y2 -= spiral[(int)pos2].dy * engine->scale_y * 0.5;

		pos2 -= 0.5 * 2;
	}

	float distFront = (x1 - bullet->x) * (x1 - bullet->x);
	distFront += (y1 - bullet->y) * (y1 - bullet->y);

	float distBack = (x2 - bullet->x) * (x2 - bullet->x);
	distBack += (y2 - bullet->y) * (y2 - bullet->y);

	return distFront > distBack ? 1:0;
}

char Bullet_IsColliding(Bullet* bullet, Ball* ball) {
	float dist = (bullet->x - ball->x) * (bullet->x - ball->x);
	dist += (bullet->y - ball->y) * (bullet->y - ball->y);
	return dist < DISTANCE_TO_COLLIDE ? 1:0;
}

int Bullet_IsCollidingWithChain(Bullet* bullet, BallChain* ballChain) {
	for (int i = 0; i < ballChain->len; i++) {
		if (!ballChain->balls[i].isExploding && 
			!ballChain->balls[i].inTunnel &&
			Bullet_IsColliding(bullet, &ballChain->balls[i])
		) 
			return i;
	}
	return -1;
}

void BulletsArr_CollideWithChainUpdate(Engine *engine, BulletsArr* bulletsArr, BallChain* ballChain, SpiralDot* spiral, int spiralLen) {
	for (int i = 0; i < bulletsArr->len; i++) {
		if (!bulletsArr->bullets[i].onScreen) continue;

		int collBall = Bullet_IsCollidingWithChain(&bulletsArr->bullets[i], ballChain);

		if (collBall != -1) {
			bulletsArr->bullets[i].onScreen = 0;
			
			char isInsertBack = Bullet_isInsertInBack(
				engine,
				&bulletsArr->bullets[i], 
				&ballChain->balls[collBall], spiral, spiralLen);

			collBall = collBall + isInsertBack;

		    BallChain_Insert(engine, ballChain, collBall, 
				bulletsArr->bullets[i].color, isInsertBack);
		}
	}
}

void BulletsArr_Update(BulletsArr* bulletsArr) {
	for (int i = 0; i < bulletsArr->len; i++) {
		if (bulletsArr->bullets[i].onScreen)
			Bullet_Update(&bulletsArr->bullets[i]);
	}
}

void BulletsArr_Draw(Engine *engine, BulletsArr* bulletsArr) {
	for (int i = 0; i < bulletsArr->len; i++) {
		if (bulletsArr->bullets[i].onScreen)
			Bullet_Draw(engine, &bulletsArr->bullets[i]);
	}
}