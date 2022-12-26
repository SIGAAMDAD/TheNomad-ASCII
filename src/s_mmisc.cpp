#include "g_mob.h"
#include "g_game.h"
#include "scf.h"
#include "g_rng.h"
#include "g_obj.h"

void Game::M_GenMobs(void)
{
	m_Active.reserve(MAX_MOBS_ACTIVE);
	for (nomadushort_t y = 0; y < MAP_MAX_Y; y++) {
		for (nomadushort_t x = 0; x < MAP_MAX_X; x++) {
			if (m_Active.size() == MAX_MOBS_ACTIVE) {
				break;
			}
			nomaduint_t i = rand() % 255;
			if (i > 50) {
				nomaduint_t randmob = 0;
				mobj_t newmob;
				for (nomaduint_t f = 0; f < NUMMOBS; f++) {
					if (f == randmob) {
						newmob = mobinfo[f];
					}
				}
				coord_t mpos;
				mpos.y = (P_Random() & 400)+90;
				mpos.x = (P_Random() & 400)+90;
				m_Active.emplace_back();
				m_Active.back() = (Mob*)Z_Malloc(sizeof(Mob), TAG_STATIC, &m_Active.back());
				Mob* mob = m_Active.back();
				mob->mpos = mpos;
				mob->c_mob = newmob;
				mob->is_boss = false;
				mob->mstate = stateinfo[S_MOB_NULL];
				mob->mticker = mob->mstate.numticks;
			}
		}
	}
}

Mob::Mob()
{
	stepcounter &= 0;
}

Mob::~Mob()
{
}

nomadbool_t Mob::M_HearImmediate(const Game* map)
{
	if (map->mapbuffer[mpos.y - 1][mpos.x].snd_lvl >= c_mob.snd_tol
	|| map->mapbuffer[mpos.y][mpos.x - 1].snd_lvl >= c_mob.snd_tol
	|| map->mapbuffer[mpos.y + 1][mpos.x].snd_lvl >= c_mob.snd_tol
	|| map->mapbuffer[mpos.y][mpos.x + 1].snd_lvl >= c_mob.snd_tol
	|| map->mapbuffer[mpos.y - 1][mpos.y - 1].snd_lvl >= c_mob.snd_tol
	|| map->mapbuffer[mpos.y - 1][mpos.y + 1].snd_lvl >= c_mob.snd_tol
	|| map->mapbuffer[mpos.y + 1][mpos.y - 1].snd_lvl >= c_mob.snd_tol
	|| map->mapbuffer[mpos.y + 1][mpos.y + 1].snd_lvl >= c_mob.snd_tol) {
		return true;
	}
	else {
		return false;
	}
}

nomadbool_t Mob::M_HearPlayr(const Game* map)
{
	if (scf::launch::deafmobs) {
		return false;
	}
	else {
		if (!M_HearImmediate(map)) {
			bool found = false;
			for (nomadushort_t y = mpos.y;
			y < (mpos.y - (c_mob.snd_area >> 1)); y--) {
				for (nomadushort_t x = mpos.x;
				x < (mpos.x + (c_mob.snd_area >> 1)); x--) {
					if (map->mapbuffer[y][x].snd_lvl >= c_mob.snd_tol) {
						found = true;
					}
				}
			}
			return found;
		}
		else {
			return false;
		}
	}
}

nomadbool_t Mob::M_SeePlayr(const Game* map)
{
	if (scf::launch::blindmobs) {
		return false;
	}
	else {
		switch (mdir) {
		case D_NORTH: {
			for (nomadushort_t i = mpos.y; 
			i < (mpos.y - c_mob.sight_range); --i) {
			if (map->c_map[i][mpos.x] == map->playr->sprite) {
					return true;
				}
			}
			break; }
		case D_WEST: {
			for (nomadushort_t i = mpos.x;
			i < (mpos.x - c_mob.sight_range); --i) {
				if (map->c_map[mpos.y][i] == map->playr->sprite) {
					return true;
				}
			}
			break; }
		case D_SOUTH: {
			for (nomadushort_t i = mpos.y;
			i < (mpos.y + c_mob.sight_range); ++i) {
				if (map->c_map[i][mpos.x] == map->playr->sprite) {
					return true;
				}
			}
			break; }
		case D_EAST: {
			for (nomadushort_t i = mpos.x;
			i < (mpos.x + c_mob.sight_range); ++i) {
				if (map->c_map[mpos.y][i] == map->playr->sprite) {
					return true;
				}
			}
			break; }
		default:
			N_Error("Unknown/Invalid Direction For Mob: %s", c_mob.name);
			break;
		};
		return false;
	}
}

nomadbool_t Mob::M_SmellImmediate(const Game* map)
{
	if (map->mapbuffer[mpos.y - 1][mpos.x].smell_lvl >= c_mob.smell_tol
	|| map->mapbuffer[mpos.y][mpos.x - 1].smell_lvl >= c_mob.smell_tol
	|| map->mapbuffer[mpos.y + 1][mpos.x].smell_lvl >= c_mob.smell_tol
	|| map->mapbuffer[mpos.y][mpos.x + 1].smell_lvl >= c_mob.smell_tol
	|| map->mapbuffer[mpos.y - 1][mpos.y - 1].smell_lvl >= c_mob.smell_tol
	|| map->mapbuffer[mpos.y - 1][mpos.y + 1].smell_lvl >= c_mob.smell_tol
	|| map->mapbuffer[mpos.y + 1][mpos.y - 1].smell_lvl >= c_mob.smell_tol
	|| map->mapbuffer[mpos.y + 1][mpos.y + 1].smell_lvl >= c_mob.smell_tol) {
		return true;
	}
	else {
		return false;
	}
}

nomadbool_t Mob::M_SmellPlayr(const Game* map)
{
	if (scf::launch::nosmell) {
		return false;
	}
	else {
		if (!M_SmellImmediate(map)) {
			bool found = false;
			for (nomadushort_t y = mpos.y;
			y < (mpos.y - (c_mob.snd_area >> 1)); y--) {
				for (nomadushort_t x = mpos.x;
				x < (mpos.x + (c_mob.snd_area >> 1)); x--) {
					if (map->mapbuffer[y][x].smell_lvl >= c_mob.smell_tol) {
						found = true;
					}
				}
			}
			return found;
		}
		else {
			return false;
		}
	}
}

void Mob::M_SpawnThink(Game* const game)
{
	if (mticker) {
		return;
	}
	else {
		mstate = stateinfo[S_MOB_IDLE];
		mticker = mstate.numticks;
	}
}
void Mob::M_IdleThink(Game* const game)
{
	// if the mob smells and hears the playr OR sees the playr, they'll enter chase state
	if (mticker <= 0) {
		nomadenum_t pursuitcounter = idle;
		if (M_SmellPlayr(game)) {
			pursuitcounter |= smellplayr;
		}
		if (M_HearPlayr(game)) {
			pursuitcounter |= hearplayr;
		}
		if (M_SeePlayr(game)) {
			pursuitcounter |= seeplayr;
		}
		if (pursuitcounter < smellplayr) {
			return;
		}
		else if (pursuitcounter > smellplayr && pursuitcounter < seeplayr) {
			mstate = stateinfo[S_MOB_WANDER];
			mticker = mstate.numticks;
		}
		else if (pursuitcounter & seeplayr) {
			mstate = stateinfo[S_MOB_CHASEPLAYR];
			mticker = mstate.numticks;
		}
		else {
			return;
		}
	}
	else {
		return;
	}
}
void Mob::M_ChasePlayr(Game* const game)
{
	return;
}
void Mob::M_FightThink(Game* const game)
{
	return;
}
void Mob::M_FleeThink(Game* const game)
{
	return;
}
void Mob::M_WanderThink(Game* const game)
{
	if (!stepcounter) {
		stepcounter = P_Random() & 18; // get a cardinal number in the future
		
		// and now with a newly set step counter, we change the direction if rng decides it so
		if ((P_Random() & 100) < c_mob.rng) {
			mdir = P_Random() & 3; // might be the same direction
		}
	}
	else {
		stepcounter--;
		coord_t pos = game->E_GetDir(mdir);
		if (game->c_map[mpos.y+pos.y][mpos.x+pos.x] != '#') {
			game->E_MoveImmediate(&mpos, mdir);
		}
	}
	/*
	nomadenum_t pursuitcounter = idle;
	if (M_SmellPlayr(game)) {
		pursuitcounter |= smellplayr;
	}
	if (M_HearPlayr(game)) {
		pursuitcounter |= hearplayr;
	}
	if (M_SeePlayr(game)) {
		pursuitcounter |= seeplayr;
	}
	if (pursuitcounter < hearplayr) {
		mstate = stateinfo[S_MOB_IDLE];
		mticker = mstate.numticks;
}
	else if (pursuitcounter > smellplayr && pursuitcounter < seeplayr) {
		mticker = mstate.numticks;
	}
	else if (pursuitcounter & seeplayr) {
		mstate = stateinfo[S_MOB_CHASEPLAYR];
		mticker = mstate.id;
	}
	if (!game->E_Move(&mpos, &mdir)) {
		return;
	}
	*/
}
void Mob::M_DeadThink(Game* const game)
{
	return;
}

void Game::M_FollowPlayr(Mob* const mob, nomadbool_t smell, nomadbool_t hear,
	nomadbool_t see)
{
	return;
}