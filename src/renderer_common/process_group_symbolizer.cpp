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

#include <mapnik/renderer_common/process_group_symbolizer.hpp>

namespace mapnik {

common_point_render_thunk::common_point_render_thunk(pixel_position const &pos, marker const &m,
                                                     agg::trans_affine const &tr, double opacity)
    : pos_(pos), marker_(std::make_shared<marker>(m)),
      tr_(tr), opacity_(opacity)
{}

common_text_render_thunk::common_text_render_thunk(placements_list const &placements)
    : placements_(), glyphs_(std::make_shared<std::vector<glyph_info> >())
{
    std::vector<glyph_info> &glyph_vec = *glyphs_;
    
    size_t glyph_count = 0;
    for (glyph_positions_ptr positions : placements)
    {
        glyph_count += std::distance(positions->begin(), positions->end());
    }
    glyph_vec.reserve(glyph_count);
    
    for (glyph_positions_ptr positions : placements)
    {
        glyph_positions_ptr new_positions = std::make_shared<glyph_positions>();
        new_positions->reserve(std::distance(positions->begin(), positions->end()));
        glyph_positions &new_pos = *new_positions;
        
        new_pos.set_base_point(positions->get_base_point());
        if (positions->marker())
        {
            new_pos.set_marker(positions->marker(), positions->marker_pos());
        }
        
        for (glyph_position const &pos : *positions)
        {
            glyph_vec.push_back(*pos.glyph);
            new_pos.push_back(glyph_vec.back(), pos.pos, pos.rot);
        }
        
        placements_.push_back(new_positions);
    }
}

geometry_type *origin_point(proj_transform const &prj_trans,
                            renderer_common const &common)
{
    // note that we choose a point in the middle of the screen to
    // try to ensure that we don't get edge artefacts due to any
    // symbolizers with avoid-edges set: only the avoid-edges of
    // the group symbolizer itself should matter.
    double x = common.width_ / 2.0, y = common.height_ / 2.0, z = 0.0;
    common.t_.backward(&x, &y);
    prj_trans.forward(x, y, z);
    geometry_type *geom = new geometry_type(geometry_type::Point);
    geom->move_to(x, y);
    return geom;
}

} // namespace mapnik
