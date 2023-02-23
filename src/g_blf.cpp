#include "g_game.h"
//#undef byte
//#undef assert(x)
//#include "nlohmann/json.hpp"
#include "g_bff.h"

//using json = nlohmann::json;

#define BLF_ALLOC_SIZE (sizeof(blf_file_t*)*bfflvls.size())

void G_AllocBLFList(const std::list<blf_file_t*>& bfflvls)
{
	if (bfflvls.size() < 1)
		return;
	
	blf_file_t **lvl_list = (blf_file_t **)R_Malloc(BLF_ALLOC_SIZE, TAG_STATIC, &lvl_list);
	for (nomadulong_t i = 0; i < bfflvls.size(); ++i) {
		lvl_list[i] = (blf_file_t *)R_Malloc(sizeof(blf_t), TAG_STATIC, &lvl_list[i]);
		blf_file_t* lvl = bfflvls[i];
		blf_file_t *blf = lvl_list[i];
		blf->map_dim.height = lvl->map_height;
		blf->map_dim.width = lvl_>map_width;
		blf->lvl_map = (char **)R_Malloc(sizeof(char *) * (lvl->map_height+2), TAG_STATIC, &blf->lvl_map);
		for (nomadshort_t y = 0; y < lvl->map_height + 2; ++y) {
			blf->lvl_map[y] = (char *)R_Malloc(sizeof(char) * (lvl->map_width+2), TAG_STATIC, &blf->lvl_map[y]);
		}
		memcpy(&(*blf), &(*lvl), sizeof(blf_file_t));
		Z_Free(lvl->lvl_map);
		Z_Free(lvl);
	}
}

blf_file_t* G_LoadBLF(const std::string& name, blf_file_t* old)
{
    blf_file_t* blf = (blf_file_t *)Z_Malloc(sizeof(blf_file_t), TAG_STATIC, &blf);
	memmove(&(*blf), &(*old), sizeof(blf_file_t));
	R_Free(old);
}

void G_FreeBLF(blf_file_t *blf)
{
	// put it back into the reserved heap
	blf_file_t* reserve = (blf_file_t *)R_Malloc(sizeof(blf_file_t), TAG_STATIC, &reserve);
   	memcpy(&(*reserve), &(*blf), sizeof(blf_t));
	for (nomaduint_t i = 0; i < blf->map_dim.height; ++i) {
        Z_Free(blf->lvl_map[i]);
    }
    Z_Free(blf->lvl_map);
    Z_Free(blf);
}