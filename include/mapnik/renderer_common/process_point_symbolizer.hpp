/*****************************************************************************
 *
 * This file is part of Mapnik (c++ mapping toolkit)
 *
 * Copyright (C) 2013 Artem Pavlenko
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

#ifndef MAPNIK_RENDERER_COMMON_PROCESS_POINT_SYMBOLIZER_HPP
#define MAPNIK_RENDERER_COMMON_PROCESS_POINT_SYMBOLIZER_HPP

#include <mapnik/text/symbolizer_helpers.hpp>

namespace mapnik {

template <typename F>
void render_point_symbolizer(point_symbolizer const &sym,
                             mapnik::feature_impl &feature,
                             proj_transform const &prj_trans,
                             renderer_common &common,
                             F render_marker)
{
    text_symbolizer_helper helper(
        sym, feature, prj_trans,
        common.width_, common.height_,
        common.scale_factor_,
        common.t_, common.font_manager_, *common.detector_,
        common.query_extent_);
    
    double opacity = get<double>(sym,keys::opacity,feature, 1.0);

    placements_list const& placements = helper.get();
    for (glyph_positions_ptr glyphs : placements)
    {
        if (glyphs->marker())
        {
            render_marker(glyphs->marker_pos(),
                          *(glyphs->marker()->marker),
                          glyphs->marker()->transform,
                          opacity);
        }
    }
}

} // namespace mapnik

#endif /* MAPNIK_RENDERER_COMMON_PROCESS_POINT_SYMBOLIZER_HPP */
