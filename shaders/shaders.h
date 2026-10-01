#pragma once
/* Every GCN shader binary. The headers are generated from the .s sources by the header tool
   (hash, PGM_RSRC1 in each header's comment); ps_blue, ps_shadow, ps_shadow_clear, vs_shader and
   vs_shadow have no .s: hand-assembled. */
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
#include "ps_ui.h"
#include "ps_cvt_a.h"
#include "ps_cvt_b.h"
#include "ps_cvt_full_a.h"
#include "ps_cvt_full_b.h"
#include "ps_clock.h"
#include "ps_clock_light.h"
#include "ps_resolve.h"
#include "ps_shader.h"
#include "ps_shadow.h"
#include "ps_shadow_clear.h"
#include "ps_stars.h"
#include "vs_shader.h"
#include "vs_shadow.h"
#include "vs_model.h"
#include "vs_model_shadow.h"
