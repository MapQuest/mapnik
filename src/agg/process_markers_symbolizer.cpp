/*****************************************************************************
 *
 * This file is part of Mapnik (c++ mapping toolkit)
 *
 * Copyright (C) 2011 Artem Pavlenko
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *****************************************************************************/

// mapnik
#include <mapnik/graphics.hpp>
#include <mapnik/agg_renderer.hpp>
#include <mapnik/agg_rasterizer.hpp>

#include <mapnik/debug.hpp>
#include <mapnik/feature.hpp>
#include <mapnik/geom_util.hpp>
#include <mapnik/vertex_converters.hpp>
#include <mapnik/marker_helpers.hpp>
#include <mapnik/marker.hpp>
#include <mapnik/marker_cache.hpp>
#include <mapnik/svg/svg_renderer_agg.hpp>
#include <mapnik/svg/svg_storage.hpp>
#include <mapnik/svg/svg_path_adapter.hpp>
#include <mapnik/svg/svg_path_attributes.hpp>
#include <mapnik/symbolizer.hpp>
#include <mapnik/parse_path.hpp>
#include <mapnik/renderer_common/process_markers_symbolizer.hpp>
#include <mapnik/text/symbolizer_helpers.hpp>
#include <mapnik/text/renderer.hpp>

// agg
#include "agg_basics.h"
#include "agg_renderer_base.h"
#include "agg_renderer_scanline.h"
#include "agg_rendering_buffer.h"
#include "agg_pixfmt_rgba.h"
#include "agg_color_rgba.h"
#include "agg_rasterizer_scanline_aa.h"
#include "agg_scanline_u.h"
#include "agg_path_storage.h"
#include "agg_conv_clip_polyline.h"
#include "agg_conv_transform.h"


// boost
#include <boost/optional.hpp>

namespace mapnik {

template <typename T0, typename T1>
void agg_renderer<T0,T1>::process(markers_symbolizer const& sym,
                              feature_impl & feature,
                              proj_transform const& prj_trans)
{
    box2d<double> clip_box = clipping_extent();
    text_symbolizer_helper helper(
            sym, feature, prj_trans,
            common_.width_, common_.height_,
            common_.scale_factor_,
            common_.t_, common_.font_manager_, *common_.detector_,
            clip_box);

    composite_mode_e comp_op = get<composite_mode_e>(sym, keys::comp_op, feature, src_over);
    double opacity = get<double>(sym,keys::opacity,feature, 1.0);

    placements_list const& placements = helper.get();
    for (glyph_positions_ptr glyphs : placements)
    {
        if (glyphs->marker())
        {
            render_marker(glyphs->marker_pos(),
                          *(glyphs->marker()->marker),
                          glyphs->marker()->transform,
                          opacity, comp_op);
        }
    }
}

template void agg_renderer<image_32>::process(markers_symbolizer const&,
                                              mapnik::feature_impl &,
                                              proj_transform const&);
}
