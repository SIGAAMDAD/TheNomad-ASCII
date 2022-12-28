#include "g_mob.h"
#include "g_game.h"
#include "scf.h"
#include "g_rng.h"
#include "g_obj.h"

static Game* game;

void Game::M_GenMobs(void)
{
	game = this;
	m_Active.reserve(MAX_MOBS_ACTIVE);
	for (nomadushort_t y = 0; y < MAP_MAX_Y; ++y) {
		for (nomadushort_t x = 0; x < MAP_MAX_X; ++x) {
			if (m_Active.size() == MAX_MOBS_ACTIVE) {
				break;
			}
			srand(time(NULL));
			nomaduint_t i = rand() % 100;
			mobj_t newmob = mobinfo[(rand() % NUMMOBS)];
			while (i > newmob.chance_to_spawn) {
				newmob = mobinfo[(rand() % NUMMOBS)];
			};
			coord_t mpos;
			mpos.y = (P_Random() & 400)+90;
			mpos.x = (P_Random() & 400)+90;
			m_Active.emplace_back();
			m_Active.back() = (Mob*)Z_Malloc(sizeof(Mob), TAG_STATIC, &m_Active.back());
			Mob* mob = m_Active.back();
			mob->mpos = mpos;
			mob->c_mob = newmob;
			pthread_mutex_init(&mob->mutex, NULL);
			mob->is_boss = false;
			mob->mstate = stateinfo[S_MOB_WANDER];
			mob->mticker = mob->mstate.numticks;
			mob->stepcounter &= 0;
			mob->mdir = P_Random() & 3;
		}
	}
}

Mob::Mob()
{
}

Mob::~Mob()
{
	pthread_mutex_destroy(&mutex);
}

nomadbool_t Mob::M_HearImmediate()
{
	Game* const map = game;
	if (map->sndmap[mpos.y - 1][mpos.x] >= c_mob.snd_tol
	|| map->sndmap[mpos.y][mpos.x - 1] >= c_mob.snd_tol
	|| map->sndmap[mpos.y + 1][mpos.x] >= c_mob.snd_tol
	|| map->sndmap[mpos.y][mpos.x + 1] >= c_mob.snd_tol
	|| map->sndmap[mpos.y - 1][mpos.y - 1] >= c_mob.snd_tol
	|| map->sndmap[mpos.y - 1][mpos.y + 1] >= c_mob.snd_tol
	|| map->sndmap[mpos.y + 1][mpos.y - 1] >= c_mob.snd_tol
	|| map->sndmap[mpos.y + 1][mpos.y + 1] >= c_mob.snd_tol) {
		return true;
	}
	else {
		return false;
	}
}

nomadbool_t Mob::M_HearPlayr()
{
	Game* const map = game;
	if (scf::launch::deafmobs) {
		return false;
	}
	else {
		if (M_HearImmediate()) {
			return true;
		}
		else {
			bool found = false;
			coord_t area;
			area.y = mpos.y - (c_mob.snd_area >> 1);
			area.x = mpos.x - (c_mob.snd_area >> 1);
			for (nomadushort_t y = mpos.y; y < area.y; ++y) {
				for (nomadushort_t x = mpos.x; x < area.x; ++x) {
					if (map->sndmap[y][x] >= c_mob.snd_tol) {
						found = true;
					}
				}
			}
			return found;
		}
	}
}

nomadbool_t Mob::M_SeePlayr()
{
	Game* const map = game;
	if (scf::launch::blindmobs) {
		return false;
	}
	else {
		nomadbool_t found = false;
		switch (mdir) {
		case D_NORTH: {
			for (nomadushort_t i = mpos.y; 
			i < (mpos.y - c_mob.sight_range); --i) {
			if (map->c_map[i][mpos.x] == map->playr->sprite) {
					found = true;
				}
			}
			break; }
		case D_WEST: {
			for (nomadushort_t i = mpos.x;
			i < (mpos.x - c_mob.sight_range); --i) {
				if (map->c_map[mpos.y][i] == map->playr->sprite) {
					found = true;
				}
			}
			break; }
		case D_SOUTH: {
			for (nomadushort_t i = mpos.y;
			i < (mpos.y + c_mob.sight_range); ++i) {
				if (map->c_map[i][mpos.x] == map->playr->sprite) {
					found = true;
				}
			}
			break; }
		case D_EAST: {
			for (nomadushort_t i = mpos.x;
			i < (mpos.x + c_mob.sight_range); ++i) {
				if (map->c_map[mpos.y][i] == map->playr->sprite) {
					found = true;
				}
			}
			break; }
		default:
			N_Error("Unknown/Invalid Direction For Mob: %s", c_mob.name);
			break;
		};
		return found;
	}
}

nomadbool_t Mob::M_SmellImmediate()
{
	Game* const map = game;
	if (map->smellmap[mpos.y - 1][mpos.x] >= c_mob.smell_tol
	|| map->smellmap[mpos.y][mpos.x - 1] >= c_mob.smell_tol
	|| map->smellmap[mpos.y + 1][mpos.x] >= c_mob.smell_tol
	|| map->smellmap[mpos.y][mpos.x + 1] >= c_mob.smell_tol
	|| map->smellmap[mpos.y - 1][mpos.y - 1] >= c_mob.smell_tol
	|| map->smellmap[mpos.y - 1][mpos.y + 1] >= c_mob.smell_tol
	|| map->smellmap[mpos.y + 1][mpos.y - 1] >= c_mob.smell_tol
	|| map->smellmap[mpos.y + 1][mpos.y + 1] >= c_mob.smell_tol) {
		return true;
	}
	else {
		return false;
	}
}

nomadbool_t Mob::M_SmellPlayr()
{
	Game* const map = game;
	if (scf::launch::nosmell) {
		return false;
	}
	else {
		if (M_SmellImmediate()) {
			return true;
		}
		else {
			bool found = false;
			for (nomadushort_t y = mpos.y;
			y < (mpos.y - (c_mob.snd_area >> 1)); ++y) {
				for (nomadushort_t x = mpos.x;
				x < (mpos.x + (c_mob.snd_area >> 1)); ++x) {
					if (map->smellmap[y][x] >= c_mob.smell_tol) {
						found = true;
					}
				}
			}
			return found;
		}
	}
}

void Mob::M_SpawnThink()
{
	if (!mticker) {
		mstate = stateinfo[S_MOB_WANDER];
		mticker = mstate.numticks;
	}
}

void Mob::M_ChasePlayr()
{
	return;
}
void Mob::M_FightThink()
{
	return;
}
void Mob::M_FleeThink()
{
	return;
}

void Mob::M_WanderThink()
{
	if (!stepcounter) {
		stepcounter = P_Random() & 10; // get a cardinal number in the future
	
		// and now with a newly set step counter, we change the direction if rng decides it so
		if ((P_Random() & 100) < c_mob.rng) {
			mdir = P_Random() & 3; // might be the same direction
		}
	}
	else {
		stepcounter--;
		coord_t pos = game->E_GetDir(mdir);
		char move = game->c_map[mpos.y+pos.y][mpos.x+pos.x];
		switch (move) {
		case '.':
		case ' ':
			game->E_MoveImmediate(&mpos, mdir);
			break;
		default:
			mdir = P_Random() & 3;
			break;
		};
	}
	if (M_SeePlayr()) {
		M_FollowPlayr(this, game);
	}
}
void Mob::M_DeadThink()
{
	return;
}