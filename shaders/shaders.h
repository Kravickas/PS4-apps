#pragma once
/* GPU shader binaries (GCN2 / Sea Islands). The .s files next to the post,
   ps_stars and ps_dark shaders are their llvm-mc sources (see README.txt). */
#include "ps_blue.h"
#include "ps_dark.h"
#include "ps_floor.h"
#include "ps_model.h"
#include "ps_post_blur.h"
#include "ps_post_comp.h"
#include "ps_post_down.h"
#include "ps_post_final.h"
#include "ps_post_final_v0.h" /* bisect: bc-tiled final pass (CAFE0213) */
#include "ps_post_final_v2.h" /* bisect: flare-photo final pass (CAFE0217) */
#include "ps_post_final_b1.h" /* bisect: bc-tiled + flare loads (CAFE0220) */
#include "ps_post_final_b2.h" /* bisect: flare-photo, block skipped unconditionally (CAFE0221) */
#include "ps_post_final_b3.h" /* bisect: flare-photo without the glare sample (CAFE0222) */
#include "ps_shader.h"
#include "ps_shadow.h"
#include "ps_shadow_clear.h"
#include "ps_stars.h"
#include "vs_shader.h"
#include "vs_shadow.h"
#include "vs_model.h"
#include "vs_model_shadow.h"
